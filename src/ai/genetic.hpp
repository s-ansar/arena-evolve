#pragma once

#include <cstddef>
#include <random>
#include <vector>

#include "ai/network.hpp"

namespace ai {

// A plain generational genetic algorithm over network weights.
//
// One generation:
//   1. Elitism: the best few genomes are copied unchanged, so the best fitness can never
//      go down from one generation to the next (fitness evaluation is deterministic).
//   2. Every other child: pick two parents by tournament, mix them by uniform crossover,
//      then mutate a few weights with Gaussian noise.
//
// All randomness comes from one seeded std::mt19937_64, so a run is exactly reproducible
// from its seed. Not thread-safe: only the main evolution loop uses the generator.
struct GaParams {
    std::size_t population = 64;
    std::size_t elites = 2;           // copied unchanged into the next generation
    std::size_t tournament = 3;       // genomes compared per parent pick; bigger = greedier
    double mutation_rate = 0.1;       // chance that each weight is changed
    double mutation_sigma = 0.3;      // standard deviation of the noise added to it
    double init_sigma = 1.0;          // spread of the random starting weights
};

using Rng = std::mt19937_64;

Genome random_genome(Rng& rng, double sigma);

// Draw k genomes at random (with replacement) and return the index of the fittest.
// Strong genomes usually win, but weak ones sometimes get through, which keeps variety.
std::size_t tournament_select(const std::vector<double>& fitness, std::size_t k, Rng& rng);

// Each weight comes from parent a or parent b with equal chance.
Genome crossover(const Genome& a, const Genome& b, Rng& rng);

// Each weight, with probability rate, gets Gaussian noise of standard deviation sigma added.
void mutate(Genome& genome, double rate, double sigma, Rng& rng);

// Build the next population from the current one and its fitness scores (same order).
std::vector<Genome> next_generation(const std::vector<Genome>& population,
                                    const std::vector<double>& fitness, const GaParams& params, Rng& rng);

}  // namespace ai
