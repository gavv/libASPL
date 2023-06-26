#include <aspl/Object.hpp>

#include "TestTracer.hpp"

#include <algorithm>
#include <deque>
#include <map>
#include <set>
#include <vector>

#include <gtest/gtest.h>

struct DispatcherTest : ::testing::Test
{
    static constexpr UInt32 TestHintMaxID = 50;

    std::shared_ptr<aspl::Tracer> tracer = std::make_shared<TestTracer>();

    std::shared_ptr<aspl::Dispatcher> dispatcher =
        std::make_shared<aspl::Dispatcher>(tracer, TestHintMaxID);

    std::shared_ptr<aspl::Context> context =
        std::make_shared<aspl::Context>(tracer, dispatcher);
};

TEST_F(DispatcherTest, UniqueAlive)
{
    constexpr size_t NumCreatedObjects = TestHintMaxID * 3;

    std::map<AudioObjectID, std::shared_ptr<aspl::Object>> objects;

    for (size_t i = 0; i < NumCreatedObjects; i++) {
        auto object = std::make_shared<aspl::Object>(context);

        // check that identifier is always non-zero
        ASSERT_GT(object->GetID(), 0);

        // check that identifiers are unique
        ASSERT_EQ(objects.count(object->GetID()), 0);

        objects[object->GetID()] = object;
    }
}

TEST_F(DispatcherTest, FindAlive)
{
    constexpr size_t NumCreatedObjects = TestHintMaxID * 3;

    std::map<AudioObjectID, std::shared_ptr<aspl::Object>> objects;

    for (size_t i = 0; i < NumCreatedObjects; i++) {
        auto object = std::make_shared<aspl::Object>(context);

        // check that identifier is always non-zero
        ASSERT_GT(object->GetID(), 0);

        // check that identifiers are unique
        ASSERT_EQ(objects.count(object->GetID()), 0);

        objects[object->GetID()] = object;
    }

    // check that we can find each object
    for (auto [objectID, object] : objects) {
        ASSERT_EQ(context->Dispatcher->FindObject(objectID), object);
    }
}

TEST_F(DispatcherTest, ReservedID)
{
    constexpr size_t NumCreatedObjects = TestHintMaxID * 3;

    const auto ReservedID = kAudioObjectPlugInObject;

    std::vector<std::shared_ptr<aspl::Object>> objects;

    for (size_t i = 0; i < NumCreatedObjects; i++) {
        auto object = std::make_shared<aspl::Object>(context);

        // check that identifier is always non-zero
        ASSERT_GT(object->GetID(), 0);

        // check that reserved identifier is never auto-allocated
        ASSERT_NE(object->GetID(), ReservedID);

        objects.push_back(object);
    }

    auto object = std::make_shared<aspl::Object>(context, "Object", ReservedID);

    // check that reserved identifier can be assigned manually
    ASSERT_EQ(object->GetID(), ReservedID);
}

TEST_F(DispatcherTest, ReuseDestroyed)
{
    constexpr size_t DesiredReusedObjects = TestHintMaxID / 10;

    std::set<AudioObjectID> everUsed;
    std::set<AudioObjectID> reused;

    while (reused.size() < DesiredReusedObjects) {
        auto object = std::make_shared<aspl::Object>(context);

        // check that identifier is always non-zero
        ASSERT_GT(object->GetID(), 0);

        if (everUsed.count(object->GetID())) {
            reused.insert(object->GetID());
        }

        everUsed.insert(object->GetID());
    }
}

TEST_F(DispatcherTest, Mixed)
{
    // we want to achieve 3000 alive objects
    constexpr size_t DesiredAliveObjects = TestHintMaxID * 3;

    // we will keep alive every 3rd created object
    constexpr size_t KeepAliveRatio = 3;

    // we check that at least 1/3 of ever allocated identifiers are reused
    constexpr double MinReuseRatio = 1. / 3.;

    // we check that a new allocated identifier is not reused from at least the
    // last 300 destroyed objects
    constexpr size_t MinReuseDelay = TestHintMaxID / 3;

    // we need to create this number of objects to achieve desired number
    // of alive objects (because we will drop part of the created objects,
    // according to KeepAliveRatio)
    constexpr size_t TotalCreatedObjects = DesiredAliveObjects * KeepAliveRatio;

    std::map<AudioObjectID, std::shared_ptr<aspl::Object>> aliveObjects;
    std::deque<AudioObjectID> recentlyDestroyedObjects;

    size_t createdObjects = 0;

    while (aliveObjects.size() < DesiredAliveObjects) {
        auto object = std::make_shared<aspl::Object>(context);

        // check that identifier is always non-zero
        ASSERT_GT(object->GetID(), 0);

        // check that identifiers are reused instead of unrestrictedly growing
        ASSERT_LE(object->GetID(), TotalCreatedObjects * (1. - MinReuseRatio));

        // check that identifiers are unique
        ASSERT_EQ(aliveObjects.count(object->GetID()), 0);

        // check that identifiers are not reused immediately
        ASSERT_TRUE(std::find(recentlyDestroyedObjects.begin(),
                        recentlyDestroyedObjects.end(),
                        object->GetID()) == recentlyDestroyedObjects.end());

        if ((createdObjects % KeepAliveRatio) == KeepAliveRatio - 1) {
            aliveObjects[object->GetID()] = object;
        } else {
            recentlyDestroyedObjects.push_back(object->GetID());
            if (recentlyDestroyedObjects.size() > MinReuseDelay) {
                recentlyDestroyedObjects.pop_front();
            }
        }
        createdObjects++;
    }

    ASSERT_EQ(createdObjects, TotalCreatedObjects);
}
