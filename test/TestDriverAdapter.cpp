// Copyright (c) libASPL authors
// Licensed under MIT

#include "aspl/Context.hpp"
#include "aspl/Driver.hpp"
#include "aspl/DriverAdapter.hpp"
#include "aspl/Plugin.hpp"
#include "aspl/VirtualHost.hpp"

#include "Convert.hpp"
#include "MockTracer.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace {

class TestControlRequestHandler : public aspl::ControlRequestHandler
{
public:
    TestControlRequestHandler() = default;

    OSStatus OnStartIO() override
    {
        startIOCallCount++;
        return kAudioHardwareNoError;
    }

    void OnStopIO() override
    {
        stopIOCallCount++;
    }

    std::shared_ptr<aspl::Client> OnAddClient(const aspl::ClientInfo& clientInfo) override
    {
        lastAddedClientInfo = clientInfo;
        addClientCallCount++;
        return std::make_shared<aspl::Client>(clientInfo);
    }

    void OnRemoveClient(std::shared_ptr<aspl::Client> client) override
    {
        lastRemovedClient = client;
        removeClientCallCount++;
    }

    aspl::ClientInfo lastAddedClientInfo;
    std::shared_ptr<aspl::Client> lastRemovedClient;
    int addClientCallCount = 0;
    int removeClientCallCount = 0;
    int startIOCallCount = 0;
    int stopIOCallCount = 0;
};

class TestIORequestHandler : public aspl::IORequestHandler
{
public:
    TestIORequestHandler() = default;

    void OnReadClientInput(const std::shared_ptr<aspl::Client>& client,
        const std::shared_ptr<aspl::Stream>& stream,
        Float64 zeroTimestamp,
        Float64 timestamp,
        void* bytes,
        UInt32 bytesCount) override
    {
        readClientInputCallCount++;
        lastReadClient = client;
        lastReadStream = stream;
        lastReadZeroTimestamp = zeroTimestamp;
        lastReadTimestamp = timestamp;
        lastReadBytesCount = bytesCount;

        // Fill with test pattern
        if (bytes && bytesCount > 0) {
            memset(bytes, 0x42, bytesCount);
        }
    }

    void OnWriteClientOutput(const std::shared_ptr<aspl::Client>& client,
        const std::shared_ptr<aspl::Stream>& stream,
        Float64 zeroTimestamp,
        Float64 timestamp,
        const Float32* frames,
        UInt32 frameCount,
        UInt32 channelCount) override
    {
        writeClientOutputCallCount++;
        lastWriteClient = client;
        lastWriteStream = stream;
        lastWriteZeroTimestamp = zeroTimestamp;
        lastWriteTimestamp = timestamp;
        lastWriteFrameCount = frameCount;
        lastWriteChannelCount = channelCount;
    }

    std::shared_ptr<aspl::Client> lastReadClient;
    std::shared_ptr<aspl::Stream> lastReadStream;
    Float64 lastReadZeroTimestamp = 0;
    Float64 lastReadTimestamp = 0;
    UInt32 lastReadBytesCount = 0;
    int readClientInputCallCount = 0;

    std::shared_ptr<aspl::Client> lastWriteClient;
    std::shared_ptr<aspl::Stream> lastWriteStream;
    Float64 lastWriteZeroTimestamp = 0;
    Float64 lastWriteTimestamp = 0;
    UInt32 lastWriteFrameCount = 0;
    UInt32 lastWriteChannelCount = 0;
    int writeClientOutputCallCount = 0;
};

} // anonymous namespace

struct DriverAdapterTest : testing::Test
{
    std::shared_ptr<aspl::Tracer> tracer;
    std::shared_ptr<aspl::Context> context;

    std::shared_ptr<aspl::Plugin> plugin;
    std::shared_ptr<aspl::Driver> driver;

    std::shared_ptr<aspl::Device> device;
    std::shared_ptr<aspl::Stream> inputStream;
    std::shared_ptr<aspl::Stream> outputStream;

