// arena_evolve [generations] [threads] [seed] [output] [log]
// Evolves neural-network controllers against the chase bot and saves the best genome.
// Defaults: 40 generations, all CPU threads, seed 1, best_genome.txt, evolution.csv.
// The log (best and mean fitness per generation) can be charted with arena_view --fitness.
//
// Prints, per generation: best and mean fitness. At the end: the best genome's record on
// the 16 held-out start angles (never used in training), next to two baselines, and how
// much faster fitness evaluation runs on more threads.

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <numeric>
#include <string>
#include <thread>

#include "ai/chase.hpp"
#include "ai/evolution_log.hpp"
#include "ai/genetic.hpp"
#include "ai/match.hpp"
#include "ai/network.hpp"
#include "ai/thread_pool.hpp"

namespace {

using Clock = std::chrono::steady_clock;

// Score every genome against the chase bot, in parallel. Each task writes only its own
// slot of fitness, so the tasks share nothing that needs a lock.
std::vector<double> evaluate_population(ai::ThreadPool& pool, const std::vector<ai::Genome>& population,
                                        const std::vector<double>& angles) {
    std::vector<double> fitness(population.size());
    pool.parallel_for(population.size(), [&](std::size_t i) {
        const ai::Genome& genome = population[i];
        const ai::SimController candidate = [&genome](const sim::Arena& a, std::size_t me) {
            return ai::neural_thrust(genome, a, me);
        };
        fitness[i] = ai::evaluate(candidate, ai::chase_thrust, angles).fitness;
    });
    return fitness;
}

void print_record(const std::string& name, const ai::Record& r) {
    std::cout << "  " << std::left << std::setw(28) << name << std::right << " wins " << std::setw(2)
              << r.wins << "  draws " << std::setw(2) << r.draws << "  losses " << std::setw(2) << r.losses
              << "  fitness " << std::fixed << std::setprecision(2) << r.fitness << "\n";
}

double seconds_since(Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}

}  // namespace

int main(int argc, char** argv) {
    const int generations = argc > 1 ? std::stoi(argv[1]) : 40;
    const unsigned hardware = std::max(1u, std::thread::hardware_concurrency());
    const std::size_t threads = argc > 2 ? std::stoul(argv[2]) : hardware;
    const std::uint64_t seed = argc > 3 ? std::stoull(argv[3]) : 1;
    const std::string output = argc > 4 ? argv[4] : "best_genome.txt";
    const std::string log_path = argc > 5 ? argv[5] : "evolution.csv";

    try {
        const ai::GaParams params;
        ai::Rng rng(seed);
        ai::ThreadPool pool(threads);

        // Train on 8 start angles; test on 16 different ones (offset by half a step) to
        // check the controller learned to fight, not just memorised 8 openings.
        const std::vector<double> train = ai::start_angles(8, 0.0);
        const std::vector<double> test = ai::start_angles(16, std::numbers::pi / 16.0);

        std::vector<ai::Genome> population;
        for (std::size_t i = 0; i < params.population; ++i) {
            population.push_back(ai::random_genome(rng, params.init_sigma));
        }
        const ai::Genome first_random = population.front();

        std::cout << "evolving: population " << params.population << ", " << generations
                  << " generations, " << pool.size() << " threads, seed " << seed << "\n";
        const auto start = Clock::now();
        std::vector<double> fitness;
        std::vector<ai::GenerationStats> log;
        for (int generation = 0; generation <= generations; ++generation) {
            fitness = evaluate_population(pool, population, train);
            const double best = *std::max_element(fitness.begin(), fitness.end());
            const double mean = std::accumulate(fitness.begin(), fitness.end(), 0.0) / fitness.size();
            log.push_back({generation, best, mean});
            std::cout << "gen " << std::setw(3) << generation << "  best " << std::fixed << std::setprecision(2)
                      << best << "  mean " << mean << "\n";
            if (generation < generations) {
                population = ai::next_generation(population, fitness, params, rng);
            }
        }
        const double training_seconds = seconds_since(start);

        const auto best_index = static_cast<std::size_t>(
            std::max_element(fitness.begin(), fitness.end()) - fitness.begin());
        const ai::Genome& best = population[best_index];
        ai::save_genome(best, output);
        std::ofstream log_file(log_path);
        ai::write_evolution_log(log_file, log);
        if (!log_file) {
            throw std::runtime_error("cannot write " + log_path);
        }
        std::cout << "saved best genome to " << output << " and the log to " << log_path << " (training took "
                  << std::setprecision(1) << training_seconds << " s)\n";

        // Results on the held-out angles, 32 matches each (16 angles x both sides).
        const auto neural = [](const ai::Genome& g) {
            return ai::SimController{[&g](const sim::Arena& a, std::size_t me) { return ai::neural_thrust(g, a, me); }};
        };
        std::cout << "vs chase bot, 16 unseen start angles x 2 sides:\n";
        print_record("evolved (best genome)", ai::evaluate(neural(best), ai::chase_thrust, test));
        print_record("random genome (gen 0)", ai::evaluate(neural(first_random), ai::chase_thrust, test));
        print_record("chase bot itself", ai::evaluate(ai::chase_thrust, ai::chase_thrust, test));

        // Speed-up of one generation's evaluation with 1, 2, 4... threads.
        std::cout << "fitness evaluation, one generation of " << population.size() << " genomes:\n";
        double one_thread = 0.0;
        for (std::size_t n = 1; n <= hardware; n *= 2) {
            ai::ThreadPool timing_pool(n);
            evaluate_population(timing_pool, population, train);  // warm-up
            const auto t0 = Clock::now();
            constexpr int kRepeats = 3;
            for (int r = 0; r < kRepeats; ++r) {
                evaluate_population(timing_pool, population, train);
            }
            const double seconds = seconds_since(t0) / kRepeats;
            if (n == 1) {
                one_thread = seconds;
            }
            std::cout << "  " << n << " thread(s): " << std::setprecision(3) << seconds * 1000.0
                      << " ms, speed-up " << std::setprecision(2) << one_thread / seconds << "x\n";
        }
    } catch (const std::exception& error) {
        std::cerr << "arena_evolve: " << error.what() << std::endl;
        return EXIT_FAILURE;
    }
}
