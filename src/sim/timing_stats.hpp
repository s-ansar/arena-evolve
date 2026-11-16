#pragma once

#include <chrono>
#include <cmath>
#include <cstddef>

namespace sim {

// Running statistics over many time measurements: count, min, max, mean and standard
// deviation, without storing every sample (the server may run for days).
//
// Mean and variance use Welford's method: each new sample nudges the mean and a running
// sum of squared differences. The obvious "sum of squares minus square of sum" formula
// loses most of its precision when the numbers are large and close together, which is
// exactly what tick intervals look like (millions of nanoseconds, differing by a few).
//
// Jitter, in this project, means the standard deviation of the tick interval.
// Not thread-safe: one thread adds samples.
class TimingStats {
public:
    using Duration = std::chrono::nanoseconds;

    void add(Duration sample) {
        const double x = static_cast<double>(sample.count());
        ++count_;
        const double delta = x - mean_;
        mean_ += delta / static_cast<double>(count_);
        m2_ += delta * (x - mean_);  // uses the old and the new mean, as Welford requires
        if (count_ == 1 || sample < min_) {
            min_ = sample;
        }
        if (count_ == 1 || sample > max_) {
            max_ = sample;
        }
    }

    std::size_t count() const { return count_; }
    Duration min() const { return min_; }
    Duration max() const { return max_; }
    Duration mean() const { return Duration{static_cast<Duration::rep>(std::llround(mean_))}; }

    // Population standard deviation (divides by n). Zero until there are two samples.
    Duration stddev() const {
        if (count_ < 2) {
            return Duration{0};
        }
        return Duration{static_cast<Duration::rep>(std::llround(std::sqrt(m2_ / static_cast<double>(count_))))};
    }

private:
    std::size_t count_ = 0;
    double mean_ = 0.0;  // nanoseconds
    double m2_ = 0.0;    // sum of squared differences from the mean, nanoseconds squared
    Duration min_{0};
    Duration max_{0};
};

}  // namespace sim