    std::string customProp;
    std::shared_ptr<TestControlRequestHandler> controlHandler;
    std::shared_ptr<TestIORequestHandler> ioHandler;

    std::shared_ptr<aspl::VirtualHost> virtHost;

    void SetUp() override
    {
        tracer = std::make_shared<MockTracer>();
        context = std::make_shared<aspl::Context>(tracer);

        plugin = std::make_shared<aspl::Plugin>(context);
        driver = std::make_shared<aspl::Driver>(context, plugin);

        aspl::DeviceParameters deviceParams;
        deviceParams.EnableMixing = false;
        deviceParams.EnableRealtimeTracing = true;
        device = std::make_shared<aspl::Device>(context, deviceParams);

        plugin->AddDevice(device);

        inputStream = device->AddStreamAsync(aspl::Direction::Input);
        outputStream = device->AddStreamAsync(aspl::Direction::Output);

        outputStream->RegisterCustomProperty(
            'ASPL',
            [this]() -> CFStringRef {
                CFStringRef result = nullptr;
                aspl::Convert::ToFoundation(customProp, result);
                return result;
            },
            [this](CFStringRef value) {
                aspl::Convert::FromFoundation(value, customProp);
            });

        controlHandler = std::make_shared<TestControlRequestHandler>();
        device->SetControlHandler(controlHandler);

        ioHandler = std::make_shared<TestIORequestHandler>();
        device->SetIOHandler(ioHandler);

        virtHost = std::make_shared<aspl::VirtualHost>(tracer);
    }

    void ExpectEvents(aspl::VirtualEventType evType, int evCount)
    {
        EXPECT_TRUE(virtHost->HasAvailEvents());

        for (int i = 0; i < evCount; i++) {
            auto event = virtHost->ReadEvent();
            ASSERT_TRUE(event);
            EXPECT_EQ(event->EventType, evType);
        }
    }

    void ExpectNoEvents()
    {
        EXPECT_FALSE(virtHost->HasAvailEvents());
    }
};

TEST_F(DriverAdapterTest, Initialize_VirtualHost)
{
    auto driverAdapter =
        std::make_shared<aspl::DriverAdapter>(driver->GetReference(), tracer);

    ASSERT_EQ(context->Host.load(), nullptr);

    OSStatus status = driverAdapter->Initialize(virtHost);
    ASSERT_EQ(kAudioHardwareNoError, status);

    EXPECT_EQ(context->Host.load(), virtHost->GetReference());
    EXPECT_FALSE(virtHost->HasAvailEvents());

    EXPECT_EQ(virtHost.use_count(), 2);

    ExpectNoEvents();
}

TEST_F(DriverAdapterTest, Initialize_HostRef)
{
    auto driverAdapter =
        std::make_shared<aspl::DriverAdapter>(driver->GetReference(), tracer);

    ASSERT_EQ(context->Host.load(), nullptr);

    OSStatus status = driverAdapter->Initialize(virtHost->GetReference());
    ASSERT_EQ(kAudioHardwareNoError, status);

    EXPECT_EQ(context->Host.load(), virtHost->GetReference());
    EXPECT_FALSE(virtHost->HasAvailEvents());

    EXPECT_EQ(virtHost.use_count(), 1);

    ExpectNoEvents();
}

