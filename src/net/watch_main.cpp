// arena_watch [host] [port] [seconds]
// Watches a running server over UDP for a number of seconds, then prints how many State
// datagrams arrived, how many were lost or out of order, and the one-way latency.
// Latency is only meaningful when watcher and server run on the same machine (see State).

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

#include "net/protocol.hpp"
#include "net/socket.hpp"
#include "net/stream_stats.hpp"

namespace {

using Clock = std::chrono::steady_clock;

double to_us(std::chrono::nanoseconds d) { return static_cast<double>(d.count()) / 1000.0; }

}  // namespace

int main(int argc, char** argv) {
    const std::string host = argc > 1 ? argv[1] : "127.0.0.1";
    const auto port = static_cast<std::uint16_t>(argc > 2 ? std::stoi(argv[2]) : 5555);
    const std::chrono::seconds duration{argc > 3 ? std::stoi(argv[3]) : 10};

    try {
        const net::Socket udp = net::open_udp(0);
        // Wake at least every 200 ms even if nothing arrives, so the Watch renewal below
        // and the end time are checked on time.
        net::set_receive_timeout(udp, std::chrono::milliseconds{200});
        const net::Endpoint server = net::resolve_udp(host, port);
        const auto watch_frame = net::encode(net::Watch{});

        net::StreamStats stats;
        const auto end = Clock::now() + duration;
        auto next_watch = Clock::now();
        while (Clock::now() < end) {
            // Renew the lease every second. The first Watch, or any renewal, may itself be
            // lost: UDP. Sending again regularly is how UDP programs cope with that.
            if (Clock::now() >= next_watch) {
                net::send_datagram(udp, server, watch_frame);
                next_watch += std::chrono::seconds{1};
            }

            std::optional<net::Datagram> datagram = net::receive_datagram(udp, true);
            const auto arrived = Clock::now();  // read the clock first, before any decoding
            if (!datagram || !(datagram->from == server)) {
                continue;
            }
            try {
                const std::optional<net::Message> message = net::try_decode(datagram->bytes);
                if (const net::State* state = message ? std::get_if<net::State>(&*message) : nullptr) {
                    const auto arrived_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        arrived.time_since_epoch());
                    const std::chrono::nanoseconds sent_ns{static_cast<std::int64_t>(state->sent_at_ns)};
                    stats.on_state(state->tick, arrived_ns - sent_ns);
                }
            } catch (const net::ProtocolError&) {
                // a broken datagram: ignore it
            }
        }

        const auto& latency = stats.latency();
        std::cout << "received " << stats.received() << ", lost " << stats.lost()
                  << ", out of order " << stats.out_of_order() << "\n";
        if (latency.count() > 0) {
            std::cout << "latency us: mean " << to_us(latency.mean()) << ", stddev "
                      << to_us(latency.stddev()) << ", min " << to_us(latency.min()) << ", max "
                      << to_us(latency.max()) << "\n";
        }
    } catch (const std::exception& error) {
        std::cerr << "arena_watch: " << error.what() << std::endl;
        return EXIT_FAILURE;
    }
}
