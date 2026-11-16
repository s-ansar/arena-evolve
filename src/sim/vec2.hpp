#pragma once

#include <cmath>

namespace sim {

// A 2D vector, used for positions, velocities and forces in the arena. Units are metres
// for positions and metres per second for velocities.
struct Vec2 {
    double x = 0.0;
    double y = 0.0;

    constexpr Vec2 operator+(Vec2 other) const { return {x + other.x, y + other.y}; }
    constexpr Vec2 operator-(Vec2 other) const { return {x - other.x, y - other.y}; }
    constexpr Vec2 operator*(double k) const { return {x * k, y * k}; }
    constexpr bool operator==(const Vec2&) const = default;

    // Dot product: how much two vectors point the same way. Used to find the speed along a
    // collision direction.
    constexpr double dot(Vec2 other) const { return x * other.x + y * other.y; }

    double length() const { return std::sqrt(x * x + y * y); }
};

}  // namespace sim
