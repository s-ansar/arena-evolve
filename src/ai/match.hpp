#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

#include "sim/arena.hpp"

namespace ai {

// A controller for headless matches: given the arena and which robot it drives, return the
// thrust. Must not keep state between calls, so the same match always plays out the same way.
using SimController = std::function<sim::Vec2(const sim::Arena& arena, std::size_t me)>;

// Every headless match uses the same rules, so fitness numbers are comparable.
inline constexpr sim::Seconds kMatchDt{1.0 / 60.0};  // the server's tick rate
inline constexpr int kMatchMaxTicks = 1200;          // 20 seconds; then it is a draw

struct MatchOutcome {
    std::optional<std::size_t> winner;  // nothing for a draw
    int ticks = 0;
};

// Play one match as fast as possible. Both controllers decide from the same snapshot before
// either thrust is applied, so robot 0 gets no advantage from "moving first".
MatchOutcome play_headless(sim::Arena arena, const SimController& robot0, const SimController& robot1);

// Robot 0 at distance 0.5 from the centre at the given angle, robot 1 opposite it.
sim::Arena start_position(double angle_radians);

// count angles spread evenly round the circle, starting at offset.
std::vector<double> start_angles(int count, double offset_radians);

struct Record {
    int wins = 0;
    int draws = 0;
    int losses = 0;
    double fitness = 0.0;
};

// Play candidate against opponent from every start angle, once as robot 0 and once as
// robot 1 (so neither side of the ring is favoured). Fitness per match:
//   win:  2 + (1 - ticks / max)   winning beats everything; winning sooner is a bit better
//   draw: 1
//   loss: ticks / max              losing later is better than losing at once
// The tiers make any win worth more than any draw, and any draw more than any loss, while
// the small extra terms give the genetic algorithm a slope to climb even before it wins.
Record evaluate(const SimController& candidate, const SimController& opponent,
                const std::vector<double>& angles);

}  // namespace ai
