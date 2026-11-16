#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>

namespace net {

// A first-in, first-out queue that many threads can push to and pop from at once.
// The server uses one: every client's reader thread pushes commands, and the simulation
// thread pops them each tick.
//
// Thread safety: every member function locks mutex_, so items_ and closed_ are only ever
// touched by one thread at a time. The condition variable lets pop() sleep until there is
// something to take, instead of spinning and burning CPU.
template <typename T>
class ThreadSafeQueue {
public:
    void push(T item) {
        {
            std::lock_guard lock(mutex_);
            items_.push_back(std::move(item));
        }
        // Notify after unlocking, so the woken thread does not immediately block on the mutex.
        ready_.notify_one();
    }

    // Take the front item if there is one. Never blocks, so it is safe in a real-time loop.
    std::optional<T> try_pop() {
        std::lock_guard lock(mutex_);
        return take_front();
    }

    // Wait until an item arrives or the queue is closed. Returns nothing only when the
    // queue is closed and empty, so items pushed before close() are never lost.
    std::optional<T> pop() {
        std::unique_lock lock(mutex_);
        // wait() rechecks the condition after every wake-up, which also handles spurious
        // wake-ups (a condition variable may wake a thread even without a notify).
        ready_.wait(lock, [this] { return !items_.empty() || closed_; });
        return take_front();
    }

    // Wake every waiting pop() so the threads can finish. Used at shutdown.
    void close() {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
        }
        ready_.notify_all();
    }

private:
    // Caller must hold mutex_.
    std::optional<T> take_front() {
        if (items_.empty()) {
            return std::nullopt;
        }
        T item = std::move(items_.front());
        items_.pop_front();
        return item;
    }

    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<T> items_;  // guarded by mutex_
    bool closed_ = false;  // guarded by mutex_
};

}  // namespace net
