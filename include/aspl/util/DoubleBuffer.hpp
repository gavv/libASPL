// Copyright (c) libASPL authors
// Licensed under MIT

//! @file aspl/util/DoubleBuffer.hpp
//! @brief Lock-free double buffer container.

#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <utility>

namespace aspl::util {

//! Doubly-buffered value with non-blocking read and blocking write.
//!
//! Logically, this is a single-value container, which has getter and setter
//! with the following characteristics:
//!
//!  - getters are running concurrently, and setters are serialized
//!
//!  - setter is blocking; it may be blocked by both getters and setters,
//!    but in the average case only by setters
//!
//!  - getter is non-blocking and lock-free; it does not block if a setter
//!    thread is suspended in the middle
//!
//!  - getter is not wait-free though; if setters are called too frequently
//!    and have higher priority, then getter may spin until it has a chance
//!    to cut in between
//!
//!  - sequential consistency is provided; after a setter returns, it's
//!    guaranteed that subsequent getters will observe up-to-date value
//!
//! Physically container is implemented as two copies of the value:
//!
//!  - one copy is read-only and used by getters (concurrently)
//!
//!  - another one is for setters; basically, the setter updates its copy
//!    while it's not visible to getters, waits until all getter using
//!    another copy finish, and switches the copies
//!
//! The algorithm is optimized for the following use case:
//!
//!  - getters are frequent and setters are infrequent
//!
//!  - the value is not very large and it's acceptable to make
//!    extra copies
//!
//! The value stored in the double buffer is immutable after it's set. To
//! change the value, you need to call the getter, make a copy, modify it,
//! and pass it to the setter.
//!
//! The value should have public default and copy constructors. If it also
//! has move constructor, it can be used in setter.
//!
//! If the value has non-trivial destructor, before setter returns, it waits
//! until the previously used value is not accessed by readers anymore and
//! invokes destructor for the old value.
//!
//! Typical reader looks like the following:
//! @code
//!   DoubleBuffer<T> fooBuf;
//!   ...
//!   {
//!     auto readLock = fooBuf.GetReadLock();
//!
//!     const T& fooValue = readLock.GetReference();
//!     // safe read-only access to fooValue until block end
//!   }
//! @endcode
//!
//! Or, if the reader is okay to call the copy constructor:
//! @code
//!   DoubleBuffer<T> fooBuf;
//!   ...
//!   T fooValue = fooBuf.ReadValue();
//! @endcode
//!
//! Typical writer looks like this:
//! @code
//!   DoubleBuffer<T> fooBuf;
//!   ...
//!   {
//!     auto writeLock = fooBuf.GetWriteLock();
//!
//!     T& newFooValue = writeLock.GetWriteReference();
//!     // safe write-only access to newFooValue until block end;
//!     // write-only means that newFooValue does not have useful
//!     // value, it's available only for writing!
//!
//!     const T& oldFooValue = writeLock.GetReadReference();
//!     // optional: read-only access to old value, if you need
//!   }
//! @endcode
//!
//! Or, if the writer is okay to call the copy or move constructor:
//! @code
//!   DoubleBuffer<T> fooBuf;
//!   ...
//!   T fooValue = fooBuf.ReadValue(); // copy ctor
//!   // modify fooValue
//!   fooBuf.WriteValue(std::move(fooValue)); // move ctor
//! @endcode
//!
//! @see ReadLock, WriteLock.
template <typename T>
class DoubleBuffer
{
    using BufferIndex = int64_t;
    struct Buffer;

public:
    //! Read lock.
    //!
    //! Similar to scoped locks like std::lock_guard, but also provides
    //! GetReference() method to access the value.
    //!
    //! While the lock is alive, it's safe to access the data for reading.
    class ReadLock
    {
    public:
        //! Acquire the lock.
        //! Non-blocking and lock-free.
        //! May spin for a while if a setter is running concurrently, but wont
        //! block if the setter is suspended somewhere on the halfway.
        ReadLock(const DoubleBuffer& doubleBuffer);

        //! Release the lock.
        ~ReadLock();

        //! Move lock.
        ReadLock(ReadLock&& other);

        ReadLock(const ReadLock&) = delete;
        ReadLock& operator=(const ReadLock&) = delete;
        ReadLock& operator=(ReadLock&&) = delete;

        //! Get read-only reference to the value.
        //! The reference may be used until read lock destructor is called.
        //! The referred value is guaranteed to be immutable until that.
        const T& GetReference() const;

    private:
        const Buffer* buffer_;
    };

