// Copyright (c) libASPL authors
// Licensed under MIT

//! @file aspl/util/ObjectPool.hpp
//! @brief Lock-free object pool.

#pragma once

#include <aspl/util/Freelist.hpp>

#include <map>
#include <memory>
#include <shared_mutex>
#include <tuple>
#include <type_traits>

namespace aspl::util {

//! Object pool with amortized lock-free allocations.
//!
//! @tparam T defines object type.
//! @tparam Allocator is used to allocate memory when cache is empty.
//!
//! ObjectPool manages an auto-growing cache of reusable memory chunks. When the cache
//! is non-empty, allocations are lock-free (i.e. threads won't block each other).
//!
//! Note that if custom @tparam Allocator is used, it must support rebinding
//! (via std::allocator_traits::rebind_traits).
//!
//! Under the hood, a rebound copy of the allocator is used, which usually means:
//!  - instead of Allocator<T>, another allocator type is selected, usually Allocator<X>
//!    (see rebind_traits for details)
//!  - a "rebound copy" of an allocator is created by instantiating Allocator<X> and
//!    passing original Allocator<T> as an argument to its constructor
//!  - here, U is some implementation-defined type, usually larger than T
//!
//! Such design choice is dictated by std::allocate_shared(). It allows us to use
//! std::shared_ptr<T> while managing allocations. Internally, std::allocate_shared()
//! rebounds allocator to some hidden type that contains both control block and T.
//!
//! Good news: if you use Allocator = MyAllocator<T>, defined as:
//! @code
//!   template <class T [, ...whatever...]> class MyAllocator {
//!   public:
//!     template <class U> MyAllocator(const MyAllocator<X>& other) { ... }
//!     ...
//!   };
//! @endcode
//!
//! ..then std::allocator_traits will implement rebinding automatically by substituting
//! template parameter T in MyAllocator with the other type it needs.
template <class T, class Allocator = std::allocator<T>>
class ObjectPool
{
    static_assert(std::is_destructible_v<T>, "T must be destructible");

public:
    //! Initialize empty pool.
    explicit ObjectPool(const Allocator& allocator = Allocator())
        : alloc_(allocator)
        , cache_(std::make_shared<Cache>())
        , cachingAllocator_(alloc_, cache_)
    {
    }

    //! Destroy pool.
    //!
    //! All objects from cache (those which were allocated and returned to pool)
    //! are deallocated, i.e. returned to underlying allocator.
    //!
    //! Objects that are not returned to pool become dangling. When they expire,
    //! they will notice that the pool was destroyed, and will return their
    //! memory directly to allocator.
    ~ObjectPool() = default;

    //! Allocate and construct object.
    //! @p args are perfect-forwarded to the object's constructor.
    //!
    //! If cache is non-empty, gets memory chunk from cache. Otherwise, allocates
    //! memory chunk from allocator.
    //!
    //! When returned shared_ptr expires, the object is destroyed, but the memory
    //! is returned to the pool's cache instead of the allocator.
    //!
    //! Returned object holds a weak pointer to pool and a rebound copy of the allocator.
    //! If the object outlives the pool, it returns memory to allocator instead of pool.
    //!
    //! Thread-safe, non-blocking.
    template <class... Args>
    std::shared_ptr<T> Allocate(Args&&... args)
    {
        static_assert(
            std::is_constructible_v<T, Args...>, "T must be constructible via T(args)");

        // std::allocate_shared() will:
        //   1. rebind CachingAllocator<T> to CachingAllocator<U>, where U is
        //      a type that holds control block and AllocObject
        //   2. call CachingAllocator::allocate(), which will return uninitialized
        //      memory either from cache or from underlying allocator
        //   3. use placement new to construct AllocObject in that memory
        auto ptr = std::allocate_shared<AllocObject>(
            cachingAllocator_, std::forward<Args>(args)...);

        if (!ptr) {
            return nullptr;
        }

        // Construct aliased shared_ptr:
        //   - refers to the same control block as the original one
        //   - but exposes a different pointer to user, in our case
        //     AllocObject::payload instead of AllocObject itself
        return std::shared_ptr<T>{ptr, &ptr->payload};
    }

private:
    // Base class for cache nodes.
    // Provides abstract method ReturnToAllocator() without knowing concrete
    // allocator type.
    struct CacheNodeBase : FreelistNode
    {
        virtual ~CacheNodeBase() = default;
        virtual void ReturnToAllocator() = 0;
    };

    // Cache node.
    // When memory chunk is free and lives in cache, it is initialized
    // with CacheNode<U>, where U is the element type of a rebound allocator.
    template <class U>
    struct CacheNode final : CacheNodeBase
    {
        // Rebind user-provided Allocator to allocate elements of type U
        using alloc_traits =
            typename std::allocator_traits<Allocator>::template rebind_traits<U>;

        // Hold rebound copy of the allocator
        typename alloc_traits::allocator_type alloc;

        explicit CacheNode(const typename alloc_traits::allocator_type& a)
            : alloc(a)
        {
        }

