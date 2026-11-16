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
    Vec2 position{};         // metres
    Vec2 velocity{};         // metres per second
    Vec2 thrust{};           // metres per second squared; set by the robot's controller
    double radius = 0.1;     // metres
    double max_thrust = 1.0; // metres per second squared; stronger requests are cut to this
    double drag = 1.0;       // per second; slows the robot, so top speed = max_thrust / drag
    bool in_ring = true;     // false once pushed out; an out robot no longer moves or collides
};

// Advance one robot by dt using explicit Euler integration:
//   new position = old position + velocity * dt
//   new velocity = old velocity + (thrust - drag * velocity) * dt
// Position uses the old velocity, so a single step is exactly velocity * dt.
// Simple and good enough at a small, fixed dt.
// Any chrono duration converts automatically, e.g. integrate(robot, std::chrono::milliseconds{16}).
inline void integrate(Robot& robot, Seconds dt) {
    const double t = dt.count();
    robot.position = robot.position + robot.velocity * t;

    // A controller cannot ask for more than the motors can give.
    Vec2 thrust = robot.thrust;
    const double strength = thrust.length();
    if (strength > robot.max_thrust) {
        thrust = thrust * (robot.max_thrust / strength);
    }
    robot.velocity = robot.velocity + (thrust - robot.velocity * robot.drag) * t;
}

}  // namespace sim
