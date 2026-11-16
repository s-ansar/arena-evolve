#pragma once

#include <istream>
#include <ostream>
#include <vector>

namespace ai {

// One line of the evolution log: how good the population was in one generation.
struct GenerationStats {
    int generation = 0;
    double best = 0.0;
    double mean = 0.0;
    bool operator==(const GenerationStats&) const = default;
};

// CSV with a header line, then one line per generation:
//   generation,best,mean
//   0,16,2.32
// arena_evolve writes it; arena_view charts it. Plain CSV also opens in Excel or pandas.
void write_evolution_log(std::ostream& out, const std::vector<GenerationStats>& rows);

// Throws std::runtime_error on a missing header or a malformed line.
std::vector<GenerationStats> read_evolution_log(std::istream& in);

}  // namespace ai
