#include "sim/loop.hpp"

#include <chrono>
#include <thread>

namespace sim {

int run_headless(Arena& arena, Seconds dt, int max_ticks) {
    int ticks = 0;
    while (ticks < max_ticks && !match_over(arena)) {
        step(arena, dt);
        ++ticks;
    }
    return ticks;
}

RunStats run_realtime(Arena& arena, Seconds dt, int max_ticks) {
    using Clock = std::chrono::steady_clock;
    // Convert the step to the clock's own integer unit once, so deadlines add up exactly
    // instead of collecting floating-point rounding error over thousands of ticks.
    const auto period = std::chrono::duration_cast<Clock::duration>(dt);

    RunStats stats;
    auto deadline = Clock::now() + period;
    while (stats.ticks < max_ticks && !match_over(arena)) {
        // Deadlines are start + n * period, not "now + period", so small delays do not add up
        // into drift. sleep_until returns at once if the deadline has already passed.
        if (Clock::now() > deadline) {
            ++stats.late_ticks;
        }
        std::this_thread::sleep_until(deadline);

        // The simulation always advances by exactly dt, however late we are. That keeps
        // the physics the same on a fast or a slow machine.
        step(arena, dt);
        ++stats.ticks;
        deadline += period;

        // If we fell more than a whole tick behind (for example the process was paused),
        // do not rush through a burst of catch-up ticks. Restart the schedule from now.
        if (Clock::now() > deadline + period) {
            deadline = Clock::now() + period;
        }
    }
    return stats;
}

}  // namespace sim