        // Call destructor and deallocate memory.
        void ReturnToAllocator() override
        {
            // Copy allocator to local variable before calling destructor.
            auto allocCopy = alloc;
            // Destroy `this'.
            this->~CacheNode();
            // Return memory to (rebound) allocator.
            alloc_traits::deallocate(allocCopy, reinterpret_cast<U*>(this), 1);
        }
    };

    // Lock-free cache.
    struct Cache final
    {
        std::map<size_t, Freelist<CacheNodeBase>> cacheMap;
        mutable std::shared_mutex cacheMutex;

        // Select Freelist for type U.
        // Most likely this is always called for the same U, and we'll have a
        // single-element map. However, it's debatable whether std::allocate_shared() is
        // allowed to rebind allocator to multiple types, so we have this safety measure.
        // Anyway, after quickly populating map for all (or one) possible types, next
        // calls won't block or allocate.
        template <class U>
        Freelist<CacheNodeBase>& FindList()
        {
            constexpr size_t Key = sizeof(U);

            { // Fast path
                std::shared_lock readLock(cacheMutex);

                if (auto it = cacheMap.find(Key); it != cacheMap.end()) {
                    return it->second;
                }
            }

            // Slow path
            std::unique_lock writeLock(cacheMutex);

            auto it = cacheMap.find(Key);
            if (it == cacheMap.end()) {
                std::tie(it, std::ignore) = cacheMap.emplace(std::piecewise_construct,
                    std::forward_as_tuple(Key),
                    std::forward_as_tuple());
            }

            return it->second;
        }

        template <class U>
        CacheNodeBase* GetNode()
        {
            return FindList<U>().GetNode();
        }

        template <class U>
        void PutNode(CacheNodeBase* node)
        {
            FindList<U>().PutNode(node);
        }

        ~Cache()
        {
            // At this point, nobody has a reference to the cache, so there is no need
            // to bother about races. Just return everything we have to allocator.
            for (auto& [_, freelist] : cacheMap) {
                while (CacheNodeBase* node = freelist.GetNode()) {
                    node->ReturnToAllocator();
                }
            }
        }
    };

    // Implements standard allocator protocol for std::allocate_shared().
    // Combines underlying user-provided allocator + lock-free cache.
    // Supports rebinding, all rebound copies share cache.
    template <class U>
    struct CachingAllocator final
    {
        using value_type = U;
        using alloc_traits =
            typename std::allocator_traits<Allocator>::template rebind_traits<U>;

        typename alloc_traits::allocator_type alloc;
        std::weak_ptr<Cache> weak_cache;

        CachingAllocator(const typename alloc_traits::allocator_type& alloc,
            const std::shared_ptr<Cache>& cache)
            : alloc(alloc)
            , weak_cache(cache)
        {
        }

        // Construct rebound copy.
        template <class UU>
        CachingAllocator(const CachingAllocator<UU>& other)
            : alloc(other.alloc) // rebind underlying allocator
            , weak_cache(other.weak_cache) // share same cache
        {
        }

        // Called from std::allocate_shared().
        U* allocate(size_t n)
        {
            // We must return pointer to uninitialized memory chunk
            U* mem = nullptr;

            if (std::shared_ptr<Cache> cache = weak_cache.lock()) {
                if (CacheNodeBase* node = cache->template GetNode<U>()) {
                    // We found a cached node. Call its destructor and reuse its memory.
                    node->~CacheNodeBase();
                    mem = reinterpret_cast<U*>(node);
                }
            }

            if (!mem) {
                // Cache is empty, need a real allocation.
                mem = alloc_traits::allocate(alloc, 1);
            }

            return mem;
        }

        // Called when shared_ptr expires.
        // At this point the object destructor is already called.
        void deallocate(U* mem, size_t n)
        {
            if (std::shared_ptr<Cache> cache = weak_cache.lock()) {
                // Pool is still alive, construct cache node in the released memory and
                // add it back to the pool's cache.
                CacheNode<U>* node = new (mem) CacheNode<U>(alloc);
                cache->template PutNode<U>(node);
            } else {
                // Seems we've outlived the pool. Return memory to allocator.
                alloc_traits::deallocate(alloc, mem, 1);
            }
        }
    };

    // Wrapper around T.
    // We actually allocate shared_ptr<AllocObject> instead of shared_ptr<T>.
    union AllocObject
    {
        T payload;

        // Placeholder to ensure that allocated object has enough space for a CacheNode.
        // When AllocObject is returned to pool, its memory is reused for cache node,
        // so we need to be sure that it's large enough even if T is very small.
        CacheNode<T> placeholder;

        template <class... Args>
        AllocObject(Args&&... args)
            : payload(std::forward<Args>(args)...)
        {
        }

        ~AllocObject()
        {
            payload.~T();
        }
    };

    Allocator alloc_;
    std::shared_ptr<Cache> cache_;
    CachingAllocator<T> cachingAllocator_;
};

} // namespace aspl::util
