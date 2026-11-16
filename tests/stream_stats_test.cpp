#include <gtest/gtest.h>

#include "net/stream_stats.hpp"

using std::chrono::microseconds;

TEST(StreamStats, CountsGapsAsLost) {
    net::StreamStats stats;
    for (std::uint32_t tick : {1u, 2u, 5u, 6u}) {  // 3 and 4 never arrive
        stats.on_state(tick, microseconds{50});
    }
    EXPECT_EQ(stats.received(), 4u);
    EXPECT_EQ(stats.lost(), 2u);
    EXPECT_EQ(stats.out_of_order(), 0u);
}

TEST(StreamStats, ALateDatagramIsOutOfOrderNotLost) {
    net::StreamStats stats;
    for (std::uint32_t tick : {1u, 3u, 2u, 4u}) {  // 2 overtaken by 3
        stats.on_state(tick, microseconds{50});
    }
    EXPECT_EQ(stats.lost(), 0u);
    EXPECT_EQ(stats.out_of_order(), 1u);
}

TEST(StreamStats, ANewMatchRestartsTickNumbers) {
    net::StreamStats stats;
    for (std::uint32_t tick : {1u, 2u, 3u, 1u, 2u}) {
        stats.on_state(tick, microseconds{50});
    }
    EXPECT_EQ(stats.lost(), 0u);
    EXPECT_EQ(stats.out_of_order(), 0u);
}

TEST(StreamStats, StartingMidMatchIsNotLoss) {
    net::StreamStats stats;
    stats.on_state(500, microseconds{10});  // joined late: ticks 1-499 were never ours
    stats.on_state(501, microseconds{30});
    EXPECT_EQ(stats.lost(), 0u);
    EXPECT_EQ(stats.latency().mean(), microseconds{20});
}
