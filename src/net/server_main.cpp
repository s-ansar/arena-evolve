// arena_server [port] [metrics_port]
// Plays two-robot matches forever: waits for two controller clients, runs a match in real
// time at 60 ticks per second, prints the result, then waits for the next pair.
// Serves /metrics (Prometheus) and /health over HTTP on metrics_port (default 8080).

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

#include "net/http_server.hpp"
#include "net/server.hpp"

namespace {

double to_us(std::chrono::nanoseconds d) { return static_cast<double>(d.count()) / 1000.0; }

void print_timing(const sim::RunStats& stats) {
    std::cout << "  ticks " << stats.ticks << ", late " << stats.late_ticks << "\n"
              << "  interval us: mean " << to_us(stats.interval.mean()) << ", jitter (stddev) "
              << to_us(stats.interval.stddev()) << ", min " << to_us(stats.interval.min())
              << ", max " << to_us(stats.interval.max()) << "\n"
              << "  wake-up lateness us: mean " << to_us(stats.lateness.mean()) << ", max "
              << to_us(stats.lateness.max()) << std::endl;
}

}  // namespace

int main(int argc, char** argv) {
    const auto port = static_cast<std::uint16_t>(argc > 1 ? std::stoi(argv[1]) : 5555);
    const auto metrics_port = static_cast<std::uint16_t>(argc > 2 ? std::stoi(argv[2]) : 8080);
    const sim::Seconds tick{1.0 / 60.0};
    const int max_ticks = 60 * 60;  // one minute; then it is a draw

    try {
        net::Server server(port);
        // Declared after server, so it is destroyed first: it reads server.metrics().
        const net::HttpServer http(metrics_port, server.metrics());
        std::cout << "arena_server listening on port " << server.port() << ", metrics on port " << http.port()
                  << std::endl;
        while (true) {
            const net::MatchResult result = server.run_match(net::starting_arena(), tick, max_ticks);
            if (result.winner) {
                std::cout << "match over: robot " << *result.winner << " wins" << std::endl;
            } else {
                std::cout << "match over: draw" << std::endl;
            }
            print_timing(result.stats);
        }
    } catch (const std::exception& error) {
        std::cerr << "arena_server: " << error.what() << std::endl;
        return EXIT_FAILURE;
    }
}
