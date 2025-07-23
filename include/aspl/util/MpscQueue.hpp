// Copyright (c) libASPL authors
// Licensed under MIT

// Copyright (c) 2010-2011 Dmitry Vyukov. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    1. Redistributions of source code must retain the above copyright notice,
//       this list of conditions and the following disclaimer.
//
//    2. Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other material provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY DMITRY VYUKOV "AS IS" AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL DMITRY VYUKOV OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
// OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
// EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// The views and conclusions contained in the software and documentation are
// those of the authors and should not be interpreted as representing official
// policies, either expressed or implied, of Dmitry Vyukov.

//! @file aspl/util/MpscQueue.hpp
//! @brief Lock-free queue.

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <type_traits>

namespace aspl::util {

//! Base class for MPSC queue elements.
//! Objects must inherit this class to be a member of MPSC queue.
class MpscNode
{
public:
    MpscNode() = default;

    MpscNode(const MpscNode&) = delete;
    MpscNode& operator=(const MpscNode&) = delete;

    virtual ~MpscNode() = default;

private:
    template <class T>
    friend class MpscQueue;

    mutable std::atomic<MpscNode*> next_ = nullptr;
    mutable std::shared_ptr<MpscNode> self_ = nullptr;
};

//! Lock-free multi-producer single-consumer queue.
//!
//! Provides sequential consistency (SEQ_CST).
//!
//! Based on Dmitry Vyukov algorithm:
//!  https://www.1024cores.net/home/lock-free-algorithms/queues/intrusive-mpsc-node-based-queue
//!  https://int08h.com/post/ode-to-a-vyukov-queue
//!
//! @tparam T defines object type, it must inherit MpscNode.
template <class T = MpscNode>
class MpscQueue
{
    static_assert(std::is_base_of_v<MpscNode, T>, "T must inherit from MpscNode");

public:
    //! Initialize empty queue.
    MpscQueue()
        : tail_(&stub_)
        , head_(&stub_)
    {
    }

    //! Destructor queue and owned nodes.
    //! Releases shared pointers of all nodes that are still in queue.
    ~MpscQueue()
    {
        MpscNode* curr = head_.load(std::memory_order_relaxed);
        while (curr && curr != &stub_) {
            MpscNode* next = curr->next_.load(std::memory_order_relaxed);

            // deletes head if this was the last reference
            auto selfPtr = std::static_pointer_cast<T>(curr->self_);
            curr->self_.reset();
            selfPtr.reset(); // if we call destructor, do it via shared_ptr<T>

            curr = next;
        }
    }

    MpscQueue(const MpscQueue&) = delete;
    MpscQueue& operator=(const MpscQueue&) = delete;

    //! Append object to the queue.
    //! Can be called concurrently.
    //!
    //! shared_ptr ownership is transferred to the queue. PopNode() will return
    //! ownership to the user. Destructor will release all unpopped nodes.
    //!
    //! After this call returns, any thread calling PopNode() is guaranteed to see
    //! a non-empty queue.
    //!
    //! @note
    //!  - On CPUs with atomic exchange, e.g. x86, this operation is both lock-free
    //!    and wait-free, i.e. it never waits for sleeping threads and never spins.
    //!  - On CPUs without atomic exchange, e.g. arm64, this operation is lock-free,
    //!    but not wait-free, i.e. it is not blocked by a stuck consumer, but
    //!    it may spin until consumer returns from PopNode() or is stuck inside
    //!    PopNode call (e.g. thread preempted).
    //!  - Concurrent PopNode() calls do not affect this operation.
    //!    Only concurrent PushNode() calls can make it spin.
    void PushNode(std::shared_ptr<T> node)
    {
        if (!node) {
            return;
        }

        MpscNode* nodePtr = static_cast<MpscNode*>(node.get());

        // transfer ownership to queue
        nodePtr->self_ = std::static_pointer_cast<MpscNode>(std::move(node));

        nodePtr->next_.store(nullptr, std::memory_order_relaxed);
        MpscNode* prev = tail_.exchange(nodePtr, std::memory_order_seq_cst);
        prev->next_.store(nodePtr, std::memory_order_release);
    }

    //! Fetch object from the beginning of the queue.
    //! Not intended to be called concurrently.
    //!
    //! shared_ptr ownership is transferred from the queue back to the user.
    //!
    //! Multiple concurrent calls to PopNode() are not intended. If another call is
    //! in progress, PopNode() will block. Concurrent PushNode() and PopNode() calls are
    //! intended and won't block each other.
    //!
    //! Returns nullptr if the queue is empty.
    //!
    //! @note
    //!  - With single consumer, this operation is lock-free on all architectures, i.e.
    //!    it is not blocked by a stuck producer(s).
    //!  - This operation is not wait-free however, it may spin until producer(s)
    //!    returns from PushNode() or is stuck inside PushNode call (e.g. thread
    //!    preempted).
    //!  - Multiple consumers are not intended because this is a single-consumer queue.
    //!    Mutex will protect from data races, but lock-free guarantee will be lost.
    std::shared_ptr<T> PopNode()
    {
        std::lock_guard lock(popMutex_);

        MpscNode* head = head_.load(std::memory_order_relaxed);
        MpscNode* next = head->next_.load(std::memory_order_acquire);

        if (head == &stub_) {
            if (!next) {
                if (tail_.load(std::memory_order_seq_cst) == head) {
                    // queue is empty
                    return nullptr;
                } else {
                    // queue is not empty, so head->next == nullptr means that
                    // a PushNode() call is in progress
                    next = WaitNext(head);
                    if (!next) {
                        return nullptr;
                    }
                }
            }
            // remove stub from the beginning of the list
            head_.store(next, std::memory_order_relaxed);
            head = next;
            next = next->next_.load(std::memory_order_acquire);
        }

        if (!next) {
            // head is not stub and head->next == nullptr

            if (tail_.load(std::memory_order_seq_cst) == head) {
                // queue is empty
                // add stub to the end of the list to ensure that we always
                // have head->next when removing head and head won't become nullptr
                PushStub();
            }

            // if head->next == nullptr here means that a PushNode() call is in progress
            next = WaitNext(head);
            if (!next) {
                return nullptr;
            }
        }

        // move list head to the next node
        head_.store(next, std::memory_order_relaxed);

        // transfer ownership back to user
        auto headPtr = std::move(head->self_);
        return std::static_pointer_cast<T>(std::move(headPtr));
    }

private:
    // Append stub to the end of the queue.
    void PushStub()
    {
        stub_.next_.store(nullptr, std::memory_order_relaxed);
        MpscNode* prev = tail_.exchange(&stub_, std::memory_order_seq_cst);
        prev->next_.store(&stub_, std::memory_order_release);
    }

    // Wait until concurrent PushNode() completes and node->next becomes non-null.
    MpscNode* WaitNext(MpscNode* node)
    {
        for (;;) {
            if (MpscNode* next = node->next_.load(std::memory_order_seq_cst)) {
                return next;
            }
            CpuRelax();
        }
    }

    // CPU hint that we're busy-waiting.
    static inline void CpuRelax()
    {
#if defined(__x86_64__)
        __asm__ __volatile__("pause" ::: "memory");
#elif defined(__aarch64__)
        __asm__ __volatile__("yield" ::: "memory");
#else
        // no-op
#endif
    }

    std::atomic<MpscNode*> tail_;
    std::atomic<MpscNode*> head_;
    MpscNode stub_;

    mutable std::mutex popMutex_;
};

} // namespace aspl::util
