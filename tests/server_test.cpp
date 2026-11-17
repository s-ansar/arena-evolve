#include <gtest/gtest.h>

#include <atomic>
#include <optional>
#include <thread>

#include "ai/chase.hpp"
#include "net/client.hpp"
#include "net/http_server.hpp"
#include "net/server.hpp"
#include "net/stream_stats.hpp"

// End to end on this machine: a server thread, a chasing client, a passive client (both
// over TCP) and a watcher (over UDP). This exercises sockets, the protocol, the per-client
// reader and writer threads, the command queue, the mailbox and the UDP broadcast together,
// under each sanitizer in CI.
TEST(Server, ChaserPushesAPassiveRobotOutOfTheRing) {
    net::Server server(0);  // port 0: the OS picks a free one

    // The watcher registers before the match. Its Watch datagram waits in the server's UDP
    // receive buffer until the first tick reads it.
    const net::Socket watcher = net::open_udp(0);
    net::set_receive_timeout(watcher, std::chrono::milliseconds{100});
    net::send_datagram(watcher, net::resolve_udp("127.0.0.1", server.port()), net::encode(net::Watch{}));

    std::optional<net::MatchResult> result;
    std::thread server_thread([&] {
        // 10 ms ticks; the push takes a few seconds of simulated (and real) time.
        result = server.run_match(net::starting_arena(), sim::Seconds{0.01}, 1000);
    });

    // The server is already listening, so these connect even before it calls accept().
    int chaser_id = -1;
    bool chaser_saw_the_end = false;
    std::thread chaser([&] {
        const net::MatchReport report = net::play_match("127.0.0.1", server.port(), ai::chase);
        chaser_id = report.robot;
        chaser_saw_the_end = report.finished;
    });
    std::thread passive([&] { net::play_match("127.0.0.1", server.port(), {}); });

    // Count State datagrams until the match is over. match_done is atomic because this
    // thread reads it while the main thread writes it.
    std::atomic<bool> match_done = false;
    net::StreamStats watched;
    std::thread watch_thread([&] {
        while (!match_done) {
            std::optional<net::Datagram> datagram = net::receive_datagram(watcher, true);
            if (!datagram) {
                continue;  // timed out; check match_done again
            }
            const std::optional<net::Message> message = net::try_decode(datagram->bytes);
            if (const net::State* state = message ? std::get_if<net::State>(&*message) : nullptr) {
                watched.on_state(state->tick, std::chrono::nanoseconds{0});
            }
        }
    });

    server_thread.join();
    chaser.join();
    passive.join();
    match_done = true;
    watch_thread.join();

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->winner.has_value()) << "match ended in a draw or timed out";
    EXPECT_EQ(static_cast<int>(*result->winner), chaser_id);
    EXPECT_TRUE(chaser_saw_the_end);  // the client got the final, decided state
    EXPECT_EQ(result->stats.lateness.count(), static_cast<std::size_t>(result->stats.ticks));

    // UDP may drop packets, so do not demand every tick. Locally nearly all should arrive.
    EXPECT_GT(watched.received(), static_cast<std::uint64_t>(result->stats.ticks / 2));

    // The monitoring counters agree with the match.
    const net::Metrics& metrics = server.metrics();
    EXPECT_EQ(metrics.ticks.load(), static_cast<std::uint64_t>(result->stats.ticks));
    EXPECT_EQ(metrics.matches_started.load(), 1u);
    EXPECT_EQ(metrics.matches_won.load(), 1u);
    EXPECT_EQ(metrics.matches_drawn.load(), 0u);
    EXPECT_FALSE(metrics.match_running.load());
    EXPECT_GT(metrics.last_interval_mean_seconds.load(), 0.0);
}

namespace {

// Send one raw HTTP request and return everything the server replies before closing.
std::string http_get(std::uint16_t port, const std::string& path) {
    const net::Socket socket = net::connect_tcp("127.0.0.1", port);
    const std::string request = "GET " + path + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
    net::send_all(socket, std::span(reinterpret_cast<const std::uint8_t*>(request.data()), request.size()));
    std::vector<std::uint8_t> reply;
    while (net::receive_some(socket, reply)) {
    }
    return {reply.begin(), reply.end()};
}

}  // namespace

TEST(HttpServer, ServesHealthAndMetricsOverRealTcp) {
    net::Metrics metrics;
    metrics.ticks = 42;
    const net::HttpServer http(0, metrics);

    const std::string health = http_get(http.port(), "/health");
    EXPECT_EQ(health.rfind("HTTP/1.1 200 OK", 0), 0u);

    const std::string scraped = http_get(http.port(), "/metrics");
    EXPECT_NE(scraped.find("arena_ticks_total 42\n"), std::string::npos);

    EXPECT_EQ(http_get(http.port(), "/missing").rfind("HTTP/1.1 404", 0), 0u);
}  // destructor stops the HTTP thread; TSan checks the shutdown is race-free

TEST(Server, ConnectingToAClosedPortFails) {
    std::uint16_t unused_port = 0;
    {
        net::Server server(0);
        unused_port = server.port();
    }  // server destroyed: nothing listens on that port any more
    EXPECT_THROW(net::play_match("127.0.0.1", unused_port, {}), std::system_error);
}

// ---- graceful drain (what makes a Kubernetes rolling update safe) ----

TEST(Server, StopWhileWaitingForPlayersStartsNoMatch) {
    net::Server server(0);
    std::optional<net::MatchResult> result = net::MatchResult{};  // must become nullopt
    std::thread server_thread([&] { result = server.run_match(net::starting_arena(), sim::Seconds{0.01}, 100); });

    // One player connects; the second never comes. Then a stop is requested.
    std::thread lonely([&] {
        // No Welcome is ever sent (the match never started), so play_match throws.
        EXPECT_THROW(net::play_match("127.0.0.1", server.port(), {}), net::ProtocolError);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds{200});
    server.request_stop();

    server_thread.join();
    lonely.join();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(server.metrics().matches_started.load(), 0u);
    EXPECT_TRUE(server.stopping());
}

TEST(Server, StopDuringAMatchLetsItFinish) {
    net::Server server(0);
    std::optional<net::MatchResult> result;
    std::thread server_thread([&] { result = server.run_match(net::starting_arena(), sim::Seconds{0.01}, 1000); });

    net::MatchReport chaser_report;
    net::MatchReport passive_report;
    std::thread chaser([&] { chaser_report = net::play_match("127.0.0.1", server.port(), ai::chase); });
    std::thread passive([&] { passive_report = net::play_match("127.0.0.1", server.port(), {}); });

    // Wait until the match is really running, then ask the server to stop.
    while (!server.metrics().match_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    server.request_stop();
    EXPECT_EQ(net::http_response("GET /ready HTTP/1.1\r\n\r\n", server.metrics()).rfind("HTTP/1.1 503", 0), 0u);

    server_thread.join();
    chaser.join();
    passive.join();

    // The match ran to its natural end: a winner, and both clients saw the final state.
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->winner.has_value());
    EXPECT_TRUE(chaser_report.finished);
    EXPECT_TRUE(passive_report.finished);
    EXPECT_EQ(server.metrics().matches_started.load(), 1u);
    EXPECT_EQ(server.metrics().matches_won.load(), 1u);

    // And no new match starts after the stop.
    EXPECT_FALSE(server.run_match(net::starting_arena(), sim::Seconds{0.01}, 10).has_value());
}
