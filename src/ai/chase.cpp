#include "ai/chase.hpp"

#include <cmath>
#include <limits>

namespace ai {

net::Command chase(const net::State& state, std::uint8_t me) {
    if (me >= state.robots.size() || !state.robots[me].in_ring) {
        return {};
    }
    const net::RobotState& self = state.robots[me];

    // Find the nearest opponent that is still in the ring.
    float best_dx = 0.0f;
    float best_dy = 0.0f;
    float best_distance = std::numeric_limits<float>::infinity();
    for (std::size_t i = 0; i < state.robots.size(); ++i) {
        const net::RobotState& other = state.robots[i];
        if (i == me || !other.in_ring) {
            continue;
        }
        const float dx = other.x - self.x;
        const float dy = other.y - self.y;
        const float distance = std::hypot(dx, dy);
        if (distance < best_distance) {
            best_dx = dx;
            best_dy = dy;
            best_distance = distance;
        }
    }

    if (!std::isfinite(best_distance) || best_distance == 0.0f) {
        return {};  // nobody left to chase, or exactly on top of them (no direction)
    }
    // A unit vector towards the target. Length 1 is the robot's full thrust; the server
    // would cut anything stronger down anyway.
    return {best_dx / best_distance, best_dy / best_distance};
}

}  // namespace ai