TEST_F(DriverAdapterTest, AddRemoveClient)
{
    auto driverAdapter =
        std::make_shared<aspl::DriverAdapter>(driver->GetReference(), tracer);

    aspl::ClientInfo clientInfo;
    clientInfo.ClientID = 123;
    clientInfo.ProcessID = 456;
    clientInfo.IsNativeEndian = true;
    clientInfo.BundleID = "test.bundle";

    { // initialize
        ASSERT_EQ(driverAdapter->Initialize(virtHost), kAudioHardwareNoError);
        ExpectNoEvents();
    }

    { // add client
        ASSERT_EQ(driverAdapter->AddDeviceClient(device->GetID(), clientInfo),
            kAudioHardwareNoError);

        EXPECT_EQ(controlHandler->addClientCallCount, 1);
        EXPECT_EQ(controlHandler->lastAddedClientInfo.ClientID, 123);
        EXPECT_EQ(controlHandler->lastAddedClientInfo.ProcessID, 456);
        EXPECT_EQ(controlHandler->lastAddedClientInfo.IsNativeEndian, true);
        EXPECT_EQ(controlHandler->lastAddedClientInfo.BundleID, "test.bundle");

        ExpectNoEvents();
    }

    { // remove client
        ASSERT_EQ(driverAdapter->RemoveDeviceClient(device->GetID(), clientInfo),
            kAudioHardwareNoError);

        EXPECT_EQ(controlHandler->removeClientCallCount, 1);
        ASSERT_TRUE(controlHandler->lastRemovedClient);
        EXPECT_EQ(controlHandler->lastRemovedClient->GetClientID(), 123);
        EXPECT_EQ(controlHandler->lastRemovedClient->GetProcessID(), 456);
        EXPECT_EQ(controlHandler->lastRemovedClient->GetIsNativeEndian(), true);
        EXPECT_EQ(controlHandler->lastRemovedClient->GetBundleID(), "test.bundle");

        ExpectNoEvents();
    }
}

TEST_F(DriverAdapterTest, ConfigurationChange)
{
    auto driverAdapter =
        std::make_shared<aspl::DriverAdapter>(driver->GetReference(), tracer);

    const UInt64 changeAction = 12345;
    void* changeInfo = reinterpret_cast<void*>(0xDEADBEEF);

    ASSERT_EQ(driverAdapter->Initialize(virtHost), kAudioHardwareNoError);
    ExpectNoEvents();

    { // perform configuration change
        ASSERT_EQ(driverAdapter->PerformDeviceConfigurationChange(
                      device->GetID(), changeAction, changeInfo),
            kAudioHardwareNoError);

        ExpectNoEvents();
    }

    { // abort configuration change
        ASSERT_EQ(driverAdapter->AbortDeviceConfigurationChange(
                      device->GetID(), changeAction, changeInfo),
            kAudioHardwareNoError);

        ExpectNoEvents();
    }
}

TEST_F(DriverAdapterTest, Properties_HasProperty)
{
    auto driverAdapter =
        std::make_shared<aspl::DriverAdapter>(driver->GetReference(), tracer);

    ASSERT_EQ(driverAdapter->Initialize(virtHost), kAudioHardwareNoError);

    { // kAudioObjectPropertyName
        EXPECT_TRUE(
            driverAdapter->HasProperty(device->GetID(), 0, kAudioObjectPropertyName));
    }

    { // kAudioDevicePropertyNominalSampleRate
        EXPECT_TRUE(driverAdapter->HasProperty(
            device->GetID(), 0, kAudioDevicePropertyNominalSampleRate));
    }

    { // 'fake'
        EXPECT_FALSE(driverAdapter->HasProperty(device->GetID(), 0, 'fake'));
    }
}

TEST_F(DriverAdapterTest, Properties_IsSettable)
{
    auto driverAdapter =
        std::make_shared<aspl::DriverAdapter>(driver->GetReference(), tracer);

    ASSERT_EQ(driverAdapter->Initialize(virtHost), kAudioHardwareNoError);

    { // kAudioObjectPropertyName
        auto [status, settable] = driverAdapter->IsPropertySettable(
            device->GetID(), 0, kAudioObjectPropertyName);

        EXPECT_EQ(status, kAudioHardwareNoError);
        EXPECT_FALSE(settable);
    }

    { // kAudioDevicePropertyNominalSampleRate
        auto [status, settable] = driverAdapter->IsPropertySettable(
            device->GetID(), 0, kAudioDevicePropertyNominalSampleRate);

        EXPECT_EQ(status, kAudioHardwareNoError);
        EXPECT_TRUE(settable);
    }

    { // custom property
        auto [status, settable] =
            driverAdapter->IsPropertySettable(outputStream->GetID(), 0, 'ASPL');

        EXPECT_EQ(status, kAudioHardwareNoError);
        EXPECT_TRUE(settable);
    }
}

