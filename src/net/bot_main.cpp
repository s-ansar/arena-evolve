// arena_bot [host] [port] [--passive | --genome <file>]
// A controller client. By default it runs the baseline chase controller.
//   --passive        send no commands at all (a sitting target, handy for testing)
//   --genome <file>  drive with an evolved neural network saved by arena_evolve

#include <cstdlib>
#include <iostream>
#include <string>

#include "ai/chase.hpp"
#include "ai/network.hpp"
#include "net/client.hpp"

int main(int argc, char** argv) {
    const std::string host = argc > 1 ? argv[1] : "127.0.0.1";
    const auto port = static_cast<std::uint16_t>(argc > 2 ? std::stoi(argv[2]) : 5555);
    const std::string mode = argc > 3 ? argv[3] : "";

    try {
        net::Controller controller{ai::chase};
        ai::Genome genome;  // must outlive the controller that refers to it
        if (mode == "--passive") {
            controller = {};
        } else if (mode == "--genome") {
            if (argc < 5) {
                throw std::runtime_error("--genome needs a file name");
            }
            genome = ai::load_genome(argv[4]);
            // The network was evolved on sim::Arena, so rebuild one from each State.
            controller = [&genome](const net::State& state, std::uint8_t me) {
                const sim::Vec2 thrust = ai::neural_thrust(genome, ai::arena_from_state(state), me);
                return net::Command{static_cast<float>(thrust.x), static_cast<float>(thrust.y)};
            };
        }
        const std::uint8_t me = net::play_match(host, port, controller);
        std::cout << "arena_bot: match finished, I was robot " << static_cast<int>(me) << std::endl;
    } catch (const std::exception& error) {
        std::cerr << "arena_bot: " << error.what() << std::endl;
        return EXIT_FAILURE;
    }
}
