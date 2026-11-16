#include "sim/arena.hpp"

namespace sim {

namespace {

// Resolve one pair of robots. All robots have the same mass, so the maths stays simple.
void collide(Robot& a, Robot& b) {
    const Vec2 between = b.position - a.position;
    const double distance = between.length();
    const double touching = a.radius + b.radius;

    // Not overlapping: nothing to do. Exactly on top of each other: there is no direction
    // to push in, so skip (this needs two robots placed at the same spot, which spawn avoids).
    if (distance >= touching || distance == 0.0) {
        return;
    }

    // Unit vector from a to b: the direction of the collision.
    const Vec2 normal = between * (1.0 / distance);

    // 1. Position fix: move each robot back half the overlap, so they end up just touching.
    const double overlap = touching - distance;
    a.position = a.position - normal * (overlap / 2.0);
    b.position = b.position + normal * (overlap / 2.0);

    // 2. Velocity fix: an elastic collision between equal masses swaps the parts of their
    // velocities that point along the normal. Sideways parts are unchanged.
    // closing_speed < 0 means they are moving towards each other. If they are already moving
    // apart, leave the velocities alone, or they would get stuck together.
    const double closing_speed = (b.velocity - a.velocity).dot(normal);
    if (closing_speed < 0.0) {
        a.velocity = a.velocity + normal * closing_speed;
        b.velocity = b.velocity - normal * closing_speed;
    }
}

}  // namespace

void step(Arena& arena, Seconds dt) {
    for (Robot& robot : arena.robots) {
        if (robot.in_ring) {
            integrate(robot, dt);
        }
    }

    // Check every pair once. O(n^2), which is fine for the handful of robots in a match.
    for (std::size_t i = 0; i < arena.robots.size(); ++i) {
        for (std::size_t j = i + 1; j < arena.robots.size(); ++j) {
            Robot& a = arena.robots[i];
            Robot& b = arena.robots[j];
            if (a.in_ring && b.in_ring) {
                collide(a, b);
            }
        }
    }

    // Ring-out check last, so a robot pushed out by a collision this tick is caught now.
    for (Robot& robot : arena.robots) {
        if (robot.in_ring && robot.position.length() > arena.ring_radius) {
            robot.in_ring = false;
        }
    }
}

bool match_over(const Arena& arena) {
    std::size_t still_in = 0;
    for (const Robot& robot : arena.robots) {
        if (robot.in_ring) {
            ++still_in;
        }
    }
    return still_in <= 1;
}

std::optional<std::size_t> winner(const Arena& arena) {
    std::optional<std::size_t> last_in;
    for (std::size_t i = 0; i < arena.robots.size(); ++i) {
        if (arena.robots[i].in_ring) {
            if (last_in) {
                return std::nullopt;  // two or more still in: no winner yet
            }
            last_in = i;
        }
    }
    return last_in;  // exactly one in: the winner; none in: a draw
}

}  // namespace sim