TEST_F(DriverAdapterTest, Properties_Getters)
{
    auto driverAdapter =
        std::make_shared<aspl::DriverAdapter>(driver->GetReference(), tracer);

    ASSERT_EQ(driverAdapter->Initialize(virtHost), kAudioHardwareNoError);

    { // GetScalarProperty
        auto [status, sampleRate] = driverAdapter->GetScalarProperty<Float64>(
            device->GetID(), 0, kAudioDevicePropertyNominalSampleRate);

        EXPECT_EQ(status, kAudioHardwareNoError);
        EXPECT_EQ(sampleRate, 44100.0);
    }

    { // GetStringProperty
        auto [status, name] = driverAdapter->GetStringProperty(
            device->GetID(), 0, kAudioObjectPropertyName);

        EXPECT_EQ(status, kAudioHardwareNoError);
        EXPECT_FALSE(name.empty());
    }

    { // GetVectorProperty
        auto [status, ranges] = driverAdapter->GetVectorProperty<AudioValueRange>(
            device->GetID(), 0, kAudioDevicePropertyAvailableNominalSampleRates);

        EXPECT_EQ(status, kAudioHardwareNoError);
        EXPECT_FALSE(ranges.empty());
    }

    { // GetCFProperty
        auto [status, nameRef] = driverAdapter->GetCFProperty<CFStringRef>(
            device->GetID(), 0, kAudioObjectPropertyName);

        EXPECT_EQ(status, kAudioHardwareNoError);
        EXPECT_TRUE(nameRef);

        std::string nameStr;
        aspl::Convert::FromFoundation(nameRef.get(), nameStr);
        EXPECT_FALSE(nameStr.empty());
    }
}

TEST_F(DriverAdapterTest, Properties_Setters)
{
    auto driverAdapter =
        std::make_shared<aspl::DriverAdapter>(driver->GetReference(), tracer);

    ASSERT_EQ(driverAdapter->Initialize(virtHost), kAudioHardwareNoError);

    { // SetScalarProperty
        const UInt32 isActive = 1;
        OSStatus status = driverAdapter->SetScalarProperty<UInt32>(
            outputStream->GetID(), 0, isActive, kAudioStreamPropertyIsActive);
        EXPECT_EQ(status, kAudioHardwareNoError);

        auto [getStatus, getIsActive] = driverAdapter->GetScalarProperty<UInt32>(
            outputStream->GetID(), 0, kAudioStreamPropertyIsActive);
        EXPECT_EQ(getStatus, kAudioHardwareNoError);
        EXPECT_EQ(getIsActive, isActive);
    }

    { // SetStringProperty
        const std::string testValue = "foo";
        OSStatus status =
            driverAdapter->SetStringProperty(outputStream->GetID(), 0, testValue, 'ASPL');
        EXPECT_EQ(status, kAudioHardwareNoError);

        auto [getStatus, getString] =
            driverAdapter->GetStringProperty(outputStream->GetID(), 0, 'ASPL');
        EXPECT_EQ(getStatus, kAudioHardwareNoError);
        EXPECT_EQ(getString, testValue);
    }

    { // SetCFProperty
        CFStringRef cfStringValue = nullptr;
        aspl::Convert::ToFoundation(std::string("bar"), cfStringValue);

        OSStatus status = driverAdapter->SetCFProperty<CFStringRef>(
            outputStream->GetID(), 0, cfStringValue, 'ASPL');
        EXPECT_EQ(status, kAudioHardwareNoError);

        auto [getStatus, getCFString] =
            driverAdapter->GetCFProperty<CFStringRef>(outputStream->GetID(), 0, 'ASPL');
        EXPECT_EQ(getStatus, kAudioHardwareNoError);
        EXPECT_TRUE(getCFString);

        std::string resultString;
        aspl::Convert::FromFoundation(getCFString.get(), resultString);
        EXPECT_EQ(resultString, "bar");

        if (cfStringValue) {
            CFRelease(cfStringValue);
        }
    }
}

