// arena_fitness <genome file> [train|test]
// Scores one saved genome against the chase bot and prints one line:
//   fitness <number> wins <n> draws <n> losses <n>
// A standalone program on purpose: the Arena Platform can run many of these at once
// (Slurm job arrays, Kubernetes Jobs), one genome each, on any machine.

#include <cstdlib>
#include <iostream>
#include <numbers>
#include <string>

#include "ai/chase.hpp"
#include "ai/match.hpp"
#include "ai/network.hpp"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: arena_fitness <genome file> [train|test]\n";
        return EXIT_FAILURE;
    }
    const std::string set = argc > 2 ? argv[2] : "test";

    try {
        const ai::Genome genome = ai::load_genome(argv[1]);
        // Same angle sets as arena_evolve.
        const std::vector<double> angles =
            set == "train" ? ai::start_angles(8, 0.0) : ai::start_angles(16, std::numbers::pi / 16.0);
        const ai::SimController candidate = [&genome](const sim::Arena& a, std::size_t me) {
            return ai::neural_thrust(genome, a, me);
        };
        const ai::Record r = ai::evaluate(candidate, ai::chase_thrust, angles);
        std::cout << "fitness " << r.fitness << " wins " << r.wins << " draws " << r.draws << " losses "
                  << r.losses << "\n";
    } catch (const std::exception& error) {
        std::cerr << "arena_fitness: " << error.what() << std::endl;
        return EXIT_FAILURE;
    }
}
