#pragma once

#include <chrono>

#include "sim/vec2.hpp"

namespace sim {

// Simulation time step: seconds stored as a double. The unit is part of the type,
// so there are no bare numbers that might be seconds or milliseconds.
using Seconds = std::chrono::duration<double>;

// A circular robot. The simulator moves every robot by the same small time step each tick.
// Not thread-safe: only the simulation thread may touch a Robot.
struct Robot {
    Vec2 position;       // metres
    Vec2 velocity;       // metres per second
    double radius = 0.1; // metres
    bool in_ring = true; // false once pushed out; an out robot no longer moves or collides
};

// Advance one robot by dt using explicit Euler integration:
// new position = old position + velocity * dt. Simple and good enough at a small, fixed dt.
// Any chrono duration converts automatically, e.g. integrate(robot, std::chrono::milliseconds{16}).
inline void integrate(Robot& robot, Seconds dt) {
    robot.position = robot.position + robot.velocity * dt.count();
}

}  // namespace sim
