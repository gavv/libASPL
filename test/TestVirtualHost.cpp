// Copyright (c) libASPL authors
// Licensed under MIT

#include "aspl/Context.hpp"
#include "aspl/Device.hpp"
#include "aspl/Plugin.hpp"
#include "aspl/Storage.hpp"
#include "aspl/VirtualHost.hpp"

#include "ChronoAssertions.hpp"
#include "Convert.hpp"
#include "MockTracer.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace chrono = std::chrono;

namespace {

class CustomVirtualHost : public aspl::VirtualHost
{
public:
    explicit CustomVirtualHost(std::shared_ptr<aspl::Tracer> tracer,
        const aspl::VirtualHostParameters& params = {})
        : VirtualHost(tracer, params)
    {
    }

    void SetFakeTime(chrono::steady_clock::time_point time)
    {
        fakeTimeNanos_ =
            chrono::duration_cast<chrono::nanoseconds>(time.time_since_epoch()).count();
    }

    void AdvanceFakeTime(chrono::steady_clock::duration duration)
    {
        fakeTimeNanos_ += chrono::duration_cast<chrono::nanoseconds>(duration).count();
    }

    // Make protected methods public for testing
    using VirtualHost::DoNextTickImpl;
    using VirtualHost::GetNextTickImpl;
    using VirtualHost::IsSafetyZoneImpl;

    chrono::steady_clock::time_point GetCurrentTimeImpl() const override
    {
        return chrono::steady_clock::time_point(
            chrono::nanoseconds(fakeTimeNanos_.load()));
    }

private:
    std::atomic<UInt64> fakeTimeNanos_;
};

struct CustomEventInfo : aspl::VirtualEventInfo
{
    explicit CustomEventInfo()
        : VirtualEventInfo(aspl::VirtualEventType::CustomEvent)
    {
    }

    std::string CustomField;
    int CustomNumber = 0;
};

template <typename Rep, typename Period>
void RandomDelay(std::chrono::duration<Rep, Period> maxDelay)
{
    static thread_local std::random_device device;
    static thread_local std::mt19937 gen(device());

    auto maxNanos =
        std::chrono::duration_cast<std::chrono::nanoseconds>(maxDelay).count();

    std::uniform_int_distribution<decltype(maxNanos)> dist(0, maxNanos);

    std::this_thread::sleep_for(std::chrono::nanoseconds(dist(gen)));
}

void ShortDelay()
{
    std::this_thread::sleep_for(chrono::milliseconds(5));
}

} // anonymous namespace

struct VirtualHostTest : testing::Test
{
    std::shared_ptr<MockTracer> tracer;

    aspl::VirtualHostParameters virtParams;

    void SetUp() override
    {
        tracer = std::make_shared<MockTracer>();

        virtParams.EnableEventTracing = true;
        virtParams.EnableIOTracing = true;
        virtParams.EnableStorageTracing = true;
    }
};

TEST_F(VirtualHostTest, Events_ObjectPropertyChange)
{
    auto virtHost = std::make_shared<aspl::VirtualHost>(tracer);

    { // single address
        AudioObjectPropertyAddress address = {
            'test', kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};

        OSStatus status = virtHost->PropertiesChanged(123, 1, &address);
        EXPECT_EQ(status, kAudioHardwareNoError);

        EXPECT_TRUE(virtHost->HasAvailEvents());

        auto event = virtHost->ReadEvent();
        ASSERT_NE(event, nullptr);
        EXPECT_EQ(event->EventType, aspl::VirtualEventType::ObjectPropertyChange);
        ASSERT_TRUE(event->ObjectPropertyChange.has_value());
        EXPECT_EQ(event->ObjectPropertyChange->ObjectID, 123u);
        EXPECT_EQ(event->ObjectPropertyChange->PropertyAddress.mSelector, 'test');
        EXPECT_EQ(event->ObjectPropertyChange->PropertyAddress.mScope,
            kAudioObjectPropertyScopeGlobal);
        EXPECT_EQ(event->ObjectPropertyChange->PropertyAddress.mElement,
            kAudioObjectPropertyElementMain);

        EXPECT_FALSE(virtHost->HasAvailEvents());
    }

    { // multiple addresses
        AudioObjectPropertyAddress addresses[3] = {
            {'tst1', kAudioObjectPropertyScopeInput, 1},
            {'tst2', kAudioObjectPropertyScopeOutput, 2},
            {'tst3', kAudioObjectPropertyScopeGlobal, 3}};

        OSStatus status = virtHost->PropertiesChanged(456, 3, addresses);
        EXPECT_EQ(status, kAudioHardwareNoError);

        EXPECT_TRUE(virtHost->HasAvailEvents());

        for (int i = 0; i < 3; ++i) {
            auto event = virtHost->ReadEvent();
            ASSERT_NE(event, nullptr);
            EXPECT_EQ(event->EventType, aspl::VirtualEventType::ObjectPropertyChange);
            ASSERT_TRUE(event->ObjectPropertyChange.has_value());
            EXPECT_EQ(event->ObjectPropertyChange->ObjectID, 456u);
            EXPECT_EQ(event->ObjectPropertyChange->PropertyAddress.mSelector,
                addresses[i].mSelector);
            EXPECT_EQ(
                event->ObjectPropertyChange->PropertyAddress.mScope, addresses[i].mScope);
            EXPECT_EQ(event->ObjectPropertyChange->PropertyAddress.mElement,
                addresses[i].mElement);
        }

        EXPECT_FALSE(virtHost->HasAvailEvents());
    }
}