    //! Write lock.
    //!
    //! Similar to scoped locks like std::lock_guard, but also provides
    //! GetReference() method to access the value.
    //!
    //! While the lock is alive, it's safe to access the data for writing.
    class WriteLock
    {
    public:
        //! Acquire the lock.
        //! Blocks until all getters, invoked before this call, are finished.
        //! Concurrent write locks are serialized.
        WriteLock(DoubleBuffer& doubleBuffer);

        //! Release the lock.
        //! It is guaranteed that if a getter is called after a write lock is released,
        //! the getter will observe the updated value.
        ~WriteLock();

        //! Move lock.
        WriteLock(WriteLock&& other);

        WriteLock(const WriteLock&) = delete;
        WriteLock& operator=(const WriteLock&) = delete;
        WriteLock& operator=(WriteLock&&) = delete;

        //! Get read-only reference to the old value.
        //! The reference may be used until write lock destructor is called.
        //! This reference is different from GetWriteReference().
        const T& GetReadReference() const;

        //! Get write-only reference to the new value.
        //! The reference may be used until write lock destructor is called.
        //! This reference is different from GetReadReference().
        //! The referenced value is default-initialized and does NOT contain
        //! the up-to-date data; use GetReadReference() if you need one.
        //! For trivial types, default-initialized means garbage.
        T& GetWriteReference();

        //! Construct contained value in-place.
        //! A bit more efficient than GetWriteReference() when you need
        //! to call non-trivial constructor.
        template <typename... Args>
        void Emplace(Args&&... args);

    private:
        DoubleBuffer* doubleBuffer_;

        BufferIndex oldIndex_;
        BufferIndex newIndex_;

        Buffer* oldBuffer_;
        Buffer* newBuffer_;
    };

    //! Initialize buffer with given value.
    explicit DoubleBuffer(const T& value)
    {
        WriteValue(value);
    }

    //! Initialize buffer with given value.
    explicit DoubleBuffer(T&& value = T())
    {
        WriteValue(std::move(value));
    }

    ~DoubleBuffer() = default;

    DoubleBuffer(const DoubleBuffer&) = delete;
    DoubleBuffer& operator=(const DoubleBuffer&) = delete;

    //! Get locked read-only reference to the value.
    //! Non-blocking and lock-free, see ReadLock for details.
    ReadLock GetReadLock() const
    {
        return ReadLock(*this);
    }

    //! Get locked write-only reference to the value.
    //! Blocking, see WriteLock for details.
    WriteLock GetWriteLock()
    {
        return WriteLock(*this);
    }

    //! Get copy of the value.
    //! Non-blocking and lock-free, see ReadLock for details.
    //! To avoid copy constructor, use GetReadLock() instead.
    T ReadValue() const
    {
        ReadLock readLock(*this);

        return T(readLock.GetReference());
    }

    //! Overwrite value by copy, move, or constructing in-place.
    //! Blocking, see WriteLock for details.
    //! To avoid copy/move constructor, use GetWriteLock() instead.
    template <typename... Args>
    void WriteValue(Args&&... args)
    {
        WriteLock writeLock(*this);

        writeLock.Emplace(std::forward<Args>(args)...);
    }

private:
    friend class ReadLock;
    friend class WriteLock;

    struct Buffer
    {
        std::optional<T> value = {};
        std::atomic<BufferIndex> index = -1;
        mutable std::shared_mutex mutex;
    };

    const Buffer& GetBufferAt(BufferIndex index) const
    {
        return buffers_[index % 2];
    }

    Buffer& GetBufferAt(BufferIndex index)
    {
        return buffers_[index % 2];
    }

    std::mutex writeMutex_;

