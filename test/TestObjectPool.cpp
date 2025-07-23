// Copyright (c) libASPL authors
// Licensed under MIT

#include "aspl/util/ObjectPool.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <string>

namespace {

struct TestObject
{
    static std::atomic<int> objectCount;
    static std::atomic<int> ctorCount;
    static std::atomic<int> dtorCount;

    int intValue = 0;
    std::string strValue;

    TestObject(const TestObject&) = delete;
    TestObject& operator=(const TestObject&) = delete;

    TestObject()
    {
        objectCount++;
        ctorCount++;
    }

    TestObject(int intVal, const std::string& strVal)
        : intValue(intVal)
        , strValue(strVal)
    {
        objectCount++;
        ctorCount++;
    }

    ~TestObject()
    {
        objectCount--;
        dtorCount++;
    }
};

std::atomic<int> TestObject::objectCount = 0;
std::atomic<int> TestObject::ctorCount = 0;
std::atomic<int> TestObject::dtorCount = 0;

struct TestCounters
{
    std::atomic<int> allocCount = 0;
    std::atomic<int> deallocCount = 0;
};

template <class T = TestObject>
struct TestAllocator
{
    using value_type = T;

    TestAllocator(TestCounters* counters)
        : counters(counters)
    {
    }

    template <class U>
    TestAllocator(const TestAllocator<U>& other)
        : counters(other.counters)
    {
    }

    TestCounters* counters;

    T* allocate(size_t n)
    {
        counters->allocCount++;
        return static_cast<T*>(operator new(n * sizeof(T)));
    }

    void deallocate(T* p, size_t n)
    {
        counters->deallocCount++;
        operator delete(p);
    }
};

} // namespace

struct ObjectPoolTest : testing::Test
{
    void SetUp() override
    {
        TestObject::objectCount = 0;
        TestObject::ctorCount = 0;
        TestObject::dtorCount = 0;
    }
};

TEST_F(ObjectPoolTest, Allocate)
{
    aspl::util::ObjectPool<TestObject> pool;

    std::shared_ptr<TestObject> obj1 = pool.Allocate();
    std::shared_ptr<TestObject> obj2 = pool.Allocate();
    std::shared_ptr<TestObject> obj3 = pool.Allocate();

    EXPECT_NE(nullptr, obj1);
    EXPECT_NE(nullptr, obj2);
    EXPECT_NE(nullptr, obj3);

    EXPECT_NE(obj1, obj2);
    EXPECT_NE(obj1, obj3);

    EXPECT_EQ(3, TestObject::objectCount);
}

TEST_F(ObjectPoolTest, Pooling)
{
    aspl::util::ObjectPool<TestObject> pool;

    // Allocate 5 objects and remember raw pointers
    std::shared_ptr<TestObject> objsPhase1[5];
    TestObject* ptrsPhase1[5];

    for (size_t i = 0; i < 5; i++) {
        objsPhase1[i] = pool.Allocate();
        ptrsPhase1[i] = objsPhase1[i].get();
    }

    EXPECT_EQ(5, TestObject::objectCount);

    // Allocate 3 more objects
    std::shared_ptr<TestObject> objs2Phase2[3];

    for (size_t i = 0; i < 3; i++) {
        objs2Phase2[i] = pool.Allocate();
    }

    EXPECT_EQ(8, TestObject::objectCount);

    // Drop first 5 shared ptrs, keep last 3
    const size_t releaseOrder[] = {2, 3, 1, 4, 0};

    for (auto idx : releaseOrder) {
        objsPhase1[idx].reset();
    }

    EXPECT_EQ(3, TestObject::objectCount);

    // Allocate 5 objects again
    std::shared_ptr<TestObject> objsPhase3[5];
    TestObject* ptrsPhase3[5];

    for (size_t i = 0; i < 5; i++) {
        objsPhase3[i] = pool.Allocate();
        ptrsPhase3[i] = objsPhase3[i].get();
    }

    EXPECT_EQ(8, TestObject::objectCount);

    // Check that same 5 pointers returned, in reversed order
    // as how they were released
    for (size_t i = 0; i < 5; i++) {
        EXPECT_EQ(ptrsPhase3[i], ptrsPhase1[releaseOrder[5 - i - 1]]);
    }
}