TEST_F(VirtualHostTest, Events_DeviceConfigurationRequest)
{
    auto virtHost = std::make_shared<aspl::VirtualHost>(tracer);

    // invoke RequestDeviceConfigurationChange 3 times
    for (int i = 0; i < 3; ++i) {
        OSStatus status = virtHost->RequestDeviceConfigurationChange(
            100 + i, 200 + i, reinterpret_cast<void*>(300 + i));
        EXPECT_EQ(status, kAudioHardwareNoError);
    }

    EXPECT_TRUE(virtHost->HasAvailEvents());

    // read and check 3 events
    for (size_t i = 0; i < 3; ++i) {
        auto event = virtHost->ReadEvent();
        ASSERT_NE(event, nullptr);
        EXPECT_EQ(event->EventType, aspl::VirtualEventType::DeviceConfigurationRequest);
        ASSERT_TRUE(event->DeviceConfigurationRequest.has_value());
        EXPECT_EQ(event->DeviceConfigurationRequest->DeviceID, 100 + i);
        EXPECT_EQ(event->DeviceConfigurationRequest->ChangeAction, 200 + i);
        EXPECT_EQ(event->DeviceConfigurationRequest->ChangeInfo,
            reinterpret_cast<void*>(300 + i));
    }

    EXPECT_FALSE(virtHost->HasAvailEvents());
}

TEST_F(VirtualHostTest, Events_Storage)
{
    auto virtHost = std::make_shared<aspl::VirtualHost>(tracer);

    CFStringRef key = CFStringCreateWithCString(
        kCFAllocatorDefault, "event_key", kCFStringEncodingUTF8);
    CFStringRef value = CFStringCreateWithCString(
        kCFAllocatorDefault, "event_value", kCFStringEncodingUTF8);

    { // write key
        OSStatus status = virtHost->WriteToStorage(key, value);
        EXPECT_EQ(status, kAudioHardwareNoError);

        EXPECT_TRUE(virtHost->HasAvailEvents());

        auto event = virtHost->ReadEvent();
        ASSERT_NE(event, nullptr);
        EXPECT_EQ(event->EventType, aspl::VirtualEventType::StorageDataChange);
        ASSERT_TRUE(event->StorageDataChange.has_value());
        EXPECT_EQ(event->StorageDataChange->StorageKey, "event_key");

        EXPECT_FALSE(virtHost->HasAvailEvents());
    }

    { // read key - no new events
        CFPropertyListRef readValue = nullptr;
        OSStatus status = virtHost->CopyFromStorage(key, &readValue);
        EXPECT_EQ(status, kAudioHardwareNoError);
        ASSERT_NE(readValue, nullptr);

        EXPECT_FALSE(virtHost->HasAvailEvents());

        CFRelease(readValue);
    }

    { // delete key
        OSStatus status = virtHost->DeleteFromStorage(key);
        EXPECT_EQ(status, kAudioHardwareNoError);

        EXPECT_TRUE(virtHost->HasAvailEvents());

        auto event = virtHost->ReadEvent();
        ASSERT_NE(event, nullptr);
        EXPECT_EQ(event->EventType, aspl::VirtualEventType::StorageDataChange);
        ASSERT_TRUE(event->StorageDataChange.has_value());
        EXPECT_EQ(event->StorageDataChange->StorageKey, "event_key");

        EXPECT_FALSE(virtHost->HasAvailEvents());
    }

    CFRelease(key);
    CFRelease(value);
}

TEST_F(VirtualHostTest, Events_CustomType)
{
    auto virtHost = std::make_shared<aspl::VirtualHost>(tracer);

    // create custom event
    auto customEvent = std::make_shared<CustomEventInfo>();
    customEvent->CustomField = "test_custom_field";
    customEvent->CustomNumber = 42;

    virtHost->EnqueueEvent(customEvent);

    EXPECT_TRUE(virtHost->HasAvailEvents());

    // receive with ReadEvent
    auto event = virtHost->ReadEvent();
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->EventType, aspl::VirtualEventType::CustomEvent);

    // cast back to custom type
    auto receivedCustom = std::dynamic_pointer_cast<CustomEventInfo>(event);
    ASSERT_NE(receivedCustom, nullptr);
    EXPECT_EQ(receivedCustom->CustomField, "test_custom_field");
    EXPECT_EQ(receivedCustom->CustomNumber, 42);

    EXPECT_FALSE(virtHost->HasAvailEvents());
}

TEST_F(VirtualHostTest, Ticks_Sequential)
{
    static constexpr double TicksPerSec = 1000;
    static constexpr int TickCount = 50;

    virtParams.TicksPerSecond = TicksPerSec;

    auto virtHost = std::make_shared<aspl::VirtualHost>(tracer, virtParams);

    chrono::steady_clock::time_point firstTickTime;
    bool firstTickReceived = false;

    for (int i = 0; i < TickCount; ++i) {
        auto event = virtHost->WaitEvent();
        ASSERT_TRUE(event);

        ASSERT_EQ(event->EventType, aspl::VirtualEventType::IOTimerTick);
        ASSERT_TRUE(event->IOTimerTick);

        if (!firstTickReceived) {
            firstTickTime = event->IOTimerTick->Time;
            firstTickReceived = true;
        }

        EXPECT_EQ(event->IOTimerTick->Number, i);
        CHRONO_EXPECT_EQ(event->IOTimerTick->Time,
            firstTickTime + chrono::nanoseconds(SInt64(1e9 / TicksPerSec * i)));
    }
}

TEST_F(VirtualHostTest, Ticks_SkipLate_Off)
{
    static constexpr double TicksPerSec = 1000;
    static constexpr int TickCount = 10;

    virtParams.TicksPerSecond = TicksPerSec;
    virtParams.SkipLateTicks = false;

    auto virtHost = std::make_shared<aspl::VirtualHost>(tracer, virtParams);

    // get first tick to start timer
    auto firstEvent = virtHost->WaitEvent();
    ASSERT_TRUE(firstEvent);
    ASSERT_EQ(firstEvent->EventType, aspl::VirtualEventType::IOTimerTick);
    ASSERT_TRUE(firstEvent->IOTimerTick);
    ASSERT_EQ(firstEvent->IOTimerTick->Number, 0);
    auto firstTickTime = firstEvent->IOTimerTick->Time;

    // sleep N ticks
    std::this_thread::sleep_for(
        chrono::milliseconds(SInt64(TickCount * 1000 / TicksPerSec)));

    // read events until end
    std::vector<std::shared_ptr<aspl::VirtualEventInfo>> events;
    while (auto event = virtHost->ReadEvent()) {
        events.push_back(event);
    }

    // should have >= N ticks
    EXPECT_GE(events.size(), 10u);

    for (size_t i = 0; i < events.size(); ++i) {
        ASSERT_EQ(events[i]->EventType, aspl::VirtualEventType::IOTimerTick);
        ASSERT_TRUE(events[i]->IOTimerTick);
        EXPECT_EQ(events[i]->IOTimerTick->Number, i + 1);
        CHRONO_EXPECT_EQ(events[i]->IOTimerTick->Time,
            firstTickTime + chrono::nanoseconds(SInt64(1e9 / TicksPerSec * (i + 1))));
    }
}