    Buffer buffers_[2];
    std::atomic<BufferIndex> currentIndex_ = 0;
};

template <typename T>
DoubleBuffer<T>::ReadLock::ReadLock(const DoubleBuffer& doubleBuffer)
{
    for (;;) {
        // Load current buffer index.
        const auto currentIndex = doubleBuffer.currentIndex_.load();

        // Get reference to the current buffer.
        auto& currentBuffer = doubleBuffer.GetBufferAt(currentIndex);

        // Try to lock buffer for reading.
        //
        // The lock can fail in the case when, after we loaded the current
        // index, but before obtained the lock, the setter was called and
        // finished once, and then was called another time and is currently
        // in progress. Here we should retry and re-read current index.
        //
        // We're still lock-free because even if the ongoing setter is
        // suspended, we wont block. Instead, on the next try we will
        // switch to another buffer and will successfully obtain the lock.
        //
        // However, we're not wait-free because while this situation is
        // repeating and new setters continue to come and preempt us, we'll
        // have to retry. Fortunately, this is very unlikely to happen.
        if (!currentBuffer.mutex.try_lock_shared()) {
            continue;
        }

        // Check that the buffer index still matches the our current index
        // after obtaining the lock.
        //
        // This check catches the case similar to the above one, but when
        // the second setter is already finished.
        if (currentBuffer.index.load() != currentIndex) {
            currentBuffer.mutex.unlock_shared();
            continue;
        }

        // At this point, it is guaranteed that any setter will see that
        // we're using the buffer and wont rewrite it until we release
        // the lock in destructor.
        buffer_ = &currentBuffer;
        return;
    }
}

template <typename T>
DoubleBuffer<T>::ReadLock::~ReadLock()
{
    if (!buffer_) {
        // ReadLock moved.
        return;
    }

    buffer_->mutex.unlock_shared();
}

template <typename T>
DoubleBuffer<T>::ReadLock::ReadLock(ReadLock&& other)
    : buffer_(other.buffer_)
{
    other.buffer_ = nullptr;
}

template <typename T>
const T& DoubleBuffer<T>::ReadLock::GetReference() const
{
    return *buffer_->value;
}

template <typename T>
DoubleBuffer<T>::WriteLock::WriteLock(DoubleBuffer& doubleBuffer)
    : doubleBuffer_(&doubleBuffer)
{
    // Serialize setters.
    doubleBuffer.writeMutex_.lock();

    // Load current buffer index.
    // Since this index is modified only by setters, it's safe to use "relaxed".
    oldIndex_ = doubleBuffer.currentIndex_.load(std::memory_order_relaxed);

    // Calculate next index.
    // Maximum signed value overflows to zero.
    // Negative indices are reserved to indicate invalidated buffers.
    newIndex_ = oldIndex_ < std::numeric_limits<BufferIndex>::max() ? oldIndex_ + 1 : 0;

    // Get reference to the current buffer.
    oldBuffer_ = &doubleBuffer.GetBufferAt(oldIndex_);

    // Get reference to the next buffer, i.e. not the current one.
    newBuffer_ = &doubleBuffer.GetBufferAt(newIndex_);

    // Notify getters which were started earlier and are either working or going
    // to work with this buffer that the buffer is going to be invalidated.
    newBuffer_->index = newIndex_;

    // Wait until finishing of ongoing getters that are still using this buffer.
    //
    // After we obtain the lock, we can be sure that old getters are either
    // finished or will see the index which we've updated above and wont use the
    // buffer.
    //
    // Note that since it is the other buffer, not the current one, chances are
    // that there are no getters already, and most times we will acquire the lock
    // immediately here.
    newBuffer_->mutex.lock();
}

template <typename T>
DoubleBuffer<T>::WriteLock::~WriteLock()
{
    if (!doubleBuffer_) {
        // WriteLock moved.
        return;
    }

    // Finish writing value into new buffer.
    newBuffer_->mutex.unlock();

    // Switch current buffer index to the new buffer.
    // It is guaranteed that all getters invoked after this point will return
    // the new value.
    doubleBuffer_->currentIndex_ = newIndex_;

    if constexpr (!std::is_trivial<T>::value) {
        // Notify getters which were started earlier and are either working or going
        // to work with this buffer that the buffer is going to be invalidated.
        oldBuffer_->index = -1;

        // Wait until finishing of ongoing getters that are still using the buffer.
        // Here we can block for a while.
        std::unique_lock lock(oldBuffer_->mutex);

        // Destroy value.
        // This is needed to provide semantics of "replacing" value. We've written
        // the new value and we should destroy the old one.
        oldBuffer_->value = {};
    }

    doubleBuffer_->writeMutex_.unlock();
}

template <typename T>
DoubleBuffer<T>::WriteLock::WriteLock(WriteLock&& other)
    : doubleBuffer_(other.doubleBuffer_)
    , oldIndex_(other.oldIndex_)
    , newIndex_(other.newIndex_)
    , oldBuffer_(other.oldBuffer_)
    , newBuffer_(other.newBuffer_)
{
    other.doubleBuffer_ = nullptr;
}

template <typename T>
const T& DoubleBuffer<T>::WriteLock::GetReadReference() const
{
    return *oldBuffer_->value;
}

template <typename T>
T& DoubleBuffer<T>::WriteLock::GetWriteReference()
{
    if (!newBuffer_->value) {
        *newBuffer_->value.emplace();
    }
    return *newBuffer_->value;
}

template <typename T>
template <typename... Args>
void DoubleBuffer<T>::WriteLock::Emplace(Args&&... args)
{
    newBuffer_->value.emplace(std::forward<Args>(args)...);
}

} // namespace aspl::util
