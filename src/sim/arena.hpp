#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "sim/robot.hpp"

namespace sim {

// A circular sumo ring centred on (0, 0). A robot is out once its centre is further than
// ring_radius from the centre (like a real sumo robot tipping off the edge).
// Not thread-safe: only the simulation thread may touch an Arena.
struct Arena {
    double ring_radius = 1.0;  // metres
    std::vector<Robot> robots;
};

// Advance the whole arena by one time step:
//   1. move every robot that is still in the ring,
//   2. push apart any robots that overlap and bounce them off each other,
//   3. mark robots whose centre has left the ring as out.
void step(Arena& arena, Seconds dt);

// The match is over when at most one robot is still in the ring.
bool match_over(const Arena& arena);

// Index of the only robot left in the ring, or nothing if the match is still going or
// it is a draw (every robot is out).
std::optional<std::size_t> winner(const Arena& arena);

}  // namespace sim
