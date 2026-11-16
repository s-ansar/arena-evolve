#include "ai/chase.hpp"

#include <limits>

namespace ai {

sim::Vec2 chase_thrust(const sim::Arena& arena, std::size_t me) {
    if (me >= arena.robots.size() || !arena.robots[me].in_ring) {
        return {};
    }
    const sim::Robot& self = arena.robots[me];

    // Find the nearest opponent that is still in the ring.
    sim::Vec2 best_offset;
    double best_distance = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < arena.robots.size(); ++i) {
        const sim::Robot& other = arena.robots[i];
        if (i == me || !other.in_ring) {
            continue;
        }
        const sim::Vec2 offset = other.position - self.position;
        const double distance = offset.length();
        if (distance < best_distance) {
            best_offset = offset;
            best_distance = distance;
        }
    }

    if (best_distance == std::numeric_limits<double>::infinity() || best_distance == 0.0) {
        return {};  // nobody left to chase, or exactly on top of them (no direction)
    }
    // A unit vector towards the target. Length 1 is the robot's full thrust; the simulation
    // would cut anything stronger down anyway.
    return best_offset * (1.0 / best_distance);
}

sim::Arena arena_from_state(const net::State& state) {
    sim::Arena arena;
    for (const net::RobotState& r : state.robots) {
        sim::Robot robot;
        robot.position = {r.x, r.y};
        robot.velocity = {r.vx, r.vy};
        robot.in_ring = r.in_ring;
        arena.robots.push_back(robot);
    }
    return arena;
}

net::Command chase(const net::State& state, std::uint8_t me) {
    const sim::Vec2 thrust = chase_thrust(arena_from_state(state), me);
    return {static_cast<float>(thrust.x), static_cast<float>(thrust.y)};
}

}  // namespace ai
