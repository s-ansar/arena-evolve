#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "sim/arena.hpp"

namespace ai {

// A small fixed-shape neural network: 8 inputs -> 8 hidden neurons -> 2 outputs.
// Every neuron computes tanh(weighted sum of its inputs + bias). tanh squashes any number
// into (-1, 1): smooth, and the outputs are already a sensible thrust range.
//
// The network has no training code. Its weights are the "genome" that the genetic
// algorithm evolves (neuroevolution), so the only operation needed is forward().
constexpr std::size_t kInputs = 8;
constexpr std::size_t kHidden = 8;
constexpr std::size_t kOutputs = 2;

// Genome layout, one flat list of numbers:
//   for each hidden neuron: kInputs weights, then its bias      (8 x 9 = 72)
//   for each output neuron: kHidden weights, then its bias      (2 x 9 = 18)
constexpr std::size_t kGenomeSize = kHidden * (kInputs + 1) + kOutputs * (kHidden + 1);  // 90

using Genome = std::vector<double>;
using Inputs = std::array<double, kInputs>;
using Outputs = std::array<double, kOutputs>;

// Run the network once. genome must have kGenomeSize numbers.
Outputs forward(const Genome& genome, const Inputs& inputs);

// What a robot "sees": its own position and velocity, and where the opponent is and how it
// moves RELATIVE to itself. All values are roughly in [-1, 1] because the ring has radius 1
// and top speed is 1 m/s, so no extra scaling is needed.
Inputs sense(const sim::Robot& me, const sim::Robot& opponent);

// The thrust the network wants for robot `me`, in a two-robot arena. Zero if `me` is out or
// there is no opponent left.
sim::Vec2 neural_thrust(const Genome& genome, const sim::Arena& arena, std::size_t me);

// Save as plain text, one number per line, with enough digits (17) that loading gives back
// exactly the same doubles. Text keeps genome files readable and diff-able.
void save_genome(const Genome& genome, const std::string& path);

// Throws std::runtime_error if the file cannot be read or does not hold kGenomeSize numbers.
Genome load_genome(const std::string& path);

}  // namespace ai
