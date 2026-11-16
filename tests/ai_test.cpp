#include <gtest/gtest.h>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <set>
#include <stdexcept>

#include "ai/chase.hpp"
#include "ai/genetic.hpp"
#include "ai/match.hpp"
#include "ai/network.hpp"
#include "ai/thread_pool.hpp"

namespace {

ai::SimController neural(const ai::Genome& genome) {
    return [&genome](const sim::Arena& a, std::size_t me) { return ai::neural_thrust(genome, a, me); };
}

sim::Vec2 sit_still(const sim::Arena&, std::size_t) { return {}; }

}  // namespace

// ---- network ----

TEST(Network, GenomeSizeMatchesTheLayout) {
    EXPECT_EQ(ai::kGenomeSize, 90u);  // 8 x (8 + 1) + 2 x (8 + 1)
}

TEST(Network, AllZeroWeightsGiveZeroThrust) {
    const ai::Genome zero(ai::kGenomeSize, 0.0);
    const ai::Outputs out = ai::forward(zero, {1, 2, 3, 4, 5, 6, 7, 8});
    EXPECT_EQ(out[0], 0.0);
    EXPECT_EQ(out[1], 0.0);
}

TEST(Network, ForwardFollowsTheDocumentedLayout) {
    ai::Genome genome(ai::kGenomeSize, 0.0);
    genome[0] = 1.0;  // hidden neuron 0, weight from input 0
    const std::size_t first_output = ai::kHidden * (ai::kInputs + 1);
    genome[first_output] = 1.0;  // output 0, weight from hidden neuron 0
    const ai::Outputs out = ai::forward(genome, {0.5, 0, 0, 0, 0, 0, 0, 0});
    EXPECT_DOUBLE_EQ(out[0], std::tanh(std::tanh(0.5)));
    EXPECT_EQ(out[1], 0.0);
}

TEST(Network, RejectsAGenomeOfTheWrongSize) {
    EXPECT_THROW(ai::forward(ai::Genome(3, 0.0), {}), std::invalid_argument);
}

TEST(Network, SenseUsesRelativeOpponentPosition) {
    sim::Robot me{.position = {0.1, 0.2}, .velocity = {0.5, 0.0}};
    sim::Robot them{.position = {0.4, -0.2}, .velocity = {0.0, 0.5}};
    const ai::Inputs in = ai::sense(me, them);
    EXPECT_DOUBLE_EQ(in[4], 0.3);   // offset x
    EXPECT_DOUBLE_EQ(in[5], -0.4);  // offset y
    EXPECT_DOUBLE_EQ(in[6], -0.5);  // closing velocity x
    EXPECT_DOUBLE_EQ(in[7], 0.5);   // closing velocity y
}

TEST(Network, SaveAndLoadGiveBackExactlyTheSameWeights) {
    ai::Rng rng(7);
    const ai::Genome genome = ai::random_genome(rng, 1.0);
    const std::string path = ::testing::TempDir() + "genome_roundtrip.txt";
    ai::save_genome(genome, path);
    EXPECT_EQ(ai::load_genome(path), genome);  // exact, not approximately equal
    std::remove(path.c_str());
}

TEST(Network, LoadRejectsAFileWithTooFewNumbers) {
    const std::string path = ::testing::TempDir() + "genome_short.txt";
    ai::save_genome(ai::Genome(5, 1.0), path);
    EXPECT_THROW(ai::load_genome(path), std::runtime_error);
    std::remove(path.c_str());
}

// ---- headless matches and fitness ----

TEST(Match, ChaseBeatsARobotThatSitsStill) {
    const ai::MatchOutcome outcome = ai::play_headless(ai::start_position(0.0), ai::chase_thrust, sit_still);
    ASSERT_TRUE(outcome.winner.has_value());
    EXPECT_EQ(*outcome.winner, 0u);
    EXPECT_LT(outcome.ticks, ai::kMatchMaxTicks);
}

TEST(Match, IsDeterministic) {
    const ai::MatchOutcome a = ai::play_headless(ai::start_position(1.0), ai::chase_thrust, sit_still);
    const ai::MatchOutcome b = ai::play_headless(ai::start_position(1.0), ai::chase_thrust, sit_still);
    EXPECT_EQ(a.winner, b.winner);
    EXPECT_EQ(a.ticks, b.ticks);
}

TEST(Match, StartPositionsAreOppositeEachOther) {
    const sim::Arena arena = ai::start_position(0.3);
    EXPECT_NEAR(arena.robots[0].position.length(), 0.5, 1e-12);
    EXPECT_NEAR((arena.robots[0].position + arena.robots[1].position).length(), 0.0, 1e-12);
}

TEST(Fitness, PlaysEveryAngleFromBothSides) {
    const ai::Record record = ai::evaluate(ai::chase_thrust, sit_still, ai::start_angles(3, 0.0));
    EXPECT_EQ(record.wins + record.draws + record.losses, 6);
    EXPECT_EQ(record.wins, 6);  // chase beats a sitting robot every time
    EXPECT_GT(record.fitness, 6 * 2.0);
    EXPECT_LE(record.fitness, 6 * 3.0);
}

// ---- genetic algorithm ----

