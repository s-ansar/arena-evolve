#include "ai/genetic.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace ai {

Genome random_genome(Rng& rng, double sigma) {
    std::normal_distribution<double> noise(0.0, sigma);
    Genome genome(kGenomeSize);
    for (double& weight : genome) {
        weight = noise(rng);
    }
    return genome;
}

std::size_t tournament_select(const std::vector<double>& fitness, std::size_t k, Rng& rng) {
    std::uniform_int_distribution<std::size_t> pick(0, fitness.size() - 1);
    std::size_t best = pick(rng);
    for (std::size_t i = 1; i < k; ++i) {
        const std::size_t challenger = pick(rng);
        if (fitness[challenger] > fitness[best]) {
            best = challenger;
        }
    }
    return best;
}

Genome crossover(const Genome& a, const Genome& b, Rng& rng) {
    std::bernoulli_distribution coin(0.5);
    Genome child(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        child[i] = coin(rng) ? a[i] : b[i];
    }
    return child;
}

void mutate(Genome& genome, double rate, double sigma, Rng& rng) {
    std::bernoulli_distribution should_mutate(rate);
    std::normal_distribution<double> noise(0.0, sigma);
    for (double& weight : genome) {
        if (should_mutate(rng)) {
            weight += noise(rng);
        }
    }
}

std::vector<Genome> next_generation(const std::vector<Genome>& population,
                                    const std::vector<double>& fitness, const GaParams& params, Rng& rng) {
    if (population.size() != fitness.size() || population.empty()) {
        throw std::invalid_argument("population and fitness must be the same, non-zero size");
    }

    // Indices sorted best first. stable_sort keeps ties in their original order, so the
    // result does not depend on how the sort happens to break ties.
    std::vector<std::size_t> order(population.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(),
                     [&fitness](std::size_t a, std::size_t b) { return fitness[a] > fitness[b]; });

    std::vector<Genome> next;
    next.reserve(params.population);
    for (std::size_t i = 0; i < params.elites && i < order.size(); ++i) {
        next.push_back(population[order[i]]);
    }
    while (next.size() < params.population) {
        const Genome& mum = population[tournament_select(fitness, params.tournament, rng)];
        const Genome& dad = population[tournament_select(fitness, params.tournament, rng)];
        Genome child = crossover(mum, dad, rng);
        mutate(child, params.mutation_rate, params.mutation_sigma, rng);
        next.push_back(std::move(child));
    }
    return next;
}

}  // namespace ai