TEST_F(DriverAdapterTest, Properties_RawData)
{
    auto driverAdapter =
        std::make_shared<aspl::DriverAdapter>(driver->GetReference(), tracer);

    ASSERT_EQ(driverAdapter->Initialize(virtHost), kAudioHardwareNoError);

    { // kAudioDevicePropertyAvailableNominalSampleRates
        auto [sizeStatus, dataSize] = driverAdapter->GetPropertySize(
            device->GetID(), 0, kAudioDevicePropertyAvailableNominalSampleRates);
        EXPECT_EQ(sizeStatus, kAudioHardwareNoError);
        EXPECT_GT(dataSize, 0);

        std::vector<UInt8> buffer(dataSize);
        size_t actualSize = dataSize;
        OSStatus dataStatus = driverAdapter->GetPropertyData(device->GetID(),
            0,
            buffer.data(),
            &actualSize,
            kAudioDevicePropertyAvailableNominalSampleRates);
        EXPECT_EQ(dataStatus, kAudioHardwareNoError);
        EXPECT_EQ(actualSize, dataSize);
        EXPECT_GE(actualSize, sizeof(AudioValueRange));
    }

    { // kAudioStreamPropertyIsActive
        const UInt32 isActive = 1;
        OSStatus status = driverAdapter->SetPropertyData(outputStream->GetID(),
            0,
            &isActive,
            sizeof(isActive),
            kAudioStreamPropertyIsActive);
        EXPECT_EQ(status, kAudioHardwareNoError);

        UInt32 retrievedValue = 0;
        size_t retrievedSize = sizeof(retrievedValue);
        OSStatus getStatus = driverAdapter->GetPropertyData(outputStream->GetID(),
            0,
            &retrievedValue,
            &retrievedSize,
            kAudioStreamPropertyIsActive);
        EXPECT_EQ(getStatus, kAudioHardwareNoError);
        EXPECT_EQ(retrievedSize, sizeof(retrievedValue));
        EXPECT_EQ(retrievedValue, isActive);
    }
}

TEST_F(DriverAdapterTest, IO_StartStop)
{
    auto driverAdapter =
        std::make_shared<aspl::DriverAdapter>(driver->GetReference(), tracer);

    const UInt32 clientID = 123;

    ASSERT_EQ(driverAdapter->Initialize(virtHost), kAudioHardwareNoError);

    ASSERT_EQ(driverAdapter->StartIO(device->GetID(), clientID), kAudioHardwareNoError);
    ASSERT_EQ(driverAdapter->StopIO(device->GetID(), clientID), kAudioHardwareNoError);

    EXPECT_EQ(controlHandler->startIOCallCount, 1);
    EXPECT_EQ(controlHandler->stopIOCallCount, 1);
}

