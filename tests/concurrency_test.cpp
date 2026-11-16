#include <gtest/gtest.h>

#include <thread>
#include <vector>

#include "net/mailbox.hpp"
#include "net/thread_safe_queue.hpp"

// These tests start real threads, so the ThreadSanitizer CI job now checks something:
// a missing lock in the queue or mailbox would show up there as a data race.

TEST(ThreadSafeQueue, IsFirstInFirstOut) {
    net::ThreadSafeQueue<int> queue;
    queue.push(1);
    queue.push(2);
    EXPECT_EQ(queue.try_pop(), 1);
    EXPECT_EQ(queue.try_pop(), 2);
    EXPECT_EQ(queue.try_pop(), std::nullopt);
}

TEST(ThreadSafeQueue, ManyProducersLoseNothing) {
    constexpr int kProducers = 4;
    constexpr int kItemsEach = 1000;
    net::ThreadSafeQueue<int> queue;

    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&queue] {
            for (int i = 1; i <= kItemsEach; ++i) {
                queue.push(i);
            }
        });
    }

    // The consumer runs at the same time as the producers, using the blocking pop().
    long long sum = 0;
    for (int n = 0; n < kProducers * kItemsEach; ++n) {
        sum += queue.pop().value();
    }
    for (std::thread& t : producers) {
        t.join();
    }

    const long long one_producer = kItemsEach * (kItemsEach + 1LL) / 2;  // 1 + 2 + ... + 1000
    EXPECT_EQ(sum, kProducers * one_producer);
    EXPECT_EQ(queue.try_pop(), std::nullopt);
}

TEST(ThreadSafeQueue, CloseWakesAWaitingConsumer) {
    net::ThreadSafeQueue<int> queue;
    std::optional<int> result = 42;
    std::thread consumer([&] { result = queue.pop(); });  // blocks: the queue is empty
    queue.close();
    consumer.join();
    EXPECT_EQ(result, std::nullopt);
}

TEST(ThreadSafeQueue, ItemsPushedBeforeCloseAreStillDelivered) {
    net::ThreadSafeQueue<int> queue;
    queue.push(7);
    queue.close();
    EXPECT_EQ(queue.pop(), 7);
    EXPECT_EQ(queue.pop(), std::nullopt);
}

TEST(Mailbox, KeepsOnlyTheLatestValue) {
    net::Mailbox<int> box;
    box.put(1);
    box.put(2);
    box.put(3);
    EXPECT_EQ(box.take(), 3);
}

TEST(Mailbox, DeliversAcrossThreadsAndStopsOnClose) {
    net::Mailbox<int> box;
    std::vector<int> seen;
    std::thread reader([&] {
        while (std::optional<int> value = box.take()) {
            seen.push_back(*value);
        }
    });

    for (int i = 1; i <= 100; ++i) {
        box.put(i);
    }
    box.close();
    reader.join();

    // The reader may miss values that were overwritten, but it sees them in order and
    // always gets the last one, because a value put before close() is still delivered.
    ASSERT_FALSE(seen.empty());
    EXPECT_EQ(seen.back(), 100);
    for (std::size_t i = 1; i < seen.size(); ++i) {
        EXPECT_LT(seen[i - 1], seen[i]);
    }
}
