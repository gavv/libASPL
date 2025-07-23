// Copyright (c) libASPL authors
// Licensed under MIT

#include <aspl/util/Semaphore.hpp>

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <thread>

namespace {

constexpr auto ShortTimeout = std::chrono::milliseconds(10);
constexpr auto LongTimeout = std::chrono::milliseconds(1000);

} // anonymous namespace

struct SemaphoreTest : testing::Test
{
};

TEST_F(SemaphoreTest, TryWait)
{
    { // zero counter
        aspl::util::Semaphore sem;

        EXPECT_FALSE(sem.TryWait());
    }

    { // non-zero counter
        aspl::util::Semaphore sem(3);

        EXPECT_TRUE(sem.TryWait());
        EXPECT_TRUE(sem.TryWait());
        EXPECT_TRUE(sem.TryWait());

        EXPECT_FALSE(sem.TryWait());
    }
}

TEST_F(SemaphoreTest, Post_TryWait)
{
    { // one
        aspl::util::Semaphore sem;

        sem.Post();
        EXPECT_TRUE(sem.TryWait());

        EXPECT_FALSE(sem.TryWait());
    }

    { // many
        aspl::util::Semaphore sem;

        sem.Post();
        sem.Post();
        sem.Post();

        EXPECT_TRUE(sem.TryWait());
        EXPECT_TRUE(sem.TryWait());
        EXPECT_TRUE(sem.TryWait());

        EXPECT_FALSE(sem.TryWait());
    }
}

TEST_F(SemaphoreTest, TimedWait)
{
    { // timeout
        aspl::util::Semaphore sem;

        auto deadline = std::chrono::steady_clock::now() + ShortTimeout;
        auto start = std::chrono::steady_clock::now();

        EXPECT_FALSE(sem.TimedWait(deadline));

        auto elapsed = std::chrono::steady_clock::now() - start;
        EXPECT_GE(elapsed, ShortTimeout);
    }

    { // success
        aspl::util::Semaphore sem;

        sem.Post();
        auto deadline = std::chrono::steady_clock::now() + LongTimeout;
        auto start = std::chrono::steady_clock::now();

        EXPECT_TRUE(sem.TimedWait(deadline));

        auto elapsed = std::chrono::steady_clock::now() - start;
        EXPECT_LT(elapsed, LongTimeout);
    }
}

TEST_F(SemaphoreTest, DeadlineInPast)
{
    aspl::util::Semaphore sem;

    {
        // deadline in the past
        auto deadline = std::chrono::steady_clock::now() - LongTimeout;

        EXPECT_FALSE(sem.TimedWait(deadline));
    }

    sem.Post();

    {
        // same, but non-zero counter
        auto deadline = std::chrono::steady_clock::now() - LongTimeout;

        EXPECT_TRUE(sem.TimedWait(deadline));
    }
}

TEST_F(SemaphoreTest, Wait_Post)
{
    { // instant
        aspl::util::Semaphore sem;

        sem.Post();
        sem.Wait();

        sem.Post();
        sem.Post();

        sem.Wait();
        sem.Wait();
    }

    { // one
        aspl::util::Semaphore sem;

        std::thread waiter([&]() {
            sem.Wait();
        });

        std::this_thread::sleep_for(ShortTimeout);

        sem.Post();
        waiter.join();
    }

    { // many
        aspl::util::Semaphore sem;

        std::thread worker([&]() {
            sem.Wait();
            sem.Wait();
            sem.Wait();
        });

        std::this_thread::sleep_for(ShortTimeout);
        sem.Post();

        std::this_thread::sleep_for(ShortTimeout);
        sem.Post();

        std::this_thread::sleep_for(ShortTimeout);
        sem.Post();

        worker.join();
    }
}

TEST_F(SemaphoreTest, Close_TryWait)
{
    { // zero counter
        aspl::util::Semaphore sem;

        EXPECT_FALSE(sem.IsClosed());
        EXPECT_FALSE(sem.TryWait());

        sem.Close();

        EXPECT_TRUE(sem.IsClosed());
        EXPECT_FALSE(sem.TryWait());
    }

    { // non-zero counter
        aspl::util::Semaphore sem(99);

        EXPECT_FALSE(sem.IsClosed());
        EXPECT_TRUE(sem.TryWait());

        sem.Close();

        EXPECT_TRUE(sem.IsClosed());
        EXPECT_FALSE(sem.TryWait());
    }
}

TEST_F(SemaphoreTest, Close_BlockingWait)
{
    { // Wait
        aspl::util::Semaphore sem;

        std::atomic<bool> waitResult{true};
        std::thread waiter([&]() {
            waitResult = sem.Wait();
        });

        std::this_thread::sleep_for(ShortTimeout);

        sem.Close();
        waiter.join();

        EXPECT_FALSE(waitResult);
    }

    { // TimedWait
        aspl::util::Semaphore sem;

        auto deadline = std::chrono::steady_clock::now() + LongTimeout;
        auto start = std::chrono::steady_clock::now();

        std::atomic<bool> waitResult{true};
        std::thread waiter([&]() {
            waitResult = sem.TimedWait(deadline);
        });

        std::this_thread::sleep_for(ShortTimeout);

        sem.Close();
        waiter.join();

        auto elapsed = std::chrono::steady_clock::now() - start;
        EXPECT_FALSE(waitResult);
        EXPECT_LT(elapsed, LongTimeout);
    }
}

TEST_F(SemaphoreTest, MultipleWaiters_OnePost)
{
    static constexpr size_t WaitersCount = 50;

    aspl::util::Semaphore sem;

    std::array<std::atomic<bool>, WaitersCount> waitCompleted{};
    std::vector<std::thread> waiters;

    for (size_t i = 0; i < WaitersCount; ++i) {
        waiters.emplace_back([&, i]() {
            sem.Wait();
            waitCompleted[i] = true;
        });
    }

    std::this_thread::sleep_for(ShortTimeout);

    for (size_t expectedCompleted = 1; expectedCompleted <= WaitersCount;
         ++expectedCompleted) {
        sem.Post();

        while (true) {
            size_t completedCount = 0;
            for (const auto& completed : waitCompleted) {
                if (completed.load()) {
                    completedCount++;
                }
            }

            if (completedCount == expectedCompleted) {
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    for (auto& waiter : waiters) {
        waiter.join();
    }
}

TEST_F(SemaphoreTest, MultipleWaiters_MultiplePosts)
{
    static constexpr size_t WaitersCount = 50;

    aspl::util::Semaphore sem;

    std::array<std::atomic<bool>, WaitersCount> waitCompleted{};
    std::vector<std::thread> waiters;

    for (size_t i = 0; i < WaitersCount; ++i) {
        waiters.emplace_back([&, i]() {
            sem.Wait();
            waitCompleted[i] = true;
        });
    }

    std::this_thread::sleep_for(ShortTimeout);

    for (size_t i = 0; i < WaitersCount; ++i) {
        sem.Post();
    }

    for (auto& waiter : waiters) {
        waiter.join();
    }

    for (bool completed : waitCompleted) {
        EXPECT_TRUE(completed);
    }
}
