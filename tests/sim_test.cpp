#include <gtest/gtest.h>

#include "sim/robot.hpp"
#include "sim/vec2.hpp"

using sim::Robot;
using sim::Vec2;

TEST(Vec2, AddsAndSubtracts) {
    const Vec2 a{1.0, 2.0};
    const Vec2 b{3.0, 4.0};
    EXPECT_EQ(a + b, (Vec2{4.0, 6.0}));
    EXPECT_EQ(b - a, (Vec2{2.0, 2.0}));
}

TEST(Vec2, ScalesByANumber) {
    EXPECT_EQ((Vec2{1.5, -2.0}) * 2.0, (Vec2{3.0, -4.0}));
}

TEST(Vec2, LengthOfThreeFourIsFive) {
    EXPECT_DOUBLE_EQ((Vec2{3.0, 4.0}).length(), 5.0);
}

TEST(Robot, MovesByVelocityTimesTimeStep) {
    Robot robot;
    robot.velocity = {2.0, -1.0};
    sim::integrate(robot, 0.5);
    EXPECT_DOUBLE_EQ(robot.position.x, 1.0);
    EXPECT_DOUBLE_EQ(robot.position.y, -0.5);
}

TEST(Robot, StaysPutWithZeroVelocity) {
    Robot robot;
    robot.position = {1.0, 1.0};
    sim::integrate(robot, 1.0);
    EXPECT_EQ(robot.position, (Vec2{1.0, 1.0}));
}
