#include <gtest/gtest.h>

#include <atomic>
#include <optional>
#include <thread>

#include "ai/chase.hpp"
#include "net/client.hpp"
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
    std::thread chaser([&] { chaser_id = net::play_match("127.0.0.1", server.port(), ai::chase); });
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
    EXPECT_EQ(result->stats.lateness.count(), static_cast<std::size_t>(result->stats.ticks));

    // UDP may drop packets, so do not demand every tick. Locally nearly all should arrive.
    EXPECT_GT(watched.received(), static_cast<std::uint64_t>(result->stats.ticks / 2));
}

TEST(Server, ConnectingToAClosedPortFails) {
    std::uint16_t unused_port = 0;
    {
        net::Server server(0);
        unused_port = server.port();
    }  // server destroyed: nothing listens on that port any more
    EXPECT_THROW(net::play_match("127.0.0.1", unused_port, {}), std::system_error);
}
