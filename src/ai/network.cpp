#include "ai/network.hpp"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace ai {

Outputs forward(const Genome& genome, const Inputs& inputs) {
    if (genome.size() != kGenomeSize) {
        throw std::invalid_argument("genome has the wrong size");
    }
    std::size_t w = 0;  // index of the next weight to use, walking through the layout in order

    std::array<double, kHidden> hidden{};
    for (std::size_t h = 0; h < kHidden; ++h) {
        double sum = 0.0;
        for (std::size_t i = 0; i < kInputs; ++i) {
            sum += genome[w++] * inputs[i];
        }
        sum += genome[w++];  // bias
        hidden[h] = std::tanh(sum);
    }

    Outputs out{};
    for (std::size_t o = 0; o < kOutputs; ++o) {
        double sum = 0.0;
        for (std::size_t h = 0; h < kHidden; ++h) {
            sum += genome[w++] * hidden[h];
        }
        sum += genome[w++];  // bias
        out[o] = std::tanh(sum);
    }
    return out;
}

Inputs sense(const sim::Robot& me, const sim::Robot& opponent) {
    const sim::Vec2 offset = opponent.position - me.position;
    const sim::Vec2 closing = opponent.velocity - me.velocity;
    return {me.position.x, me.position.y, me.velocity.x, me.velocity.y,
            offset.x,      offset.y,      closing.x,     closing.y};
}

sim::Vec2 neural_thrust(const Genome& genome, const sim::Arena& arena, std::size_t me) {
    if (me >= arena.robots.size() || !arena.robots[me].in_ring) {
        return {};
    }
    // Two-robot matches: the opponent is the other robot still in the ring.
    for (std::size_t i = 0; i < arena.robots.size(); ++i) {
        if (i != me && arena.robots[i].in_ring) {
            const Outputs out = forward(genome, sense(arena.robots[me], arena.robots[i]));
            return {out[0], out[1]};
        }
    }
    return {};
}

void save_genome(const Genome& genome, const std::string& path) {
    std::ofstream file(path);
    if (!file) {
        throw std::runtime_error("cannot write " + path);
    }
    file << std::setprecision(17);  // 17 significant digits round-trip any double exactly
    for (double weight : genome) {
        file << weight << '\n';
    }
}

Genome load_genome(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("cannot read " + path);
    }
    Genome genome;
    double weight = 0.0;
    while (file >> weight) {
        genome.push_back(weight);
    }
    if (!file.eof() || genome.size() != kGenomeSize) {
        throw std::runtime_error(path + " is not a genome of " + std::to_string(kGenomeSize) + " numbers");
    }
    return genome;
}

}  // namespace ai
