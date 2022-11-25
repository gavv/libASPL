#include <aspl/DoubleBuffer.hpp>

#include <atomic>
#include <future>
#include <optional>
#include <random>
#include <thread>

#include <gtest/gtest.h>

namespace {

void RandomDelay(float percent)
{
    static thread_local std::random_device device;
    static thread_local std::mt19937 gen(device());

    std::uniform_int_distribution<> dist(0, 100);

    if (dist(gen) < int(percent * 100)) {
        std::this_thread::sleep_for(std::chrono::nanoseconds(300));
    }
}

} // anonymous namespace

struct DoubleBufferTest : ::testing::Test
{
};

TEST_F(DoubleBufferTest, InitGet)
{
    {
        aspl::DoubleBuffer<int> buf;
        ASSERT_EQ(buf.Get(), 0);
    }
    {
        aspl::DoubleBuffer<int> buf(123);
        ASSERT_EQ(buf.Get(), 123);
    }
    {
        aspl::DoubleBuffer<int> buf(123);
        for (int i = 0; i < 10; i++) {
            ASSERT_EQ(buf.Get(), 123);
        }
    }
}

TEST_F(DoubleBufferTest, SetGet)
{
    aspl::DoubleBuffer<int> buf(123);

    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(buf.Get(), 123);
    }

    buf.Set(456);

    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(buf.Get(), 456);
    }
}

TEST_F(DoubleBufferTest, ReadLock)
{
    aspl::DoubleBuffer<int> buf(123);

    for (int i = 0; i < 10; i++) {
        auto rdLock = buf.GetReadLock();
        ASSERT_EQ(rdLock.GetReference(), 123);
    }

    buf.Set(456);

    for (int i = 0; i < 10; i++) {
        auto rdLock = buf.GetReadLock();
        ASSERT_EQ(rdLock.GetReference(), 456);
    }
}

TEST_F(DoubleBufferTest, Squash)
{
    aspl::DoubleBuffer<int> buf;

    for (int i = 0; i < 10; i++) {
        buf.Set(i);
    }

    ASSERT_EQ(buf.Get(), 9);
}

TEST_F(DoubleBufferTest, Concurrent)
{
    enum
    {
        MaxVal = 10000
    };

    aspl::DoubleBuffer<int> buf;

    std::atomic<int> wrVal = 0;
    int rdVal = 0;

    auto writer = std::async(std::launch::async, [&]() {
        for (int i = 0; i < MaxVal; i++) {
            buf.Set(++wrVal);
            RandomDelay(0.05f);
        }
    });

    while (rdVal != MaxVal) {
        RandomDelay(0.2f);

        const int newRdVal = buf.Get();

        ASSERT_GE(newRdVal, rdVal);
        ASSERT_LE(newRdVal, MaxVal);

        rdVal = newRdVal;

        ASSERT_LE(rdVal, wrVal);
    }

    writer.wait();
}

TEST_F(DoubleBufferTest, ConcurrentReadLock)
{
    enum
    {
        MaxVal = 10000
    };

    aspl::DoubleBuffer<int> buf;

    std::atomic<int> wrVal = 0;
    int rdVal = 0;

    auto writer = std::async(std::launch::async, [&]() {
        for (int i = 0; i < MaxVal; i++) {
            buf.Set(++wrVal);
            RandomDelay(0.05f);
        }
    });

    while (rdVal != MaxVal) {
        auto rdLock = buf.GetReadLock();

        RandomDelay(0.2f);

        const int newRdVal = rdLock.GetReference();

        ASSERT_TRUE(newRdVal >= rdVal);
        ASSERT_TRUE(newRdVal <= MaxVal);

        rdVal = newRdVal;

        ASSERT_TRUE(rdVal <= wrVal);
    }

    writer.wait();
}

TEST_F(DoubleBufferTest, ConcurrentNonTrivial)
{
    enum
    {
        MaxVal = 10000
    };

    // DoubleBuffer has special handling for non-trivial objects - it makes sure
    // that Set() invokes destructor of the previous value. This test covers
    // races in corresponding piece of code.
    aspl::DoubleBuffer<std::optional<int>> buf(0);

    std::atomic<int> wrVal = 0;
    int rdVal = 0;

    auto writer = std::async(std::launch::async, [&]() {
        for (int i = 0; i < MaxVal; i++) {
            buf.Set(++wrVal);
            RandomDelay(0.05f);
        }
    });

    while (rdVal != MaxVal) {
        RandomDelay(0.2f);

        const auto newRdVal = buf.Get();

        ASSERT_GE(*newRdVal, rdVal);
        ASSERT_LE(*newRdVal, MaxVal);

        rdVal = *newRdVal;

        ASSERT_LE(rdVal, wrVal);
    }

    writer.wait();
}
