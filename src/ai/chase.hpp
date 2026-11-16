#pragma once

#include <cstddef>
#include <cstdint>

#include "net/protocol.hpp"
#include "sim/arena.hpp"

namespace ai {

// The baseline controller: drive straight at the nearest opponent still in the ring, at
// full thrust. No learning. Evolved controllers must beat this to count as better.
// Returns zero thrust if there is no opponent left or the robot itself is out.
sim::Vec2 chase_thrust(const sim::Arena& arena, std::size_t me);

// The same controller for a networked client, which only sees State messages.
net::Command chase(const net::State& state, std::uint8_t me);

// Rebuild an arena from a State message, so controllers written against sim::Arena (the
// fast headless matches) also work for networked clients. Radius and other settings that
// State does not carry keep their defaults.
sim::Arena arena_from_state(const net::State& state);

}  // namespace ai