TEST_F(VirtualHostTest, Ticks_SkipLate_On)
{
    static constexpr double TicksPerSec = 1000;
    static constexpr int TickCount = 10;

    virtParams.TicksPerSecond = TicksPerSec;
    virtParams.SkipLateTicks = true;

    auto virtHost = std::make_shared<aspl::VirtualHost>(tracer, virtParams);

    // get first tick to start timer
    auto firstEvent = virtHost->WaitEvent();
    ASSERT_TRUE(firstEvent);
    ASSERT_EQ(firstEvent->EventType, aspl::VirtualEventType::IOTimerTick);
    ASSERT_TRUE(firstEvent->IOTimerTick);
    ASSERT_EQ(firstEvent->IOTimerTick->Number, 0);
    auto firstTickTime = firstEvent->IOTimerTick->Time;

    // sleep N ticks
    std::this_thread::sleep_for(
        chrono::milliseconds(SInt64(TickCount * 1000 / TicksPerSec)));

    // wait next tick - should be >= Nth tick
    auto nextEvent = virtHost->WaitEvent();
    ASSERT_TRUE(nextEvent);
    ASSERT_EQ(nextEvent->EventType, aspl::VirtualEventType::IOTimerTick);
    ASSERT_TRUE(nextEvent->IOTimerTick);
    EXPECT_GE(nextEvent->IOTimerTick->Number, TickCount);
    CHRONO_EXPECT_EQ(nextEvent->IOTimerTick->Time,
        firstTickTime + chrono::nanoseconds(
                            SInt64(1e9 / TicksPerSec * nextEvent->IOTimerTick->Number)));
}