TEST(Genetic, SameSeedGivesTheSameGenomes) {
    ai::Rng a(42);
    ai::Rng b(42);
    EXPECT_EQ(ai::random_genome(a, 1.0), ai::random_genome(b, 1.0));
}

TEST(Genetic, BigTournamentAlmostAlwaysPicksTheBest) {
    ai::Rng rng(1);
    const std::vector<double> fitness{0.0, 0.0, 10.0, 0.0};
    EXPECT_EQ(ai::tournament_select(fitness, 50, rng), 2u);
}

TEST(Genetic, CrossoverTakesEveryWeightFromAParent) {
    ai::Rng rng(3);
    const ai::Genome a(ai::kGenomeSize, 1.0);
    const ai::Genome b(ai::kGenomeSize, 2.0);
    const ai::Genome child = ai::crossover(a, b, rng);
    std::set<double> values(child.begin(), child.end());
    EXPECT_EQ(values, (std::set<double>{1.0, 2.0}));  // a mix of both, nothing else
}

TEST(Genetic, MutationRateZeroChangesNothingAndOneChangesEverything) {
    ai::Rng rng(5);
    const ai::Genome original(ai::kGenomeSize, 0.0);
    ai::Genome unchanged = original;
    ai::mutate(unchanged, 0.0, 1.0, rng);
    EXPECT_EQ(unchanged, original);

    ai::Genome changed = original;
    ai::mutate(changed, 1.0, 1.0, rng);
    for (double w : changed) {
        EXPECT_NE(w, 0.0);
    }
}

TEST(Genetic, NextGenerationKeepsTheElitesFirst) {
    ai::Rng rng(9);
    ai::GaParams params;
    params.population = 6;
    params.elites = 2;
    std::vector<ai::Genome> population;
    for (int i = 0; i < 6; ++i) {
        population.push_back(ai::Genome(ai::kGenomeSize, i));
    }
    const std::vector<double> fitness{1, 5, 3, 9, 0, 2};  // best is index 3, then index 1
    const std::vector<ai::Genome> next = ai::next_generation(population, fitness, params, rng);
    ASSERT_EQ(next.size(), 6u);
    EXPECT_EQ(next[0], population[3]);
    EXPECT_EQ(next[1], population[1]);
}

// A tiny evolution run. Evaluation is deterministic and the elites survive unchanged, so the
// best fitness must never go down.
TEST(Genetic, BestFitnessNeverGetsWorse) {
    ai::Rng rng(11);
    ai::GaParams params;
    params.population = 12;
    const std::vector<double> angles = ai::start_angles(2, 0.0);

    std::vector<ai::Genome> population;
    for (std::size_t i = 0; i < params.population; ++i) {
        population.push_back(ai::random_genome(rng, params.init_sigma));
    }
    double previous_best = -1.0;
    for (int generation = 0; generation < 4; ++generation) {
        std::vector<double> fitness;
        for (const ai::Genome& g : population) {
            fitness.push_back(ai::evaluate(neural(g), ai::chase_thrust, angles).fitness);
        }
        const double best = *std::max_element(fitness.begin(), fitness.end());
        EXPECT_GE(best, previous_best);
        previous_best = best;
        population = ai::next_generation(population, fitness, params, rng);
    }
}

// ---- thread pool ----

TEST(ThreadPool, ParallelForRunsEveryIndexOnce) {
    ai::ThreadPool pool(4);
    std::vector<int> hits(1000, 0);
    pool.parallel_for(hits.size(), [&hits](std::size_t i) { hits[i] += 1; });  // own slot only
    EXPECT_EQ(std::accumulate(hits.begin(), hits.end(), 0), 1000);
    EXPECT_EQ(*std::min_element(hits.begin(), hits.end()), 1);
}

TEST(ThreadPool, CanBeReusedAndFinishesSubmittedTasks) {
    std::atomic<int> count = 0;
    {
        ai::ThreadPool pool(3);
        pool.parallel_for(10, [&count](std::size_t) { ++count; });
        pool.parallel_for(10, [&count](std::size_t) { ++count; });
        for (int i = 0; i < 5; ++i) {
            pool.submit([&count] { ++count; });
        }
    }  // destructor runs the 5 queued tasks before stopping
    EXPECT_EQ(count.load(), 25);
}

TEST(ThreadPool, ParallelFitnessEqualsSerialFitness) {
    ai::Rng rng(13);
    std::vector<ai::Genome> population;
    for (int i = 0; i < 8; ++i) {
        population.push_back(ai::random_genome(rng, 1.0));
    }
    const std::vector<double> angles = ai::start_angles(2, 0.0);

    std::vector<double> serial;
    for (const ai::Genome& g : population) {
        serial.push_back(ai::evaluate(neural(g), ai::chase_thrust, angles).fitness);
    }
    std::vector<double> parallel(population.size());
    ai::ThreadPool pool(4);
    pool.parallel_for(population.size(), [&](std::size_t i) {
        parallel[i] = ai::evaluate(neural(population[i]), ai::chase_thrust, angles).fitness;
    });
    EXPECT_EQ(parallel, serial);  // bit-for-bit: no shared state between matches
}
