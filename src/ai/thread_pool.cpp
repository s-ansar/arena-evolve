#include "ai/thread_pool.hpp"

#include <latch>

namespace ai {

ThreadPool::ThreadPool(std::size_t threads) {
    if (threads == 0) {
        threads = 1;
    }
    for (std::size_t i = 0; i < threads; ++i) {
        workers_.emplace_back([this] {
            // pop() sleeps until there is a task, and returns nothing once the queue is
            // closed and empty: that is the signal for this worker to finish.
            while (std::optional<std::function<void()>> task = tasks_.pop()) {
                (*task)();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    tasks_.close();
    for (std::thread& worker : workers_) {
        worker.join();
    }
}

void ThreadPool::submit(std::function<void()> task) { tasks_.push(std::move(task)); }

void ThreadPool::parallel_for(std::size_t count, const std::function<void(std::size_t)>& body) {
    // std::latch (C++20) is a one-time countdown: each task counts down once, and wait()
    // blocks until the count reaches zero. It also makes everything the tasks wrote visible
    // to this thread afterwards, so the caller can read the results without another lock.
    std::latch done(static_cast<std::ptrdiff_t>(count));
    for (std::size_t i = 0; i < count; ++i) {
        // body and done live on this stack frame. That is safe because we wait below and
        // do not return until every task has finished with them.
        submit([&body, &done, i] {
            body(i);
            done.count_down();
        });
    }
    done.wait();
}

}  // namespace ai
