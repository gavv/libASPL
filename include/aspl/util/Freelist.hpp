// Copyright (c) libASPL authors
// Licensed under MIT

// Copyright (c) 2014 Cameron Desrochers
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS”
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

//! @file aspl/util/Freelist.hpp
//! @brief Lock-free free-list.

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <type_traits>

namespace aspl::util {

//! Base class for freelist elements.
//! Objects must inherit this class to be a member of freelist.
class FreelistNode
{
public:
    FreelistNode() = default;

    FreelistNode(const FreelistNode&) = delete;
    FreelistNode& operator=(const FreelistNode&) = delete;

private:
    template <class T>
    friend class Freelist;

    mutable std::atomic<FreelistNode*> next_ = nullptr;
    mutable std::atomic<uint32_t> refs_ = 0;
};

//! A simple CAS-based lock-free ABA-free freelist.
//!
//! @tparam T defines element type, arbitrary type derived from FreelistNode.
//!
//! Freelist does not take ownership of the elements and does not allocate.
//! The user is responsible for both allocating and deallocating.
//!
//! Based on the article by Cameron Desrochers:
//! https://moodycamel.com/blog/2014/solving-the-aba-problem-for-lock-free-free-lists.htm
//!
//! Comments from the article are preserved as well.
template <class T = FreelistNode>
class Freelist
{
    static_assert(std::is_base_of_v<FreelistNode, T>, "T must inherit from FreelistNode");

public:
    //! Initialize empty freelist.
    Freelist()
        : head_(nullptr)
    {
    }

    Freelist(const Freelist&) = delete;
    Freelist& operator=(const Freelist&) = delete;

    //! Get a node from the freelist.
    //! Returns NULL if freelist is empty.
    //! Thread-safe, non-blocking.
    T* GetNode()
    {
        FreelistNode* currHead = head_.load(std::memory_order_acquire);

        while (currHead != nullptr) {
            FreelistNode* prevHead = currHead;

            uint32_t refs = currHead->refs_.load(std::memory_order_relaxed);

            if ((refs & RefsMask) == 0 ||
                !currHead->refs_.compare_exchange_strong(
                    refs, refs + 1, std::memory_order_acquire)) {
                currHead = head_.load(std::memory_order_acquire);
                continue;
            }

            // Good, reference count has been incremented (it wasn't at zero), which means
            // we can read the next and not worry about it changing between now and the
            // time we do the CAS
            FreelistNode* next = currHead->next_.load(std::memory_order_relaxed);

            if (head_.compare_exchange_strong(currHead,
                    next,
                    std::memory_order_acquire,
                    std::memory_order_relaxed)) {
                // Yay, got the node. This means it was on the list, which means
                // shouldBeOnFreeList must be false no matter the refcount (because
                // nobody else knows it's been taken off yet, it can't have been put
                // back on).

                // Decrease refcount twice, once for our ref, and once for the list's ref
                currHead->refs_.fetch_sub(2u, std::memory_order_relaxed);

                return static_cast<T*>(currHead);
            }

            // OK, the head must have changed on us, but we still need to decrease the
            // refcount we increased
            if (prevHead->refs_.fetch_sub(1u, std::memory_order_acq_rel) ==
                PendFlag + 1) {
                AddUnrefedNode(prevHead);
            }
        }

        return nullptr;
    }

    //! Put a node to the freelist.
    //! Assumes nobody is modifying the node concurrently.
    //! Thread-safe, non-blocking.
    void PutNode(T* node)
    {
        FreelistNode* nodePtr = static_cast<FreelistNode*>(node);

        nodePtr->next_.store(nullptr, std::memory_order_relaxed);
        nodePtr->refs_.store(PendFlag, std::memory_order_release);

        AddUnrefedNode(nodePtr);
    }

private:
    static constexpr uint32_t PendFlag = 0x80000000;
    static constexpr uint32_t RefsMask = 0x7FFFFFFF;

    // Add node knowing that it is not part of a freelist.
    void AddUnrefedNode(FreelistNode* node)
    {
        // Since the refcount is zero, and nobody can increase it once it's zero (except
        // us, and we run only one copy of this method per node at a time, i.e. the single
        // thread case), then we know we can safely change the next pointer of the node;
        // however, once the refcount is back above zero, then other threads could
        // increase it (happens under heavy contention, when the refcount goes to zero in
        // between a load and a refcount increment of a node in try_get, then back up to
        // something non-zero, then the refcount increment is done by the other thread) --
        // so, if the CAS to add the node to the actual list fails, decrease the refcount
        // and leave the add operation to the next thread who puts the refcount back at
        // zero (which could be us, hence the loop).
        FreelistNode* currHead = head_.load(std::memory_order_relaxed);

        while (true) {
            node->next_.store(currHead, std::memory_order_relaxed);
            node->refs_.store(1u, std::memory_order_release);

            if (!head_.compare_exchange_strong(currHead,
                    node,
                    std::memory_order_release,
                    std::memory_order_relaxed)) {
                // Hmm, the add failed, but we can only try again when the refcount goes
                // back to zero
                if (node->refs_.fetch_add(PendFlag - 1, std::memory_order_acq_rel) == 1) {
                    continue;
                }
            }
            return;
        }
    }

    std::atomic<FreelistNode*> head_;
};

} // namespace aspl::util
