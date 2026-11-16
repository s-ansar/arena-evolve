#pragma once

#include <cstdint>

#include "net/protocol.hpp"

namespace ai {

// The baseline controller: drive straight at the nearest opponent still in the ring, at
// full thrust. No learning. Week 4's evolved controllers must beat this to count as better.
// Returns zero thrust if there is no opponent left or the robot itself is out.
net::Command chase(const net::State& state, std::uint8_t me);

}  // namespace ai