TEST_F(DriverAdapterTest, IO_ReadClientInput)
{
    auto driverAdapter =
        std::make_shared<aspl::DriverAdapter>(driver->GetReference(), tracer);

    const UInt32 clientID = 123;
    const UInt32 bytesCount = 1024;
    std::vector<UInt8> buffer(bytesCount);

    aspl::DriverAdapter::IOCycle ioCycle;
    ioCycle.CycleCounter = 1;
    ioCycle.BatchFrameCount = 256;
    ioCycle.BatchSampleTime = 0.0;
    ioCycle.BatchHostTime = 1000;
    ioCycle.CurrentSampleTime = 0.0;
    ioCycle.CurrentHostTime = 1000;
    ioCycle.HostTicksPerFrame = 100.0;

    ASSERT_EQ(driverAdapter->Initialize(virtHost), kAudioHardwareNoError);
    ASSERT_EQ(driverAdapter->StartIO(device->GetID(), clientID), kAudioHardwareNoError);

    // check initial state
    EXPECT_EQ(ioHandler->readClientInputCallCount, 0);

    // perform multiple read operations
    for (int i = 0; i < 3; i++) {
        ioCycle.CycleCounter = i + 1;
        ioCycle.BatchSampleTime = i * 256.0;
        ioCycle.CurrentSampleTime = i * 256.0;

        // check WillReadClientInput before each cycle
        {
            auto [status, willDo, willDoInPlace] =
                driverAdapter->WillReadClientInput(device->GetID(), clientID);
            EXPECT_EQ(status, kAudioHardwareNoError);
            EXPECT_TRUE(willDo);
            EXPECT_TRUE(willDoInPlace);
        }

        OSStatus status = driverAdapter->ReadClientInput(device->GetID(),
            inputStream->GetID(),
            clientID,
            ioCycle,
            buffer.data(),
            bytesCount);

        EXPECT_EQ(status, kAudioHardwareNoError);
        EXPECT_EQ(ioHandler->readClientInputCallCount, i + 1);

        // check that handler was called with correct parameters
        EXPECT_EQ(ioHandler->lastReadBytesCount, bytesCount);
        EXPECT_EQ(ioHandler->lastReadTimestamp, i * 256.0);
        EXPECT_TRUE(ioHandler->lastReadStream);
        EXPECT_EQ(ioHandler->lastReadStream->GetID(), inputStream->GetID());

        // check that buffer was filled with test pattern
        EXPECT_EQ(buffer[0], 0x42);
        EXPECT_EQ(buffer[bytesCount - 1], 0x42);
    }

    ASSERT_EQ(driverAdapter->StopIO(device->GetID(), clientID), kAudioHardwareNoError);
}

TEST_F(DriverAdapterTest, IO_WriteClientOutput)
{
    auto driverAdapter =
        std::make_shared<aspl::DriverAdapter>(driver->GetReference(), tracer);

    const UInt32 clientID = 123;
    const UInt32 bytesCount = 1024;
    std::vector<UInt8> buffer(bytesCount, 0x55); // Fill with test pattern

    aspl::DriverAdapter::IOCycle ioCycle;
    ioCycle.CycleCounter = 1;
    ioCycle.BatchFrameCount = 256;
    ioCycle.BatchSampleTime = 0.0;
    ioCycle.BatchHostTime = 1000;
    ioCycle.CurrentSampleTime = 0.0;
    ioCycle.CurrentHostTime = 1000;
    ioCycle.HostTicksPerFrame = 100.0;

    ASSERT_EQ(driverAdapter->Initialize(virtHost), kAudioHardwareNoError);
    ASSERT_EQ(driverAdapter->StartIO(device->GetID(), clientID), kAudioHardwareNoError);

    // check initial state
    EXPECT_EQ(ioHandler->writeClientOutputCallCount, 0);

    // perform multiple write operations
    for (int i = 0; i < 3; i++) {
        ioCycle.CycleCounter = i + 1;
        ioCycle.BatchSampleTime = i * 256.0;
        ioCycle.CurrentSampleTime = i * 256.0;

        // check WillWriteClientOutput before each cycle
        {
            auto [status, willDo, willDoInPlace] =
                driverAdapter->WillWriteClientOutput(device->GetID(), clientID);
            EXPECT_EQ(status, kAudioHardwareNoError);
            EXPECT_TRUE(willDo);
            EXPECT_TRUE(willDoInPlace);
        }

        OSStatus status = driverAdapter->WriteClientOutput(device->GetID(),
            outputStream->GetID(),
            clientID,
            ioCycle,
            buffer.data(),
            bytesCount);

        EXPECT_EQ(status, kAudioHardwareNoError);
        EXPECT_EQ(ioHandler->writeClientOutputCallCount, i + 1);

        // check that handler was called with correct parameters
        EXPECT_EQ(ioHandler->lastWriteFrameCount, 256);
        EXPECT_EQ(ioHandler->lastWriteTimestamp, i * 256.0);
        EXPECT_TRUE(ioHandler->lastWriteStream);
        EXPECT_EQ(ioHandler->lastWriteStream->GetID(), outputStream->GetID());
    }

    ASSERT_EQ(driverAdapter->StopIO(device->GetID(), clientID), kAudioHardwareNoError);
}
