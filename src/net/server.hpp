#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "net/socket.hpp"
#include "sim/arena.hpp"

namespace net {

// The two-robot starting position: facing each other across the middle of the ring.
sim::Arena starting_arena();

// The match server. Threads during a match:
//   - the simulation thread (whoever calls run_match) runs the real-time tick loop;
//   - per client, a reader thread that turns incoming bytes into commands;
//   - per client, a writer thread that sends the latest state.
// Commands go reader -> sim through a ThreadSafeQueue; states go sim -> writer through a
// Mailbox. The arena itself is only ever touched by the simulation thread, so it needs no lock.
class Server {
public:
    // Start listening. port 0 lets the OS pick one (see port()).
    explicit Server(std::uint16_t port);

    std::uint16_t port() const { return port_; }

    // Wait for one client per robot in the arena (client k controls robot k), play one
    // match in real time, then disconnect everyone. Returns the winner's robot index, or
    // nothing for a draw or when max_ticks runs out.
    std::optional<std::size_t> run_match(sim::Arena arena, sim::Seconds dt, int max_ticks);

private:
    Socket listener_;
    std::uint16_t port_;
};

}  // namespace net
