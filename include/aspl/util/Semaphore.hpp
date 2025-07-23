// Copyright (c) libASPL authors
// Licensed under MIT

//! @file aspl/util/Semaphore.hpp
//! @brief Semaphore.

#pragma once

#include <mach/semaphore.h>

#include <atomic>
#include <chrono>
#include <cstdint>

namespace aspl::util {

//! Semaphore with amortized lock-free post.
//!
//! Provides sequential consistency (SEQ_CST).
//!
//! Wraps Mach semaphore to avoid syscalls when possible.
//! The following operations are lock-free:
//!  - posting to a semaphore with no active waiters
//!  - waiting on a semaphore with non-zero counter
//!  - try-wait
//!
//! In the rest cases, a syscall to Mach semaphore is issued.
//! Even so, it is still guaranteed that the poster thread would not be blocked
//! by a stuck waiter thread.
//!
//! (Post operation may need to wait until concurrent syscall releases kernel-side lock of
//! the Mach semaphore, but since the lock is acquired with preemption disabled, the
//! waiting is guaranteed to complete within limited time).
//!
//! Internally, user-space semaphore state is represented as a single 64-bit atomic:
//!
//!   +-------------------------------+-------------------------------+
//!   |       PERMITS (40 bits)       |       WAITERS (24 bits)       |
//!   +-------------------------------+-------------------------------+
//!
//! PERMITS is number of tokens granted by Post() and not consumed yet
//! WAITERS is number of pending Wait() calls that need syscall notification
//!
//! Implementation assumes that overflows can't happen, i.e. it doesn't allow
//! more than 2^40 unwaited tokens and more than 2^24 concurrent waiters.
//! If this is violated, expect event losses.
class Semaphore
{
public:
    //! Initialize semaphore with specified initial counter.
    explicit Semaphore(unsigned counter = 0);

    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;

    ~Semaphore();

    //! Check if Close() was called.
    bool IsClosed();

    //! Close semaphore.
    //! All pending and future calls unblock and fail immediately.
    void Close();

    //! Increment counter and wake up blocked waits.
    void Post();

    //! Wait until the counter becomes non-zero and decrement it.
    //! Returns false if semaphore is closed.
    bool Wait();

    //! Try to decrement counter without blocking.
    //! Returns true if counter was decremented, false if counter is zero,
    //! or semaphore is closed.
    bool TryWait();

    //! Wait until the counter becomes non-zero and decrement it.
    //! If deadline expires, return early.
    //! Returns true if counter was decremented, false if deadline expired,
    //! or semaphore is closed.
    bool TimedWait(std::chrono::steady_clock::time_point deadline);

private:
    bool BlockingWait(std::chrono::steady_clock::time_point* deadline);

    void NotifyOne();
    void NotifyAll();
    bool WaitNotified(int64_t timeoutNs);

    std::atomic<uint64_t> state_;
    semaphore_t handle_;
};

} // namespace aspl::util
