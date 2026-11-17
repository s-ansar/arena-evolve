#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>

namespace net {

// Numbers the server exposes for monitoring (Prometheus scrapes them from /metrics).
//
// Threads: the simulation thread writes, the HTTP thread reads. Every field is a
// std::atomic, so each single read or write is indivisible and no lock is needed.
// memory_order_relaxed is enough: each number stands on its own, and a scrape that sees
// "ticks" from one moment and "matches" from a moment later is harmless.
struct Metrics {
    std::atomic<std::uint64_t> matches_won{0};
    std::atomic<std::uint64_t> matches_drawn{0};
    std::atomic<std::uint64_t> ticks{0};
    std::atomic<std::uint64_t> late_ticks{0};
    std::atomic<std::uint64_t> watchers{0};
    std::atomic<bool> match_running{false};
    // From the last finished match, in seconds (Prometheus convention: base units).
    std::atomic<double> last_interval_mean_seconds{0.0};
    std::atomic<double> last_jitter_seconds{0.0};
    std::atomic<double> last_lateness_max_seconds{0.0};
};

// The Prometheus text exposition format: "# HELP" and "# TYPE" lines, then "name value".
// Counters only ever go up (Prometheus computes rates from them); gauges go up and down.
std::string render_prometheus(const Metrics& metrics);

// Answer one HTTP request (the raw text up to the blank line) with a complete response:
//   GET /metrics  -> 200, Prometheus text
//   GET /health   -> 200, "ok" (liveness: the process is up and answering)
//   GET other     -> 404;  other methods -> 405;  garbage -> 400
// Pure function, no I/O, so it is unit-tested without sockets.
std::string http_response(std::string_view request, const Metrics& metrics);

}  // namespace net
