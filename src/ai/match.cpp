#include "ai/match.hpp"

#include <cmath>
#include <numbers>

namespace ai {

MatchOutcome play_headless(sim::Arena arena, const SimController& robot0, const SimController& robot1) {
    int ticks = 0;
    while (ticks < kMatchMaxTicks && !sim::match_over(arena)) {
        const sim::Vec2 thrust0 = robot0(arena, 0);
        const sim::Vec2 thrust1 = robot1(arena, 1);
        arena.robots[0].thrust = thrust0;
        arena.robots[1].thrust = thrust1;
        sim::step(arena, kMatchDt);
        ++ticks;
    }
    return {sim::winner(arena), ticks};
}

sim::Arena start_position(double angle_radians) {
    const sim::Vec2 spot{0.5 * std::cos(angle_radians), 0.5 * std::sin(angle_radians)};
    sim::Arena arena;
    arena.robots.push_back(sim::Robot{.position = spot});
    arena.robots.push_back(sim::Robot{.position = spot * -1.0});
    return arena;
}

std::vector<double> start_angles(int count, double offset_radians) {
    std::vector<double> angles;
    for (int i = 0; i < count; ++i) {
        angles.push_back(offset_radians + 2.0 * std::numbers::pi * i / count);
    }
    return angles;
}

namespace {

void score(Record& record, const MatchOutcome& outcome, std::size_t candidate_index) {
    const double time_used = static_cast<double>(outcome.ticks) / kMatchMaxTicks;
    if (!outcome.winner) {
        ++record.draws;
        record.fitness += 1.0;
    } else if (*outcome.winner == candidate_index) {
        ++record.wins;
        record.fitness += 2.0 + (1.0 - time_used);
    } else {
        ++record.losses;
        record.fitness += time_used;
    }
}

}  // namespace

Record evaluate(const SimController& candidate, const SimController& opponent,
                const std::vector<double>& angles) {
    Record record;
    for (double angle : angles) {
        score(record, play_headless(start_position(angle), candidate, opponent), 0);
        score(record, play_headless(start_position(angle), opponent, candidate), 1);
    }
    return record;
}

}  // namespace ai
