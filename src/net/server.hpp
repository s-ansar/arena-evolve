#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "net/metrics.hpp"
#include "net/socket.hpp"
#include "sim/arena.hpp"
#include "sim/loop.hpp"

namespace net {

// The two-robot starting position: facing each other across the middle of the ring.
sim::Arena starting_arena();

struct MatchResult {
    std::optional<std::size_t> winner;  // nothing for a draw or when max_ticks ran out
    sim::RunStats stats;                // tick timing: lateness and jitter
};

// The match server. Threads during a match:
//   - the simulation thread (whoever calls run_match) runs the real-time tick loop;
//   - per client, a reader thread that turns incoming bytes into commands;
//   - per client, a writer thread that sends the latest state.
// Commands go reader -> sim through a ThreadSafeQueue; states go sim -> writer through a
// Mailbox. The arena itself is only ever touched by the simulation thread, so it needs no lock.
//
// Watchers (viewers, measurement tools) get every State over UDP instead. They send a Watch
// datagram to the server's port (same number as TCP; UDP and TCP ports are separate) and
// must repeat it every second or they are dropped.
class Server {
public:
    // Start listening on TCP and UDP. port 0 lets the OS pick one (see port()).
    explicit Server(std::uint16_t port);

    std::uint16_t port() const { return port_; }

    // Wait for one client per robot in the arena (client k controls robot k), play one
    // match in real time, then disconnect everyone. Returns nothing if a stop was requested
    // before the match could start.
    std::optional<MatchResult> run_match(sim::Arena arena, sim::Seconds dt, int max_ticks);

    // Graceful drain, safe to call from any thread (used on SIGTERM): start no new matches,
    // but let a match that is already running finish. /ready then reports 503, so
    // Kubernetes stops sending new players here.
    void request_stop();
    bool stopping() const { return metrics_.draining.load(std::memory_order_relaxed); }

    // Live counters for monitoring. Safe to read from any thread (all atomics); written only
    // by the simulation thread.
    const Metrics& metrics() const { return metrics_; }

private:
    using Clock = std::chrono::steady_clock;

    struct Watcher {
        Endpoint endpoint;
        Clock::time_point last_heard;
    };

    // Both run on the simulation thread only, so watchers_ needs no lock.
    void update_watchers();
    void broadcast(const std::vector<std::uint8_t>& frame);

    Socket listener_;
    std::uint16_t port_;
    Socket udp_;
    std::vector<Watcher> watchers_;  // simulation thread only
    Metrics metrics_;
};

}  // namespace net
