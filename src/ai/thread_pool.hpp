#pragma once

#include <cstddef>
#include <functional>
#include <thread>
#include <vector>

#include "net/thread_safe_queue.hpp"

namespace ai {

// A fixed set of worker threads that run tasks from a shared queue. Starting threads costs
// tens of microseconds each, so the pool starts them once and reuses them for every
// generation of the genetic algorithm.
//
// Thread safety: submit() and parallel_for() may be called from any one thread. The queue
// does its own locking. Tasks must not throw (an exception escaping a worker ends the
// program).
class ThreadPool {
public:
    explicit ThreadPool(std::size_t threads);
    ~ThreadPool();  // finishes queued tasks, then stops and joins the workers

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    std::size_t size() const { return workers_.size(); }

    void submit(std::function<void()> task);

    // Run body(i) for every i in [0, count) on the pool, and return once all have finished.
    // Each call must only write to its own data (for example element i of a vector).
    void parallel_for(std::size_t count, const std::function<void(std::size_t)>& body);

private:
    // Declared before workers_, so it exists before any worker starts using it.
    net::ThreadSafeQueue<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;
};

}  // namespace ai
