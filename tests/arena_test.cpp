#include <gtest/gtest.h>

#include <chrono>

#include "sim/arena.hpp"
#include "sim/loop.hpp"

using sim::Arena;
using sim::Robot;
using sim::Seconds;
using sim::Vec2;

namespace {

// Two robots of radius 0.1 m in a 1 m ring, placed by the caller.
Arena two_robots(Vec2 pos_a, Vec2 vel_a, Vec2 pos_b, Vec2 vel_b) {
    Arena arena;
    arena.robots.push_back(Robot{pos_a, vel_a});
    arena.robots.push_back(Robot{pos_b, vel_b});
    return arena;
}

}  // namespace

TEST(Vec2, DotProduct) {
    EXPECT_DOUBLE_EQ((Vec2{1.0, 2.0}).dot(Vec2{3.0, 4.0}), 11.0);
}

TEST(Arena, StepMovesEveryRobot) {
    Arena arena = two_robots({-0.5, 0.0}, {1.0, 0.0}, {0.5, 0.0}, {0.0, 1.0});
    sim::step(arena, Seconds{0.1});
    EXPECT_DOUBLE_EQ(arena.robots[0].position.x, -0.4);
    EXPECT_DOUBLE_EQ(arena.robots[1].position.y, 0.1);
}

TEST(Arena, OverlappingRobotsArePushedApartUntilTouching) {
    // Centres 0.1 m apart, but two radii need 0.2 m. Each should move 0.05 m outwards.
    Arena arena = two_robots({-0.05, 0.0}, {}, {0.05, 0.0}, {});
    sim::step(arena, Seconds{0.0});
    EXPECT_DOUBLE_EQ(arena.robots[0].position.x, -0.1);
    EXPECT_DOUBLE_EQ(arena.robots[1].position.x, 0.1);
}

TEST(Arena, HeadOnCollisionSwapsVelocities) {
    // Equal masses, elastic: the moving robot stops and the one it hits moves off.
    Arena arena = two_robots({-0.05, 0.0}, {1.0, 0.0}, {0.05, 0.0}, {0.0, 0.0});
    sim::step(arena, Seconds{0.0});
    EXPECT_EQ(arena.robots[0].velocity, (Vec2{0.0, 0.0}));
    EXPECT_EQ(arena.robots[1].velocity, (Vec2{1.0, 0.0}));
}

TEST(Arena, OverlappingButSeparatingRobotsKeepTheirVelocities) {
    Arena arena = two_robots({-0.05, 0.0}, {-1.0, 0.0}, {0.05, 0.0}, {1.0, 0.0});
    sim::step(arena, Seconds{0.0});
    EXPECT_EQ(arena.robots[0].velocity, (Vec2{-1.0, 0.0}));
    EXPECT_EQ(arena.robots[1].velocity, (Vec2{1.0, 0.0}));
}

TEST(Arena, RobotWhoseCentreLeavesTheRingIsOut) {
    Arena arena = two_robots({0.95, 0.0}, {1.0, 0.0}, {-0.5, 0.0}, {});
    sim::step(arena, Seconds{0.1});  // centre reaches x = 1.05, past the 1 m edge
    EXPECT_FALSE(arena.robots[0].in_ring);
    EXPECT_TRUE(arena.robots[1].in_ring);
}

TEST(Arena, RobotsThatAreOutStopMoving) {
    Arena arena = two_robots({2.0, 0.0}, {1.0, 0.0}, {-0.5, 0.0}, {});
    arena.robots[0].in_ring = false;
    sim::step(arena, Seconds{1.0});
    EXPECT_EQ(arena.robots[0].position, (Vec2{2.0, 0.0}));
}

TEST(Arena, WinnerIsTheLastRobotInTheRing) {
    Arena arena = two_robots({0.0, 0.5}, {}, {0.0, -0.5}, {});
    EXPECT_FALSE(sim::match_over(arena));
    EXPECT_FALSE(sim::winner(arena).has_value());

    arena.robots[0].in_ring = false;
    EXPECT_TRUE(sim::match_over(arena));
    EXPECT_EQ(sim::winner(arena), 1u);
}

TEST(Arena, BothOutIsADraw) {
    Arena arena = two_robots({0.0, 0.5}, {}, {0.0, -0.5}, {});
    arena.robots[0].in_ring = false;
    arena.robots[1].in_ring = false;
    EXPECT_TRUE(sim::match_over(arena));
    EXPECT_FALSE(sim::winner(arena).has_value());
}

TEST(Loop, HeadlessRunStopsWhenTheMatchIsOver) {
    // Robot 0 drives right at 1 m/s from x = 0.5. A 0.125 s step is exact in binary, so the
    // positions are exactly 0.625, 0.75, 0.875, 1.0 (on the edge, still in), then 1.125 (out).
    Arena arena = two_robots({0.5, 0.0}, {1.0, 0.0}, {-0.5, 0.0}, {});
    const int ticks = sim::run_headless(arena, Seconds{0.125}, 100);
    EXPECT_EQ(ticks, 5);
    EXPECT_EQ(sim::winner(arena), 1u);
}

TEST(Loop, HeadlessRunStopsAtTheTickLimit) {
    Arena arena = two_robots({0.0, 0.5}, {}, {0.0, -0.5}, {});
    EXPECT_EQ(sim::run_headless(arena, Seconds{0.1}, 7), 7);
}

TEST(Loop, RealTimeRunTakesAtLeastTicksTimesStep) {
    using std::chrono::milliseconds;
    using std::chrono::steady_clock;
    Arena arena = two_robots({0.0, 0.5}, {}, {0.0, -0.5}, {});

    const auto start = steady_clock::now();
    const sim::RunStats stats = sim::run_realtime(arena, milliseconds{2}, 5);
    const auto elapsed = steady_clock::now() - start;

    EXPECT_EQ(stats.ticks, 5);
    // sleep_until never wakes early, so this lower bound is safe even on a slow CI machine.
    // There is no upper bound check: a busy machine can always be late.
    EXPECT_GE(elapsed, milliseconds{10});
}
