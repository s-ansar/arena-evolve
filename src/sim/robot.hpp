#pragma once

#include "sim/vec2.hpp"

namespace sim {

// A circular robot. The simulator moves every robot by the same small time step each tick.
// Not thread-safe: only the simulation thread may touch a Robot.
struct Robot {
    Vec2 position;       // metres
    Vec2 velocity;       // metres per second
    double radius = 0.1; // metres
};

// Advance one robot by dt seconds using explicit Euler integration:
// new position = old position + velocity * dt. Simple and good enough at a small, fixed dt.
inline void integrate(Robot& robot, double dt) {
    robot.position = robot.position + robot.velocity * dt;
}

}  // namespace sim