TEST_F(ObjectPoolTest, Construction)
{
    aspl::util::ObjectPool<TestObject> pool;

    {
        std::shared_ptr<TestObject> obj1 = pool.Allocate();
        std::shared_ptr<TestObject> obj2 = pool.Allocate();
        EXPECT_EQ(2, TestObject::ctorCount);
        EXPECT_EQ(0, TestObject::dtorCount);
    }

    EXPECT_EQ(2, TestObject::ctorCount);
    EXPECT_EQ(2, TestObject::dtorCount);

    {
        std::shared_ptr<TestObject> obj3 = pool.Allocate();
        EXPECT_EQ(3, TestObject::ctorCount);
        EXPECT_EQ(2, TestObject::dtorCount);
    }

    EXPECT_EQ(3, TestObject::ctorCount);
    EXPECT_EQ(3, TestObject::dtorCount);
}

TEST_F(ObjectPoolTest, ConstructorArguments)
{
    aspl::util::ObjectPool<TestObject> pool;

    std::shared_ptr<TestObject> obj = pool.Allocate(42, "hello");

    EXPECT_NE(nullptr, obj);
    EXPECT_EQ(42, obj->intValue);
    EXPECT_EQ("hello", obj->strValue);
    EXPECT_EQ(1, TestObject::objectCount);
}

TEST_F(ObjectPoolTest, CustomAllocator)
{
    TestCounters counters;
    TestAllocator allocator(&counters);
    aspl::util::ObjectPool<TestObject, TestAllocator<>> pool(allocator);

    {
        std::shared_ptr<TestObject> obj1 = pool.Allocate();
        std::shared_ptr<TestObject> obj2 = pool.Allocate();
        EXPECT_EQ(2, counters.allocCount);
        EXPECT_EQ(0, counters.deallocCount);
    }

    // Objects should be returned to pool, not deallocated
    EXPECT_EQ(2, counters.allocCount);
    EXPECT_EQ(0, counters.deallocCount);

    {
        std::shared_ptr<TestObject> obj3 = pool.Allocate();
        std::shared_ptr<TestObject> obj4 = pool.Allocate();
        // Should reuse pooled objects
        EXPECT_EQ(2, counters.allocCount);
        EXPECT_EQ(0, counters.deallocCount);
    }
}

TEST_F(ObjectPoolTest, PoolDestructor)
{
    TestCounters counters;
    TestAllocator allocator(&counters);

    {
        aspl::util::ObjectPool<TestObject, TestAllocator<>> pool(allocator);

        {
            std::shared_ptr<TestObject> obj1 = pool.Allocate();
            std::shared_ptr<TestObject> obj2 = pool.Allocate();
            std::shared_ptr<TestObject> obj3 = pool.Allocate();
        }

        EXPECT_EQ(3, counters.allocCount);
        EXPECT_EQ(0, counters.deallocCount);
    }

    // Pool destructor should deallocate pooled objects
    EXPECT_EQ(3, counters.allocCount);
    EXPECT_EQ(3, counters.deallocCount);
}

TEST_F(ObjectPoolTest, DanglingObjects)
{
    TestCounters counters;
    TestAllocator allocator(&counters);

    std::shared_ptr<TestObject> obj1, obj2;

    {
        aspl::util::ObjectPool<TestObject, TestAllocator<>> pool(allocator);
        obj1 = pool.Allocate();
        obj2 = pool.Allocate();

        EXPECT_EQ(2, counters.allocCount);
        EXPECT_EQ(0, counters.deallocCount);
    }

    // Pool destroyed, but objects still alive
    EXPECT_EQ(2, counters.allocCount);
    EXPECT_EQ(0, counters.deallocCount);
    EXPECT_EQ(2, TestObject::objectCount);

    // Release shared ptrs
    obj1.reset();
    obj2.reset();

    // Objects should be deallocated now
    EXPECT_EQ(2, counters.allocCount);
    EXPECT_EQ(2, counters.deallocCount);
    EXPECT_EQ(0, TestObject::objectCount);
}

TEST_F(ObjectPoolTest, SmallObject)
{
    struct SmallObject
    {
        char data = 0;
    };

    aspl::util::ObjectPool<SmallObject> pool;

    std::shared_ptr<SmallObject> obj1 = pool.Allocate();
    std::shared_ptr<SmallObject> obj2 = pool.Allocate();

    EXPECT_NE(nullptr, obj1);
    EXPECT_NE(nullptr, obj2);
    EXPECT_NE(obj1, obj2);
}
