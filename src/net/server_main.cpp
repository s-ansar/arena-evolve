// arena_server [port]
// Plays two-robot matches forever: waits for two controller clients, runs a match in real
// time at 60 ticks per second, prints the result, then waits for the next pair.

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

#include "net/server.hpp"

int main(int argc, char** argv) {
    const auto port = static_cast<std::uint16_t>(argc > 1 ? std::stoi(argv[1]) : 5555);
    const sim::Seconds tick{1.0 / 60.0};
    const int max_ticks = 60 * 60;  // one minute; then it is a draw

    try {
        net::Server server(port);
        std::cout << "arena_server listening on port " << server.port() << std::endl;
        while (true) {
            const auto winner = server.run_match(net::starting_arena(), tick, max_ticks);
            if (winner) {
                std::cout << "match over: robot " << *winner << " wins" << std::endl;
            } else {
                std::cout << "match over: draw" << std::endl;
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "arena_server: " << error.what() << std::endl;
        return EXIT_FAILURE;
    }
}
