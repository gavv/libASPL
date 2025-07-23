// Copyright (c) libASPL authors
// Licensed under MIT

#include <aspl/util/Semaphore.hpp>

#include <mach/mach.h>
#include <mach/sync_policy.h>

namespace aspl::util {

namespace {

// PERMITS field (high 40 bits)

static constexpr uint64_t PermitsShift = 24;
static constexpr uint64_t PermitsMask = 0xFFFFFFFFFF;

inline uint64_t GetPermits(uint64_t state)
{
    return state >> PermitsShift;
}

inline uint64_t IncPermits(uint64_t state)
{
    if ((state >> PermitsShift) < PermitsMask) {
        return state + (uint64_t(1) << PermitsShift);
    }
    return state;
}

inline uint64_t DecPermits(uint64_t state)
{
    if ((state >> PermitsShift) > 0) {
        return state - (uint64_t(1) << PermitsShift);
    }
    return state;
}

// WAITERS field (low 24 bits)

static constexpr uint64_t WaitersShift = 0;
static constexpr uint64_t WaitersMask = 0xFFFFFF;

inline uint64_t GetWaiters(uint64_t state)
{
    return state & WaitersMask;
}

inline uint64_t IncWaiters(uint64_t state)
{
    if ((state & WaitersMask) < WaitersMask) {
        return state + 1;
    }
    return state;
}

inline uint64_t DecWaiters(uint64_t state)
{
    if ((state & WaitersMask) > 0) {
        return state - 1;
    }
    return state;
}

// CLOSED mask

static constexpr uint64_t ClosedMask = 0xFFFFFFFFFFFFFFFF;

inline bool GetIsClosed(uint64_t state)
{
    return state == ClosedMask;
}

} // namespace

Semaphore::Semaphore(unsigned counter)
{
    state_ = uint64_t(counter) << PermitsShift;
    semaphore_create(mach_task_self(), &handle_, SYNC_POLICY_FIFO, 0);
}

Semaphore::~Semaphore()
{
    semaphore_destroy(mach_task_self(), handle_);
}

bool Semaphore::IsClosed()
{
    uint64_t state = state_.load(std::memory_order_seq_cst);

    return GetIsClosed(state);
}

void Semaphore::Close()
{
    // All CAS loops will detected CLOSED state and abort.
    state_.store(ClosedMask, std::memory_order_seq_cst);

    NotifyAll();
}

void Semaphore::Post()
{
    uint64_t oldState = state_.load(std::memory_order_relaxed);
    uint64_t newState;

    // Atomic increment with saturation (see IncPermits).
    do {
        if (GetIsClosed(oldState)) {
            return;
        }
        newState = IncPermits(oldState);
    }
    while (!state_.compare_exchange_weak(
        oldState, newState, std::memory_order_seq_cst, std::memory_order_relaxed));

    // We've incremented PERMITS count.
    //
    // If WAITERS count was zero, no notification is needed for this permit, because
    // any new waiter will check PERMITS count before deciding to sleep.
    //
    // If WAITERS count was non-zero, we must notify one waiter, because we've
    // added one permit.
    //
    // (On contention, we may produce some unnecessary notifications, but we would
    // never loss a notification, unless PERMITS count saturates).

    if (GetWaiters(oldState) > 0) {
        NotifyOne();
    }
}

bool Semaphore::Wait()
{
    return BlockingWait(nullptr);
}

bool Semaphore::TimedWait(std::chrono::steady_clock::time_point deadline)
{
    return BlockingWait(&deadline);
}

bool Semaphore::TryWait()
{
    uint64_t oldState = state_.load(std::memory_order_relaxed);
    uint64_t newState;

    for (;;) {
        if (GetIsClosed(oldState)) {
            return false;
        }

        if (GetPermits(oldState) == 0) {
            // No permits, task failed successfully.
            return false;
        }

        newState = DecPermits(oldState);

        if (state_.compare_exchange_weak(oldState,
                newState,
                std::memory_order_seq_cst,
                std::memory_order_relaxed)) {
            // Was able to acquire a permit.
            return true;
        }

        // Contention.
        // Need to repeat until success or failure.
    }
}

bool Semaphore::BlockingWait(std::chrono::steady_clock::time_point* deadline)
{
    uint64_t oldState = state_.load(std::memory_order_relaxed);
    uint64_t newState;

    // Either decrement PERMITS count, if non-zero, or increment WAITERS
    // count otherwise.
    do {
        if (GetIsClosed(oldState)) {
            return false;
        }

        if (GetPermits(oldState) > 0) {
            newState = DecPermits(oldState);
        } else {
            newState = IncWaiters(oldState);
        }
    }
    while (!state_.compare_exchange_weak(
        oldState, newState, std::memory_order_seq_cst, std::memory_order_relaxed));

    if (GetPermits(oldState) > 0) {
        // We've decremented PERMITS count, no need to sleep.
        return true;
    }

    // Enter wait loop.
    for (;;) {
        if (GetIsClosed(oldState)) {
            return false;
        }

        if (GetPermits(oldState) > 0) {
            // After a sleep, there are some permits available.
            // Try to decrement both PERMITS and WAITERS counts atomically.
            newState = oldState;
            newState = DecPermits(newState);
            newState = DecWaiters(newState);

            if (state_.compare_exchange_weak(oldState,
                    newState,
                    std::memory_order_seq_cst,
                    std::memory_order_relaxed)) {
                // Success, permit granted.
                return true;
            }

            // Contention, repeat.
            continue;
        }

        // No permits available, need to sleep.
        //
        // At this point, we saw zero PERMITS count, and we've incremented WAITERS
        // count before or in the same operation when we've read PERMITS.
        //
        // It means that when Post() will increment PERMITS, it is guaranteed to
        // see non-zero WAITERS count and produce a notification.
        if (deadline) {
            // Recalculate relative timeout from absolute deadline.
            const int64_t timeoutNs =
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    *deadline - std::chrono::steady_clock::now())
                    .count();

            if (timeoutNs <= 0 || !WaitNotified(timeoutNs)) {
                // Timeout already expired, or expired during wait, or wait errored.
                break;
            }
        } else {
            if (!WaitNotified(-1)) {
                // Wait errored.
                break;
            }
        }

        // If we're here, wait either succeeded or was interrupted.
        // In both cases, we need to re-read state and repeat.
        oldState = state_.load(std::memory_order_relaxed);
    }

