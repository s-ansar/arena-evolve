#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>

namespace net {

// Holds only the most recent value. put() replaces whatever was there; take() waits for a
// value and removes it.
//
// Why not a queue: the simulation thread hands each client's writer thread the latest arena
// state. If a client is slow, old states are worthless, and a queue would grow without
// limit. A mailbox drops the stale state instead, and put() never blocks, so a slow or stuck
// client can never delay the real-time tick loop.
//
// Thread safety: value_ and closed_ are only touched with mutex_ held.
template <typename T>
class Mailbox {
public:
    void put(T value) {
        {
            std::lock_guard lock(mutex_);
            value_ = std::move(value);
        }
        ready_.notify_one();
    }

    // Wait for a value or for close(). A value put before close() is still delivered, so
    // the client gets the final state of a match. Returns nothing once closed and empty.
    std::optional<T> take() {
        std::unique_lock lock(mutex_);
        ready_.wait(lock, [this] { return value_.has_value() || closed_; });
        std::optional<T> result = std::move(value_);
        value_.reset();
        return result;
    }

    void close() {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
        }
        ready_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable ready_;
    std::optional<T> value_;  // guarded by mutex_
    bool closed_ = false;     // guarded by mutex_
};

}  // namespace net
