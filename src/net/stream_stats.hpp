#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

#include "sim/timing_stats.hpp"

namespace net {

// What a UDP watcher measures about the State datagrams it receives. UDP gives no
// guarantees, so each datagram may be lost, duplicated or arrive out of order; the tick
// number in every State is what lets us notice.
//
// Not thread-safe: the watcher's single receive loop calls on_state().
class StreamStats {
public:
    // Call once per State datagram, in the order they arrive.
    void on_state(std::uint32_t tick, std::chrono::nanoseconds latency) {
        ++received_;
        latency_.add(latency);

        if (!last_tick_ || (tick == 1 && *last_tick_ > 1)) {
            last_tick_ = tick;  // first datagram, or a new match started (ticks restart at 1)
            return;
        }
        if (tick > *last_tick_) {
            lost_ += tick - *last_tick_ - 1;  // every tick we skipped over is (so far) lost
            last_tick_ = tick;
        } else {
            // Older than one we already have: it was overtaken on the way. It was counted as
            // lost when we skipped past it, so take that back. (A duplicate would also land
            // here; on one machine that does not happen, so we do not tell them apart.)
            ++out_of_order_;
            if (lost_ > 0) {
                --lost_;
            }
        }
    }

    std::uint64_t received() const { return received_; }
    std::uint64_t lost() const { return lost_; }
    std::uint64_t out_of_order() const { return out_of_order_; }
    const sim::TimingStats& latency() const { return latency_; }

private:
    std::optional<std::uint32_t> last_tick_;
    std::uint64_t received_ = 0;
    std::uint64_t lost_ = 0;
    std::uint64_t out_of_order_ = 0;
    sim::TimingStats latency_;
};

}  // namespace net