    // Timeout expired or wait errored.
    // Decrement WAITERS count and return.
    oldState = state_.load(std::memory_order_relaxed);
    do {
        if (GetIsClosed(oldState)) {
            return false;
        }

        newState = DecWaiters(oldState);
    }
    while (!state_.compare_exchange_weak(
        oldState, newState, std::memory_order_seq_cst, std::memory_order_relaxed));

    return false;
}

void Semaphore::NotifyOne()
{
    semaphore_signal(handle_);
}

void Semaphore::NotifyAll()
{
    semaphore_signal_all(handle_);
}

bool Semaphore::WaitNotified(int64_t timeoutNs)
{
    kern_return_t err;

    if (timeoutNs < 0) {
        err = semaphore_wait(handle_);
    } else {
        mach_timespec_t ts;
        ts.tv_sec = static_cast<unsigned>(timeoutNs / 1000000000LL);
        ts.tv_nsec = static_cast<clock_res_t>(timeoutNs % 1000000000LL);

        err = semaphore_timedwait(handle_, ts);
    }

    if (err == KERN_SUCCESS || err == KERN_ABORTED) {
        // Signaled (KERN_SUCCESS) or interrupted (KERN_ABORTED), need to repeat loop.
        return true;
    }

    // Timed out or errored, need to break loop.
    return false;
}

} // namespace aspl::util
