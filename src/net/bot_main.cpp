// arena_bot [host] [port] [--passive] [--genome <file>] [--forever]
// A controller client. By default it runs the baseline chase controller.
//   --passive        send no commands at all (a sitting target, handy for testing)
//   --genome <file>  drive with an evolved neural network saved by arena_evolve
//   --forever        play match after match, reconnecting (and retrying once a second if
//                    the server is unreachable). Used in containers and Kubernetes.
// Prints one line per match: finished or dropped (connection lost mid-match).

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include "ai/chase.hpp"
#include "ai/network.hpp"
#include "net/client.hpp"

int main(int argc, char** argv) {
    const std::string host = argc > 1 ? argv[1] : "127.0.0.1";
    const auto port = static_cast<std::uint16_t>(argc > 2 ? std::stoi(argv[2]) : 5555);

    try {
        net::Controller controller{ai::chase};
        ai::Genome genome;  // must outlive the controller that refers to it
        bool forever = false;
        for (int i = 3; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--passive") {
                controller = {};
            } else if (option == "--forever") {
                forever = true;
            } else if (option == "--genome" && i + 1 < argc) {
                genome = ai::load_genome(argv[++i]);
                // The network was evolved on sim::Arena, so rebuild one from each State.
                controller = [&genome](const net::State& state, std::uint8_t me) {
                    const sim::Vec2 thrust = ai::neural_thrust(genome, ai::arena_from_state(state), me);
                    return net::Command{static_cast<float>(thrust.x), static_cast<float>(thrust.y)};
                };
            } else {
                throw std::runtime_error("unknown option " + option);
            }
        }

        int played = 0;
        do {
            try {
                const net::MatchReport report = net::play_match(host, port, controller);
                ++played;
                std::cout << "arena_bot: match " << played << " " << (report.finished ? "finished" : "DROPPED")
                          << " at tick " << report.ticks << ", I was robot " << static_cast<int>(report.robot)
                          << std::endl;
            } catch (const std::exception& error) {
                if (!forever) {
                    throw;
                }
                // Server not up yet, restarting, or draining: try again shortly.
                std::cerr << "arena_bot: " << error.what() << ", retrying" << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds{1});
            }
        } while (forever);
    } catch (const std::exception& error) {
        std::cerr << "arena_bot: " << error.what() << std::endl;
        return EXIT_FAILURE;
    }
}