TEST_F(VirtualHostTest, Ticks_SafetyZone)
{
    static constexpr int MinEventCount = 100;

    static constexpr double TicksPerSec = 1000;
    static constexpr auto TickSafetyZone = chrono::microseconds(500);

    static constexpr auto TickPeriod =
        chrono::nanoseconds(static_cast<SInt64>(1e9 / TicksPerSec));

    static constexpr auto EventDelay = chrono::microseconds(50);

    struct TestCase
    {
        const char* Name;
        bool EnableSafetyZone;
    };

    TestCase testCases[] = {
        {"SafetyZone=off", false},
        {"SafetyZone=on", true},
    };

    for (const auto& tcase : testCases) {
        aspl::VirtualHostParameters params = virtParams;

        params.TicksPerSecond = TicksPerSec;
        params.TickSafetyZone =
            tcase.EnableSafetyZone ? TickSafetyZone : chrono::nanoseconds(0);

        auto virtHost = std::make_shared<aspl::VirtualHost>(tracer, params);

        std::atomic<bool> stopFlag = false;
        std::atomic<int> tickEventCount = 0;
        std::atomic<int> propEventCount = 0;
        std::atomic<int> insideSafetyZoneCount = 0;
        std::atomic<int> outsideSafetyZoneCount = 0;

        std::optional<chrono::steady_clock::time_point> nextSafetyZone;
        bool inSafetyZone = false;

        // thread that triggers property events
        std::thread propertyThread([&]() {
            AudioObjectPropertyAddress address = {
                'test', kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
            while (!stopFlag) {
                virtHost->PropertiesChanged(123, 1, &address);
                RandomDelay(EventDelay);
            }
        });

        // accumulate tick and property events
        while (tickEventCount < MinEventCount || propEventCount < MinEventCount) {
            auto event = virtHost->WaitEvent();
            ASSERT_TRUE(event) << tcase.Name;

            if (event->EventType == aspl::VirtualEventType::IOTimerTick) {
                tickEventCount++;

                // leaving safety zone
                inSafetyZone = false;
                // when to enter next safety zone
                nextSafetyZone = event->IOTimerTick->Time + TickPeriod - TickSafetyZone;
            } else if (event->EventType == aspl::VirtualEventType::ObjectPropertyChange) {
                propEventCount++;

                // DequeueTime is recorded in WaitEvent before it returns; here, it is
                // very close to current time, but is a bit behind.
                // We check DequeueTime because it's the time that WaitEvent() used for
                // safety zone check. This way test can be deterministic - we won't
                // get spurious failures when WaitEvent() returns event just before
                // safety zone starts (which is fine).
                if (!inSafetyZone && nextSafetyZone &&
                    event->DequeueTime >= *nextSafetyZone) {
                    // entering safety zone
                    inSafetyZone = true;
                }

                if (inSafetyZone) {
                    insideSafetyZoneCount++;
                } else {
                    outsideSafetyZoneCount++;
                }
            }
        }

        stopFlag = true;
        propertyThread.join();

        EXPECT_GE(tickEventCount, MinEventCount) << tcase.Name;
        EXPECT_GE(propEventCount, MinEventCount) << tcase.Name;

        if (tcase.EnableSafetyZone) {
            EXPECT_EQ(insideSafetyZoneCount, 0) << tcase.Name;
            EXPECT_EQ(outsideSafetyZoneCount, propEventCount) << tcase.Name;
        } else {
            EXPECT_GT(insideSafetyZoneCount, 0) << tcase.Name;
            EXPECT_GT(outsideSafetyZoneCount, 0) << tcase.Name;
            EXPECT_EQ(insideSafetyZoneCount + outsideSafetyZoneCount, propEventCount)
                << tcase.Name;
        }
    }
}

TEST_F(VirtualHostTest, FakeTime_TicksInTime)
{
    static constexpr SInt64 NumTicks = 100;

    static constexpr double TicksPerSec = 1000;
    static constexpr auto TickSafetyZone = chrono::microseconds(100);

    static constexpr auto TickPeriod =
        chrono::nanoseconds(static_cast<SInt64>(1e9 / TicksPerSec));

    static constexpr auto StartTime = std::chrono::steady_clock::time_point(
        std::chrono::nanoseconds(1'000'000'000'000));

    virtParams.TicksPerSecond = TicksPerSec;
    virtParams.TickSafetyZone = TickSafetyZone;
    virtParams.SkipLateTicks = true;

    auto virtHost = std::make_shared<CustomVirtualHost>(tracer, virtParams);

    virtHost->SetFakeTime(StartTime - TickPeriod);

    SInt64 tickNum = 0;

    // ideal tick times
    for (int iter = 0; iter < NumTicks; iter++, tickNum++) {
        virtHost->AdvanceFakeTime(TickPeriod);

        const auto currentTime = StartTime + TickPeriod * tickNum;
        CHRONO_ASSERT_EQ(virtHost->GetCurrentTimeImpl(), currentTime);

        auto tick = virtHost->GetNextTickImpl(currentTime);
        ASSERT_TRUE(tick);
        EXPECT_EQ(tick->Number, tickNum);
        CHRONO_EXPECT_EQ(tick->Time, StartTime + TickPeriod * tickNum);

        ASSERT_TRUE(virtHost->DoNextTickImpl(currentTime, tick->Number));
    }

    // ticks come 1/4 period earlier (considered in-time)
    for (int iter = 0; iter < NumTicks; iter++, tickNum++) {
        virtHost->AdvanceFakeTime(TickPeriod - TickPeriod / 4);

        const auto currentTime = StartTime + TickPeriod * tickNum - TickPeriod / 4;
        CHRONO_ASSERT_EQ(virtHost->GetCurrentTimeImpl(), currentTime);

        auto tick = virtHost->GetNextTickImpl(currentTime);
        ASSERT_TRUE(tick);
        EXPECT_EQ(tick->Number, tickNum);
        CHRONO_EXPECT_EQ(tick->Time, StartTime + TickPeriod * tickNum);

        ASSERT_TRUE(virtHost->DoNextTickImpl(currentTime, tick->Number));

        virtHost->AdvanceFakeTime(TickPeriod / 4);
    }

    // ticks come 1/4 period later (considered in-time)
    virtHost->AdvanceFakeTime(TickPeriod / 4);

    for (int iter = 0; iter < NumTicks; iter++, tickNum++) {
        virtHost->AdvanceFakeTime(TickPeriod);

        const auto currentTime = StartTime + TickPeriod * tickNum + TickPeriod / 4;
        CHRONO_ASSERT_EQ(virtHost->GetCurrentTimeImpl(), currentTime);

        auto tick = virtHost->GetNextTickImpl(currentTime);
        ASSERT_TRUE(tick);
        EXPECT_EQ(tick->Number, tickNum);
        CHRONO_EXPECT_EQ(tick->Time, StartTime + TickPeriod * tickNum);

        ASSERT_TRUE(virtHost->DoNextTickImpl(currentTime, tick->Number));
    }
}

TEST_F(VirtualHostTest, FakeTime_LateTicks)
{
    static constexpr SInt64 NumIterations = 10;
    static constexpr SInt64 GoodTicks = 10;

    static constexpr double TicksPerSec = 1000;
    static constexpr auto TickSafetyZone = chrono::microseconds(100);

    static constexpr auto TickPeriod =
        chrono::nanoseconds(static_cast<SInt64>(1e9 / TicksPerSec));

    static constexpr auto StartTime = std::chrono::steady_clock::time_point(
        std::chrono::nanoseconds(1'000'000'000'000));

    virtParams.TicksPerSecond = TicksPerSec;
    virtParams.TickSafetyZone = TickSafetyZone;
    virtParams.SkipLateTicks = true;

    auto virtHost = std::make_shared<CustomVirtualHost>(tracer, virtParams);

    virtHost->SetFakeTime(StartTime);

    SInt64 tickNum = 0;

    for (int iter = 0; iter < NumIterations; iter++) {
        // ideal tick times
        for (int iter = 0; iter < GoodTicks; iter++) {
            const auto currentTime = StartTime + TickPeriod * tickNum;
            CHRONO_ASSERT_EQ(virtHost->GetCurrentTimeImpl(), currentTime);

            auto tick = virtHost->GetNextTickImpl(currentTime);
            ASSERT_TRUE(tick);
            EXPECT_EQ(tick->Number, tickNum);
            CHRONO_EXPECT_EQ(tick->Time, StartTime + TickPeriod * tickNum);

            // move counter to next tick
            ASSERT_TRUE(virtHost->DoNextTickImpl(currentTime, tick->Number));
            tickNum += 1;
            ASSERT_EQ(tickNum, tick->Number + 1);

            // move time to next tick
            virtHost->AdvanceFakeTime(TickPeriod);
        }

        // tick X comes 1+1/4 period later => skip to tick X+1
        {
            // now it's time of tick X+1
            virtHost->AdvanceFakeTime(TickPeriod);
            // and plus 1/4 period
            virtHost->AdvanceFakeTime(TickPeriod / 4);

            const auto currentTime =
                StartTime + TickPeriod * tickNum + TickPeriod + TickPeriod / 4;
            CHRONO_ASSERT_EQ(virtHost->GetCurrentTimeImpl(), currentTime);

            // next tick should be now X+1 instead of X
            auto tick = virtHost->GetNextTickImpl(currentTime);
            ASSERT_TRUE(tick);
            EXPECT_EQ(tick->Number, tickNum + 1);
            CHRONO_EXPECT_EQ(tick->Time, StartTime + TickPeriod * (tickNum + 1));

            // move counter to next tick
            ASSERT_TRUE(virtHost->DoNextTickImpl(currentTime, tick->Number));
            tickNum += 2;
            ASSERT_EQ(tickNum, tick->Number + 1);

            // move time to next tick
            virtHost->AdvanceFakeTime(TickPeriod * 3 / 4);
        }

        // tick Y comes 1+3/4 period later => skip to tick Y+2
        {
            // now it's time of tick Y+1
            virtHost->AdvanceFakeTime(TickPeriod);
            // and plus 3/4 period
            virtHost->AdvanceFakeTime(TickPeriod * 3 / 4);

            const auto currentTime =
                StartTime + TickPeriod * tickNum + TickPeriod + TickPeriod * 3 / 4;
            CHRONO_ASSERT_EQ(virtHost->GetCurrentTimeImpl(), currentTime);

            // next tick should be now Y+2 instead of Y
            auto tick = virtHost->GetNextTickImpl(currentTime);
            ASSERT_TRUE(tick);
            EXPECT_EQ(tick->Number, tickNum + 2);
            CHRONO_EXPECT_EQ(tick->Time, StartTime + TickPeriod * (tickNum + 2));

            // move counter to next tick
            ASSERT_TRUE(virtHost->DoNextTickImpl(currentTime, tickNum + 2));
            tickNum += 3;
            ASSERT_EQ(tickNum, tick->Number + 1);

            // move time to start of next tick
            virtHost->AdvanceFakeTime(TickPeriod + TickPeriod / 4);
        }
    }
}

TEST_F(VirtualHostTest, FakeTime_SafetyZone)
{
    static constexpr SInt64 MinSteps = 1000;
    static constexpr int TimeStepsPerTick = 10;

    static constexpr double TicksPerSec = 1000;

    static constexpr auto TickPeriod =
        chrono::nanoseconds(static_cast<SInt64>(1e9 / TicksPerSec));

    static constexpr auto TickSafetyZone = TickPeriod / TimeStepsPerTick * 3;

    static constexpr auto StartTime = std::chrono::steady_clock::time_point(
        std::chrono::nanoseconds(1'000'000'000'000));

    virtParams.TicksPerSecond = TicksPerSec;
    virtParams.TickSafetyZone = TickSafetyZone;
    virtParams.SkipLateTicks = true;

    auto virtHost = std::make_shared<CustomVirtualHost>(tracer, virtParams);

    virtHost->SetFakeTime(StartTime);

    SInt64 tickNum = 0;
    bool safetyZone = false;
    int tickAtStep = 3;

    int insideSafetyZoneCount = 0;
    int outsideSafetyZoneCount = 0;

    while (insideSafetyZoneCount < MinSteps || outsideSafetyZoneCount < MinSteps) {
        // At which step inside time span ticks actually occurs.
        tickAtStep = (tickAtStep + 3) % TimeStepsPerTick;

        auto currentTime = StartTime + TickPeriod * tickNum;

        // Divide tick time span into N steps.
        for (int stepNum = 0; stepNum < TimeStepsPerTick; stepNum++) {
            CHRONO_ASSERT_EQ(virtHost->GetCurrentTimeImpl(), currentTime);

            // On Nth step, tick occurs.
            if (stepNum == tickAtStep) {
                ASSERT_TRUE(virtHost->DoNextTickImpl(currentTime, tickNum));
                tickNum++;
                // Tick just occurred, leave safety zone
                safetyZone = false;
            }

            const auto nextTickTime = StartTime + TickPeriod * tickNum;

            if (stepNum < tickAtStep) {
                // Before stepNum == tickAtStep, we're ahead of next tick
                CHRONO_ASSERT_GE(currentTime, nextTickTime);
            } else {
                // After that, next tick is in future
                CHRONO_ASSERT_LT(currentTime, nextTickTime);
            }

            { // In both cases our formula should match GetNextTickImpl()
                auto tick = virtHost->GetNextTickImpl(currentTime);
                ASSERT_TRUE(tick);
                ASSERT_EQ(tick->Number, tickNum);
                CHRONO_ASSERT_EQ(tick->Time, nextTickTime);
            }

            if (tickNum == 0 || currentTime >= nextTickTime - TickSafetyZone) {
                // Enter safety zone of next tick
                if (!safetyZone) {
                    safetyZone = true;
                }
            }

            EXPECT_EQ(virtHost->IsSafetyZoneImpl(currentTime), safetyZone)
                << "tickNum=" << tickNum << " stepNum=" << stepNum;

            if (safetyZone) {
                insideSafetyZoneCount++;
            } else {
                outsideSafetyZoneCount++;
            }

            virtHost->AdvanceFakeTime(TickPeriod / TimeStepsPerTick);
            currentTime += TickPeriod / TimeStepsPerTick;
        }
    }
}

TEST_F(VirtualHostTest, Storage_WriteReadDelete)
{
    auto virtHost = std::make_shared<aspl::VirtualHost>(tracer);

    CFStringRef key =
        CFStringCreateWithCString(kCFAllocatorDefault, "test_key", kCFStringEncodingUTF8);
    CFStringRef value = CFStringCreateWithCString(
        kCFAllocatorDefault, "test_value", kCFStringEncodingUTF8);

    OSStatus writeStatus = virtHost->WriteToStorage(key, value);
    EXPECT_EQ(writeStatus, kAudioHardwareNoError);

    CFPropertyListRef readValue = nullptr;
    OSStatus readStatus = virtHost->CopyFromStorage(key, &readValue);
    EXPECT_EQ(readStatus, kAudioHardwareNoError);
    ASSERT_NE(readValue, nullptr);
    EXPECT_EQ(CFGetTypeID(readValue), CFStringGetTypeID());

    std::string originalStr, readStr;
    EXPECT_TRUE(aspl::Convert::FromFoundation(value, originalStr));
    EXPECT_TRUE(aspl::Convert::FromFoundation(readValue, readStr));
    EXPECT_EQ(originalStr, readStr);

    OSStatus deleteStatus = virtHost->DeleteFromStorage(key);
    EXPECT_EQ(deleteStatus, kAudioHardwareNoError);

    CFRelease(key);
    CFRelease(value);
    CFRelease(readValue);
}

TEST_F(VirtualHostTest, Storage_Overwrite)
{
    auto virtHost = std::make_shared<aspl::VirtualHost>(tracer);

    CFStringRef key = CFStringCreateWithCString(
        kCFAllocatorDefault, "overwrite_key", kCFStringEncodingUTF8);

    CFStringRef value1 = CFStringCreateWithCString(
        kCFAllocatorDefault, "first_value", kCFStringEncodingUTF8);
    CFStringRef value2 = CFStringCreateWithCString(
        kCFAllocatorDefault, "second_value", kCFStringEncodingUTF8);

    {
        OSStatus writeStatus = virtHost->WriteToStorage(key, value1);
        EXPECT_EQ(writeStatus, kAudioHardwareNoError);

        CFPropertyListRef readValue = nullptr;
        OSStatus readStatus = virtHost->CopyFromStorage(key, &readValue);
        EXPECT_EQ(readStatus, kAudioHardwareNoError);
        ASSERT_NE(readValue, nullptr);

        std::string readStr;
        EXPECT_TRUE(aspl::Convert::FromFoundation(readValue, readStr));
        EXPECT_EQ(readStr, "first_value");

        CFRelease(readValue);
    }

    {
        OSStatus writeStatus = virtHost->WriteToStorage(key, value2);
        EXPECT_EQ(writeStatus, kAudioHardwareNoError);

        CFPropertyListRef readValue = nullptr;
        OSStatus readStatus = virtHost->CopyFromStorage(key, &readValue);
        EXPECT_EQ(readStatus, kAudioHardwareNoError);
        ASSERT_NE(readValue, nullptr);

        std::string readStr;
        EXPECT_TRUE(aspl::Convert::FromFoundation(readValue, readStr));
        EXPECT_EQ(readStr, "second_value");

        CFRelease(readValue);
    }

    CFRelease(key);
    CFRelease(value1);
    CFRelease(value2);
}

TEST_F(VirtualHostTest, Storage_NotFound)
{
    auto virtHost = std::make_shared<aspl::VirtualHost>(tracer);

    { // read non-existent
        CFStringRef key = CFStringCreateWithCString(
            kCFAllocatorDefault, "non_existent", kCFStringEncodingUTF8);

        CFPropertyListRef readValue = nullptr;
        OSStatus readStatus = virtHost->CopyFromStorage(key, &readValue);
        EXPECT_EQ(readStatus, kAudioHardwareUnknownPropertyError);
        EXPECT_EQ(readValue, nullptr);

        CFRelease(key);
    }

    { // delete non-existent
        CFStringRef key = CFStringCreateWithCString(
            kCFAllocatorDefault, "non_existent", kCFStringEncodingUTF8);

        OSStatus deleteStatus = virtHost->DeleteFromStorage(key);
        EXPECT_EQ(deleteStatus, kAudioHardwareUnknownPropertyError);

        CFRelease(key);
    }
}

TEST_F(VirtualHostTest, Storage_Types)
{
    auto virtHost = std::make_shared<aspl::VirtualHost>(tracer);

    std::shared_ptr<aspl::Context> context = std::make_shared<aspl::Context>(tracer);
    context->Host = virtHost->GetReference();

    std::shared_ptr<aspl::Storage> storage = std::make_shared<aspl::Storage>(context);

    { // string type
        std::string testValue = "test_string_value";
        EXPECT_TRUE(storage->WriteString("string_key", testValue));

        auto [readValue, success] = storage->ReadString("string_key");
        EXPECT_TRUE(success);
        EXPECT_EQ(readValue, testValue);

        EXPECT_TRUE(storage->Delete("string_key"));
    }

    { // boolean type
        bool testValue = true;
        EXPECT_TRUE(storage->WriteBoolean("bool_key", testValue));

        auto [readValue, success] = storage->ReadBoolean("bool_key");
        EXPECT_TRUE(success);
        EXPECT_EQ(readValue, testValue);

        EXPECT_TRUE(storage->Delete("bool_key"));
    }

    { // int type
        SInt64 testValue = -12345678901234LL;
        EXPECT_TRUE(storage->WriteInt("int_key", testValue));

        auto [readValue, success] = storage->ReadInt("int_key");
        EXPECT_TRUE(success);
        EXPECT_EQ(readValue, testValue);

        EXPECT_TRUE(storage->Delete("int_key"));
    }

    { // float type
        Float64 testValue = 3.14159265359;
        EXPECT_TRUE(storage->WriteFloat("float_key", testValue));

        auto [readValue, success] = storage->ReadFloat("float_key");
        EXPECT_TRUE(success);
        EXPECT_DOUBLE_EQ(readValue, testValue);

        EXPECT_TRUE(storage->Delete("float_key"));
    }

    { // bytes type
        std::vector<UInt8> testValue = {0x01, 0x02, 0x03, 0xFF, 0x00};
        EXPECT_TRUE(storage->WriteBytes("bytes_key", testValue));

        auto [readValue, success] = storage->ReadBytes("bytes_key");
        EXPECT_TRUE(success);
        EXPECT_EQ(readValue, testValue);

        EXPECT_TRUE(storage->Delete("bytes_key"));
    }

    { // custom type
        CFStringRef testValue = CFStringCreateWithCString(
            kCFAllocatorDefault, "custom_test_value", kCFStringEncodingUTF8);

        EXPECT_TRUE(storage->WriteCustom("custom_key", testValue));

        auto [readValue, success] = storage->ReadCustom("custom_key");
        EXPECT_TRUE(success);
        ASSERT_NE(readValue, nullptr);
        EXPECT_EQ(CFGetTypeID(readValue), CFStringGetTypeID());

        std::string originalStr, readStr;
        EXPECT_TRUE(aspl::Convert::FromFoundation(testValue, originalStr));
        EXPECT_TRUE(aspl::Convert::FromFoundation(readValue, readStr));
        EXPECT_EQ(originalStr, readStr);

        EXPECT_TRUE(storage->Delete("custom_key"));

        CFRelease(testValue);
        CFRelease(readValue);
    }
}

TEST_F(VirtualHostTest, Multithreaded)
{
    static constexpr double TicksPerSec = 500;
    static constexpr auto TickSafetyZone = chrono::microseconds(400);
    static constexpr auto EventDelay = chrono::microseconds(50);

    static constexpr UInt64 MinEventCount = 500;

    static constexpr AudioObjectID TestObjectID = 111;
    static constexpr AudioObjectID TestDeviceID = 222;
    static constexpr UInt64 TestDeviceAction = 333;
    static const char* TestStorageKey = "test_key";
    static const char* TestStorageValue = "test_value";

    std::atomic<bool> stopFlag = false;
    std::atomic<UInt64> propertyChangeCount = 0;
    std::atomic<UInt64> configRequestCount = 0;
    std::atomic<UInt64> storageChangeCount = 0;
    std::atomic<UInt64> tickEventCount = 0;

    virtParams.TicksPerSecond = TicksPerSec;
    virtParams.TickSafetyZone = TickSafetyZone;
    virtParams.SkipLateTicks = true;

    auto virtHost = std::make_shared<aspl::VirtualHost>(tracer, virtParams);

    // producer thread 1: property changes
    std::thread propertyThread([&]() {
        AudioObjectPropertyAddress address = {
            'prop', kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
        while (!stopFlag) {
            virtHost->PropertiesChanged(TestObjectID, 1, &address);
            RandomDelay(EventDelay);
        }
    });

    // producer thread 2: device configuration requests
    std::thread configThread([&]() {
        while (!stopFlag) {
            virtHost->RequestDeviceConfigurationChange(
                TestDeviceID, TestDeviceAction, nullptr);
            RandomDelay(EventDelay);
        }
    });

    // producer thread 3: storage operations
    std::thread storageThread([&]() {
        CFStringRef key = CFStringCreateWithCString(
            kCFAllocatorDefault, TestStorageKey, kCFStringEncodingUTF8);
        CFStringRef value = CFStringCreateWithCString(
            kCFAllocatorDefault, TestStorageValue, kCFStringEncodingUTF8);

        while (!stopFlag) {
            virtHost->WriteToStorage(key, value);
            RandomDelay(EventDelay / 3);

            CFPropertyListRef readValue = nullptr;
            virtHost->CopyFromStorage(key, &readValue);
            if (readValue) {
                CFRelease(readValue);
            }
            RandomDelay(EventDelay / 3);

            virtHost->DeleteFromStorage(key);
            RandomDelay(EventDelay / 3);
        }

        CFRelease(key);
        CFRelease(value);
    });

    // consumer thread
    std::thread consumerThread([&]() {
        SInt64 lastTickNumber = -1;

        while (true) {
            auto event = virtHost->WaitEvent();
            if (!event) {
                // AbortEvents() called
                break;
            }

            switch (event->EventType) {
            case aspl::VirtualEventType::ObjectPropertyChange:
                ASSERT_TRUE(event->ObjectPropertyChange.has_value());
                EXPECT_EQ(event->ObjectPropertyChange->ObjectID, TestObjectID);
                propertyChangeCount++;
                break;

            case aspl::VirtualEventType::DeviceConfigurationRequest:
                ASSERT_TRUE(event->DeviceConfigurationRequest.has_value());
                EXPECT_EQ(event->DeviceConfigurationRequest->DeviceID, TestDeviceID);
                configRequestCount++;
                break;

            case aspl::VirtualEventType::StorageDataChange:
                ASSERT_TRUE(event->StorageDataChange.has_value());
                EXPECT_EQ(event->StorageDataChange->StorageKey, TestStorageKey);
                storageChangeCount++;
                break;

            case aspl::VirtualEventType::IOTimerTick:
                ASSERT_TRUE(event->IOTimerTick);
                EXPECT_GT(event->IOTimerTick->Number, lastTickNumber);
                lastTickNumber = event->IOTimerTick->Number;
                tickEventCount++;
                break;

            case aspl::VirtualEventType::CustomEvent:
            default:
                FAIL() << "Unexpected event type";
                break;
            }
        }
    });

    // wait until minimum events of each type accumulated
    while (propertyChangeCount < MinEventCount || configRequestCount < MinEventCount ||
           storageChangeCount < MinEventCount || tickEventCount < MinEventCount) {
        ShortDelay();
    }

    // stop producers and abort events
    stopFlag = true;
    virtHost->AbortEvents();

    // join all threads
    propertyThread.join();
    configThread.join();
    storageThread.join();
    consumerThread.join();

    // verify we got at least minimum count of each
    EXPECT_GE(propertyChangeCount.load(), MinEventCount);
    EXPECT_GE(configRequestCount.load(), MinEventCount);
    EXPECT_GE(storageChangeCount.load(), MinEventCount);
    EXPECT_GE(tickEventCount.load(), MinEventCount);
}

TEST_F(VirtualHostTest, Integration)
{
    static const char* TestStorageKey = "test_key";
    static const char* TestStorageValue = "test_value";

    auto virtHost = std::make_shared<aspl::VirtualHost>(tracer);

    auto context = std::make_shared<aspl::Context>(tracer);
    context->Host = virtHost->GetReference();

    auto storage = std::make_shared<aspl::Storage>(context);
    auto plugin = std::make_shared<aspl::Plugin>(context);

    std::atomic<int> totalGeneratedEvents = 0;
    std::atomic<int> totalProcessedEvents = 0;

    std::map<aspl::VirtualEventType, std::atomic<int>> generatedEvents;
    std::map<aspl::VirtualEventType, std::atomic<int>> processedEvents;

    auto eventTypes = {
        aspl::VirtualEventType::ObjectPropertyChange,
        aspl::VirtualEventType::DeviceConfigurationRequest,
        aspl::VirtualEventType::StorageDataChange,
    };

    for (auto evType : eventTypes) {
        generatedEvents.emplace(evType, 0);
        processedEvents.emplace(evType, 0);
    }

    auto addGeneratedEvents = [&](aspl::VirtualEventType evType, int evCnt) {
        generatedEvents.at(evType) += evCnt;
        totalGeneratedEvents += evCnt;
    };

    auto addProcessedEvents = [&](aspl::VirtualEventType evType, int evCnt) {
        processedEvents.at(evType) += evCnt;
        totalProcessedEvents += evCnt;
    };

    // thread 1: virtual plugin host
    std::thread hostThread([&]() {
        for (;;) {
            auto event = virtHost->WaitEvent();
            if (!event) {
                break; // AbortEvents() called
            }

            switch (event->EventType) {
            case aspl::VirtualEventType::ObjectPropertyChange:
            {
                ASSERT_TRUE(event->ObjectPropertyChange);
                EXPECT_NE(event->ObjectPropertyChange->ObjectID, 0);
                EXPECT_NE(event->ObjectPropertyChange->PropertyAddress.mSelector, 0);
            } break;

            case aspl::VirtualEventType::DeviceConfigurationRequest:
            {
                ASSERT_TRUE(event->DeviceConfigurationRequest);
                EXPECT_NE(event->DeviceConfigurationRequest->DeviceID, 0u);

                // complete the configuration request
                auto device =
                    plugin->GetDeviceByID(event->DeviceConfigurationRequest->DeviceID);
                ASSERT_TRUE(device);
                const OSStatus status = device->PerformConfigurationChange(
                    event->DeviceConfigurationRequest->DeviceID,
                    event->DeviceConfigurationRequest->ChangeAction,
                    event->DeviceConfigurationRequest->ChangeInfo);
                EXPECT_EQ(status, kAudioHardwareNoError);
            } break;

            case aspl::VirtualEventType::StorageDataChange:
            {
                ASSERT_TRUE(event->StorageDataChange);
                EXPECT_FALSE(event->StorageDataChange->StorageKey.empty());
            } break;

            case aspl::VirtualEventType::IOTimerTick:
            case aspl::VirtualEventType::CustomEvent:
            default:
                FAIL() << "Unexpected event type";
                break;
            }

            addProcessedEvents(event->EventType, 1);
        }
    });

    // thread 2: plugin thread
    std::thread pluginThread([&]() {
        // default parameters
        aspl::DeviceParameters deviceParams;
        deviceParams.SampleRate = 44100;

        // triggers ObjectPropertyChange:
        //   kAudioObjectPropertyOwnedObjects, kAudioPlugInPropertyDeviceList
        auto device = std::make_shared<aspl::Device>(context, deviceParams);
        plugin->AddDevice(device);
        addGeneratedEvents(aspl::VirtualEventType::ObjectPropertyChange, 2);

        // triggers DeviceConfigurationRequest
        auto stream = device->AddStreamAsync(aspl::Direction::Output);
        ASSERT_TRUE(stream);
        addGeneratedEvents(aspl::VirtualEventType::DeviceConfigurationRequest, 1);

        // triggers DeviceConfigurationRequest
        std::vector<AudioValueRange> availableRates;
        availableRates.push_back(AudioValueRange{44100, 44100});
        availableRates.push_back(AudioValueRange{48000, 48000});
        device->SetAvailableSampleRatesAsync(availableRates);
        addGeneratedEvents(aspl::VirtualEventType::DeviceConfigurationRequest, 1);

        // wait until previous events are processed
        while (totalProcessedEvents < totalGeneratedEvents) {
            ShortDelay();
        }

        {
            // triggers DeviceConfigurationRequest
            const OSStatus status = device->SetNominalSampleRateAsync(48000);
            ASSERT_EQ(status, kAudioHardwareNoError);
            addGeneratedEvents(aspl::VirtualEventType::DeviceConfigurationRequest, 1);
        }

        { // triggers ObjectPropertyChange (kAudioDevicePropertyIsHidden)
            const OSStatus status = device->SetIsHidden(true);
            ASSERT_EQ(status, kAudioHardwareNoError);
            addGeneratedEvents(aspl::VirtualEventType::ObjectPropertyChange, 1);
        }

        { // triggers StorageDataChange
            const bool success = storage->WriteString(TestStorageKey, TestStorageValue);
            ASSERT_TRUE(success);
            addGeneratedEvents(aspl::VirtualEventType::StorageDataChange, 1);
        }

        { // triggers StorageDataChange
            const bool success = storage->Delete(TestStorageKey);
            ASSERT_TRUE(success);
            addGeneratedEvents(aspl::VirtualEventType::StorageDataChange, 1);
        }

        // wait until previous events are processed
        while (totalProcessedEvents < totalGeneratedEvents) {
            ShortDelay();
        }

        // triggers ObjectPropertyChange:
        //   kAudioObjectPropertyOwnedObjects, kAudioPlugInPropertyDeviceList
        plugin->RemoveDevice(device);
        addGeneratedEvents(aspl::VirtualEventType::ObjectPropertyChange, 2);

        // wait until previous events are processed
        while (totalProcessedEvents < totalGeneratedEvents) {
            ShortDelay();
        }
    });

    pluginThread.join();

    // wait until all events are processed
    while (totalProcessedEvents < totalGeneratedEvents) {
        ShortDelay();
    }

    EXPECT_FALSE(virtHost->HasAvailEvents());

    virtHost->AbortEvents();
    hostThread.join();

    // verify event counts
    EXPECT_EQ(totalGeneratedEvents, totalProcessedEvents);

    for (auto evType : eventTypes) {
        EXPECT_EQ(processedEvents.at(evType), generatedEvents.at(evType))
            << "  evType=" << static_cast<int>(evType);
    }
}
