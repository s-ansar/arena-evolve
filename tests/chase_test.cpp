#include <gtest/gtest.h>

#include "ai/chase.hpp"

namespace {

net::State two_robots(net::RobotState a, net::RobotState b) {
    net::State state;
    state.robots = {a, b};
    return state;
}

}  // namespace

TEST(Chase, DrivesAtFullThrustTowardsTheOpponent) {
    const net::State state = two_robots({0.0f, 0.0f}, {0.0f, 0.5f});
    EXPECT_EQ(ai::chase(state, 0), (net::Command{0.0f, 1.0f}));
    EXPECT_EQ(ai::chase(state, 1), (net::Command{0.0f, -1.0f}));
}

TEST(Chase, IgnoresOpponentsThatAreOut) {
    net::RobotState out{0.3f, 0.0f};
    out.in_ring = false;
    const net::State state = two_robots({0.0f, 0.0f}, out);
    EXPECT_EQ(ai::chase(state, 0), (net::Command{}));
}

TEST(Chase, StopsWhenItIsOutItself) {
    net::RobotState me{0.0f, 0.0f};
    me.in_ring = false;
    EXPECT_EQ(ai::chase(two_robots(me, {0.5f, 0.0f}), 0), (net::Command{}));
}
