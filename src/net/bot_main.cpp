// arena_bot [host] [port] [--passive]
// A controller client running the baseline chase controller. With --passive it sends no
// commands at all (a sitting target, handy for testing).

#include <cstdlib>
#include <iostream>
#include <string>

#include "ai/chase.hpp"
#include "net/client.hpp"

int main(int argc, char** argv) {
    const std::string host = argc > 1 ? argv[1] : "127.0.0.1";
    const auto port = static_cast<std::uint16_t>(argc > 2 ? std::stoi(argv[2]) : 5555);
    const bool passive = argc > 3 && std::string(argv[3]) == "--passive";

    try {
        const net::Controller controller = passive ? net::Controller{} : net::Controller{ai::chase};
        const std::uint8_t me = net::play_match(host, port, controller);
        std::cout << "arena_bot: match finished, I was robot " << static_cast<int>(me) << std::endl;
    } catch (const std::exception& error) {
        std::cerr << "arena_bot: " << error.what() << std::endl;
        return EXIT_FAILURE;
    }
}
