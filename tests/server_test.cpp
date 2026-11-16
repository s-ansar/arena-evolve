#include <gtest/gtest.h>

#include <optional>
#include <thread>

#include "ai/chase.hpp"
#include "net/client.hpp"
#include "net/server.hpp"

// End to end over real TCP on this machine: a server thread, a chasing client thread and a
// passive client thread. This exercises sockets, the protocol, the per-client reader and
// writer threads, the command queue and the mailbox together, under each sanitizer in CI.
TEST(Server, ChaserPushesAPassiveRobotOutOfTheRing) {
    net::Server server(0);  // port 0: the OS picks a free one

    std::optional<std::size_t> winner;
    std::thread server_thread([&] {
        // 10 ms ticks; the push takes a few seconds of simulated (and real) time.
        winner = server.run_match(net::starting_arena(), sim::Seconds{0.01}, 1000);
    });

    // The server is already listening, so these connect even before it calls accept().
    int chaser_id = -1;
    std::thread chaser([&] { chaser_id = net::play_match("127.0.0.1", server.port(), ai::chase); });
    std::thread passive([&] { net::play_match("127.0.0.1", server.port(), {}); });

    server_thread.join();
    chaser.join();
    passive.join();

    ASSERT_TRUE(winner.has_value()) << "match ended in a draw or timed out";
    EXPECT_EQ(static_cast<int>(*winner), chaser_id);
}

TEST(Server, ConnectingToAClosedPortFails) {
    std::uint16_t unused_port = 0;
    {
        net::Server server(0);
        unused_port = server.port();
    }  // server destroyed: nothing listens on that port any more
    EXPECT_THROW(net::play_match("127.0.0.1", unused_port, {}), std::system_error);
}
