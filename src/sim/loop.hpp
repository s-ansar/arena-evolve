#pragma once

#include <functional>

#include "sim/arena.hpp"

namespace sim {

// Run the match as fast as the CPU allows, with a fixed simulated time step. Used for
// fitness evaluation, where nobody is watching and wall-clock time does not matter.
// Stops when the match is over or after max_ticks. Returns the number of ticks run.
int run_headless(Arena& arena, Seconds dt, int max_ticks);

struct RunStats {
    int ticks = 0;       // ticks actually run
    int late_ticks = 0;  // ticks that started after their deadline (the loop fell behind)
};

// Called after every real-time tick, on the loop's own thread. The server uses it to send
// the new state to clients and apply their latest commands. tick counts from 1.
using TickHook = std::function<void(Arena& arena, int tick)>;

// Run the match in real time: one tick every dt of wall-clock time, timed with
// std::chrono::steady_clock (it never jumps, unlike system_clock when the PC clock is changed).
// Stops when the match is over or after max_ticks. after_tick may be empty.
RunStats run_realtime(Arena& arena, Seconds dt, int max_ticks, const TickHook& after_tick = {});

}  // namespace sim
