#include <aspl/Compat.hpp>
#include <aspl/Driver.hpp>

#include "Compare.hpp"

#include "TestTracer.hpp"

#include <CoreAudio/AudioServerPlugIn.h>
#include <CoreFoundation/CoreFoundation.h>

#include <vector>

#include <gtest/gtest.h>

namespace {

static const std::string TestName = "TestName";
static const std::string TestManufacturer = "TestManufacturer";
static const std::string TestDeviceUID = "TestDeviceUID";
static const std::string TestModelUID = "TestModelUID";
static const std::string TestSerialNumber = "TestSerialNumber";
static const std::string TestFirmwareVersion = "TestFirmwareVersion";
static const std::string TestIconURL = "TestIconURL";
static const std::string TestConfApp = "TestConfApp";
static const std::string TestResourceBundle = "TestResourceBundle";

static constexpr UInt32 TestSampleRate = 48000, TestNumChannels = 4,
                        TestStartingChannel = 33, TestLatency = 1000,
                        TestSafetyOffset = 2000, TestZeroTimeStampPeriod = 3000,
                        TestClockDomain = 123, TestMinRawVolume = 5,
                        TestMaxRawVolume = 94;

static constexpr bool TestClockIsStable = false;

static constexpr auto TestClockAlgorithm =
    kAudioDeviceClockAlgorithm12PtMovingWindowAverage;

static constexpr Float32 TestMinDecibelVolume = -94, TestMaxDecibelVolume = -5;

class TestDevice : public aspl::Device
{
public:
    explicit TestDevice(const std::shared_ptr<const aspl::Context>& context,
        const aspl::DeviceParameters& params = {})
        : Device(context, params)
    {
        fooValue_ =
            CFStringCreateWithCString(kCFAllocatorDefault, "Foo", kCFStringEncodingUTF8);

        barValue_ =
            CFStringCreateWithCString(kCFAllocatorDefault, "Bar", kCFStringEncodingUTF8);

        bazValue_ =
            CFStringCreateWithCString(kCFAllocatorDefault, "Baz", kCFStringEncodingUTF8);

        // use overload for object + method pointers
        RegisterCustomProperty(
            FooSelector, *this, &TestDevice::GetFoo, &TestDevice::SetFoo);

        // use overload for getter + setter function
        RegisterCustomProperty(
            BarSelector,
            [=]() {
                return CFStringCreateCopy(kCFAllocatorDefault, barValue_);
            },
            [=](CFStringRef value) {
                CFRelease(barValue_);
                CFRetain(value);
                barValue_ = value;
            });

        // use overload for only getter function
        RegisterCustomProperty(BazSelector, [=]() {
            return CFStringCreateCopy(kCFAllocatorDefault, bazValue_);
        });
    }

    ~TestDevice() override
    {
        CFRelease(fooValue_);
        CFRelease(barValue_);
        CFRelease(bazValue_);
    }

    // Custom properties
    static constexpr AudioObjectPropertySelector FooSelector = 'Tfoo';
    static constexpr AudioObjectPropertySelector BarSelector = 'Tbar';
    static constexpr AudioObjectPropertySelector BazSelector = 'Tbaz';

    CFStringRef GetFoo() const
    {
        return CFStringCreateCopy(kCFAllocatorDefault, fooValue_);
    }

    void SetFoo(CFStringRef value)
    {
        CFRelease(fooValue_);
        CFRetain(value);
        fooValue_ = value;
    }

private:
    CFStringRef fooValue_;
    CFStringRef barValue_;
    CFStringRef bazValue_;
};

AudioValueRange MakeAudioValueRange(Float64 min, Float64 max)
{
    AudioValueRange ret = {};
    ret.mMinimum = min;
    ret.mMaximum = max;

    return ret;
}

std::vector<AudioChannelDescription> MakeAudioChannelDescriptions(UInt32 numChans)
{
    std::vector<AudioChannelDescription> ret;

    for (UInt32 n = 0; n < numChans; n++) {
        AudioChannelDescription desc = {};
        desc.mChannelLabel = kAudioChannelLabel_Left + n;
        ret.push_back(desc);
    }

    return ret;
}

AudioStreamRangedDescription MakeRangedFormat(
    const AudioStreamBasicDescription& basicFormat,
    const AudioValueRange& range)
{
    AudioStreamRangedDescription rangedFormat;
    rangedFormat.mFormat = basicFormat;
    rangedFormat.mSampleRateRange = range;

    return rangedFormat;
}

bool HasProperty(const std::shared_ptr<aspl::Driver>& driver,
    AudioObjectID objectID,
    AudioObjectPropertySelector selector,
    AudioObjectPropertyScope scope)
{
    const auto iface = driver->GetPluginInterface();

    AudioObjectPropertyAddress address = {};
    address.mSelector = selector;
    address.mScope = scope;

    const auto result = iface.HasProperty(driver->GetReference(), objectID, 0, &address);

    return result;
}

UInt32 GetPropertySize(const std::shared_ptr<aspl::Driver>& driver,
    AudioObjectID objectID,
    AudioObjectPropertySelector selector,
    AudioObjectPropertyScope scope)
{
    const auto iface = driver->GetPluginInterface();

    AudioObjectPropertyAddress address = {};
    address.mSelector = selector;
    address.mScope = scope;

    UInt32 size = 0;

    const auto status = iface.GetPropertyDataSize(
        driver->GetReference(), objectID, 0, &address, 0, nullptr, &size);

    EXPECT_EQ(kAudioHardwareNoError, status);
    if (status != kAudioHardwareNoError) {
        return 0;
    }

    return size;
}

template <class Value>
Value GetPODProperty(const std::shared_ptr<aspl::Driver>& driver,
    AudioObjectID objectID,
    AudioObjectPropertySelector selector,
    AudioObjectPropertyScope scope)
{
    const auto iface = driver->GetPluginInterface();

    AudioObjectPropertyAddress address = {};
    address.mSelector = selector;
    address.mScope = scope;

    Value value;
    UInt32 valueSize = 0;

    const auto status = iface.GetPropertyData(driver->GetReference(),
        objectID,
        0,
        &address,
        0,
        nullptr,
        sizeof(value),
        &valueSize,
        &value);

    EXPECT_EQ(kAudioHardwareNoError, status);
    if (status != kAudioHardwareNoError) {
        return {};
    }

    EXPECT_EQ(sizeof(value), valueSize);
    if (valueSize != sizeof(value)) {
        return {};
    }

    return value;
}

template <class Value>
Value GetPODProperty(const std::shared_ptr<aspl::Driver>& driver,
    AudioObjectID objectID,
    AudioObjectPropertySelector selector,
    AudioObjectPropertyScope scope,
    Value converterInput)
{
    const auto iface = driver->GetPluginInterface();

    AudioObjectPropertyAddress address = {};
    address.mSelector = selector;
    address.mScope = scope;

    Value value = converterInput;
    UInt32 valueSize = 0;

    const auto status = iface.GetPropertyData(driver->GetReference(),
        objectID,
        0,
        &address,
        0,
        nullptr,
        sizeof(value),
        &valueSize,
        &value);

    EXPECT_EQ(kAudioHardwareNoError, status);
    if (status != kAudioHardwareNoError) {
        return {};
    }

    EXPECT_EQ(sizeof(value), valueSize);
    if (valueSize != sizeof(value)) {
        return {};
    }

    return value;
}

std::string GetStringProperty(const std::shared_ptr<aspl::Driver>& driver,
    AudioObjectID objectID,
    AudioObjectPropertySelector selector,
    AudioObjectPropertyScope scope)
{
    const auto iface = driver->GetPluginInterface();

    AudioObjectPropertyAddress address = {};
    address.mSelector = selector;
    address.mScope = scope;

    CFStringRef value;
    UInt32 valueSize = 0;

    const auto status = iface.GetPropertyData(driver->GetReference(),
        objectID,
        0,
        &address,
        0,
        nullptr,
        sizeof(value),
        &valueSize,
        &value);

    EXPECT_EQ(kAudioHardwareNoError, status);
    if (status != kAudioHardwareNoError) {
        return {};
    }

    EXPECT_EQ(sizeof(value), valueSize);
    if (valueSize != sizeof(value)) {
        return {};
    }

    CFIndex length = CFStringGetLength(value);
    CFIndex maxSize =
        CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;

    EXPECT_GE(maxSize, 0);

    std::vector<char> buffer(size_t(maxSize) + 1);

    if (maxSize > 0) {
        const auto success =
            CFStringGetCString(value, &buffer[0], maxSize, kCFStringEncodingUTF8);
        EXPECT_TRUE(success);
    }

    CFRelease(value);

    return std::string(&buffer[0]);
}

std::string GetURLProperty(const std::shared_ptr<aspl::Driver>& driver,
    AudioObjectID objectID,
    AudioObjectPropertySelector selector,
    AudioObjectPropertyScope scope)
{
    const auto iface = driver->GetPluginInterface();

    AudioObjectPropertyAddress address = {};
    address.mSelector = selector;
    address.mScope = scope;

    CFURLRef value;
    UInt32 valueSize = 0;

    const auto status = iface.GetPropertyData(driver->GetReference(),
        objectID,
        0,
        &address,
        0,
        nullptr,
        sizeof(value),
        &valueSize,
        &value);

    EXPECT_EQ(kAudioHardwareNoError, status);
    if (status != kAudioHardwareNoError) {
        return {};
    }

    EXPECT_EQ(sizeof(value), valueSize);
    if (valueSize != sizeof(value)) {
        return {};
    }

    CFStringRef stringValue = CFURLGetString(value);

    CFIndex length = CFStringGetLength(stringValue);
    CFIndex maxSize =
        CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;

    EXPECT_GE(maxSize, 0);

    std::vector<char> buffer(size_t(maxSize) + 1);

    if (maxSize > 0) {
        const auto success =
            CFStringGetCString(stringValue, &buffer[0], maxSize, kCFStringEncodingUTF8);
        EXPECT_TRUE(success);
    }

    CFRelease(value);

    return std::string(&buffer[0]);
}

template <class Element>
std::vector<Element> GetVectorProperty(const std::shared_ptr<aspl::Driver>& driver,
    AudioObjectID objectID,
    AudioObjectPropertySelector selector,
    AudioObjectPropertyScope scope)
{
    const auto iface = driver->GetPluginInterface();

    AudioObjectPropertyAddress address = {};
    address.mSelector = selector;
    address.mScope = scope;

    UInt32 valuesSize = 0;

    OSStatus status = iface.GetPropertyDataSize(
        driver->GetReference(), objectID, 0, &address, 0, nullptr, &valuesSize);

    EXPECT_EQ(kAudioHardwareNoError, status);
    if (status != kAudioHardwareNoError) {
        return {};
    }

    EXPECT_EQ(0, valuesSize % sizeof(Element));
    if (valuesSize % sizeof(Element)) {
        return {};
    }

    if (valuesSize == 0) {
        return {};
    }

    std::vector<Element> values(valuesSize / sizeof(Element));

    status = iface.GetPropertyData(driver->GetReference(),
        objectID,
        0,
        &address,
        0,
        nullptr,
        valuesSize,
        &valuesSize,
        values.data());

    EXPECT_EQ(kAudioHardwareNoError, status);
    if (status != kAudioHardwareNoError) {
        return {};
    }

    EXPECT_EQ(values.size() * sizeof(Element), valuesSize);
    if (valuesSize != values.size() * sizeof(Element)) {
        return {};
    }

    return values;
}

template <class Value>
void SetPODProperty(Value value,
    OSStatus expectedStatus,
    const std::shared_ptr<aspl::Driver>& driver,
    AudioObjectID objectID,
    AudioObjectPropertySelector selector,
    AudioObjectPropertyScope scope)
{
    const auto iface = driver->GetPluginInterface();

    AudioObjectPropertyAddress address = {};
    address.mSelector = selector;
    address.mScope = scope;

    const auto status = iface.SetPropertyData(
        driver->GetReference(), objectID, 0, &address, 0, nullptr, sizeof(value), &value);

    EXPECT_EQ(expectedStatus, status);
}

void SetStringProperty(const std::string& value,
    OSStatus expectedStatus,
    const std::shared_ptr<aspl::Driver>& driver,
    AudioObjectID objectID,
    AudioObjectPropertySelector selector,
    AudioObjectPropertyScope scope)
{
    const auto iface = driver->GetPluginInterface();

    AudioObjectPropertyAddress address = {};
    address.mSelector = selector;
    address.mScope = scope;

    CFStringRef string = CFStringCreateWithCString(
        kCFAllocatorDefault, value.c_str(), kCFStringEncodingUTF8);

    const auto status = iface.SetPropertyData(driver->GetReference(),
        objectID,
        0,
        &address,
        0,
        nullptr,
        sizeof(string),
        &string);

    CFRelease(string);

    EXPECT_EQ(expectedStatus, status);
}

template <class Vector>
void ExpectVectorsEq(const Vector& a, const Vector& b)
{
    ASSERT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); i++) {
        ASSERT_TRUE(a[i] == b[i]);
    }
}

} // anonymous namespace

struct PropertiesTest : ::testing::Test
{
    aspl::PluginParameters pluginParams;
    aspl::DeviceParameters devParams;
    aspl::StreamParameters streamParams;
    aspl::VolumeControlParameters volumeControlParams;
    aspl::MuteControlParameters muteControlParams;

    std::shared_ptr<aspl::Tracer> tracer;

    std::shared_ptr<aspl::Context> context;

    std::shared_ptr<aspl::Driver> driver;
    std::shared_ptr<aspl::Plugin> plugin;

    std::shared_ptr<aspl::Device> device;
    std::shared_ptr<aspl::Stream> stream;
    std::shared_ptr<aspl::VolumeControl> volumeControl;
    std::shared_ptr<aspl::MuteControl> muteControl;

    void SetUp() override
    {
        pluginParams.Manufacturer = TestManufacturer;
        pluginParams.ResourceBundlePath = TestResourceBundle;

        devParams.Name = TestName;
        devParams.Manufacturer = TestManufacturer;
        devParams.DeviceUID = TestDeviceUID;
        devParams.ModelUID = TestModelUID;
        devParams.SerialNumber = TestSerialNumber;
        devParams.FirmwareVersion = TestFirmwareVersion;
        devParams.IconURL = TestIconURL;
        devParams.ConfigurationApplicationBundleID = TestConfApp;
        devParams.SampleRate = TestSampleRate;
        devParams.ChannelCount = TestNumChannels;
        devParams.Latency = TestLatency;
        devParams.SafetyOffset = TestSafetyOffset;
        devParams.ZeroTimeStampPeriod = TestZeroTimeStampPeriod;
        devParams.ClockIsStable = TestClockIsStable;
        devParams.ClockAlgorithm = TestClockAlgorithm;
        devParams.ClockDomain = TestClockDomain;

        streamParams.Direction = aspl::Direction::Input;
        streamParams.StartingChannel = TestStartingChannel;
        streamParams.Format.mSampleRate = TestSampleRate;
        streamParams.Format.mChannelsPerFrame = TestNumChannels;
        streamParams.Latency = TestLatency;

        volumeControlParams.Scope = kAudioObjectPropertyScopeInput;
        volumeControlParams.MinRawVolume = TestMinRawVolume;
        volumeControlParams.MaxRawVolume = TestMaxRawVolume;
        volumeControlParams.MinDecibelVolume = TestMinDecibelVolume;
        volumeControlParams.MaxDecibelVolume = TestMaxDecibelVolume;

        muteControlParams.Scope = kAudioObjectPropertyScopeInput;

        tracer = std::make_shared<TestTracer>();
        context = std::make_shared<aspl::Context>(tracer);

        device = std::make_shared<TestDevice>(context, devParams);
        stream = device->AddStreamAsync(streamParams);
        volumeControl = device->AddVolumeControlAsync(volumeControlParams);
        muteControl = device->AddMuteControlAsync(muteControlParams);

        plugin = std::make_shared<aspl::Plugin>(context, pluginParams);
        plugin->AddDevice(device);

        driver = std::make_shared<aspl::Driver>(context, plugin);
    }

    void ExpectUInt32Property(UInt32 value,
        AudioObjectID objectID,
        AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal)
    {
        ASSERT_TRUE(HasProperty(driver, objectID, selector, scope));
        ASSERT_EQ(sizeof(value), GetPropertySize(driver, objectID, selector, scope));
        ASSERT_EQ(value, GetPODProperty<UInt32>(driver, objectID, selector, scope));
    }

    void ExpectSInt32Property(SInt32 value,
        AudioObjectID objectID,
        AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal)
    {
        ASSERT_TRUE(HasProperty(driver, objectID, selector, scope));
        ASSERT_EQ(sizeof(value), GetPropertySize(driver, objectID, selector, scope));
        ASSERT_EQ(value, GetPODProperty<SInt32>(driver, objectID, selector, scope));
    }

    void ExpectFloat64Property(Float64 value,
        AudioObjectID objectID,
        AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal)
    {
        ASSERT_TRUE(HasProperty(driver, objectID, selector, scope));
        ASSERT_EQ(sizeof(value), GetPropertySize(driver, objectID, selector, scope));
        ASSERT_EQ(value, GetPODProperty<Float64>(driver, objectID, selector, scope));
    }

    template <class Value>
    void ExpectPODProperty(Value value,
        AudioObjectID objectID,
        AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal)
    {
        ASSERT_TRUE(HasProperty(driver, objectID, selector, scope));
        ASSERT_EQ(sizeof(value), GetPropertySize(driver, objectID, selector, scope));
        ASSERT_TRUE(value == GetPODProperty<Value>(driver, objectID, selector, scope));
    }

    void ExpectStringProperty(const std::string& value,
        AudioObjectID objectID,
        AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal)
    {
        ASSERT_TRUE(HasProperty(driver, objectID, selector, scope));
        ASSERT_EQ(
            sizeof(CFStringRef), GetPropertySize(driver, objectID, selector, scope));
        ASSERT_EQ(value, GetStringProperty(driver, objectID, selector, scope));
    }

    void ExpectURLProperty(const std::string& value,
        AudioObjectID objectID,
        AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal)
    {
        ASSERT_TRUE(HasProperty(driver, objectID, selector, scope));
        ASSERT_EQ(sizeof(CFURLRef), GetPropertySize(driver, objectID, selector, scope));
        ASSERT_EQ(value, GetURLProperty(driver, objectID, selector, scope));
    }

    template <class Element>
    void ExpectVectorProperty(const std::vector<Element>& values,
        AudioObjectID objectID,
        AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal)
    {
        ASSERT_TRUE(HasProperty(driver, objectID, selector, scope));
        ExpectVectorsEq(
            values, GetVectorProperty<Element>(driver, objectID, selector, scope));
    }

    void ExpectFloat32Convertion(Float32 converterOutput,
        Float32 converterInput,
        AudioObjectID objectID,
        AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal)
    {
        ASSERT_TRUE(HasProperty(driver, objectID, selector, scope));
        ASSERT_EQ(
            sizeof(converterOutput), GetPropertySize(driver, objectID, selector, scope));
        ASSERT_EQ(converterOutput,
            GetPODProperty<Float32>(driver, objectID, selector, scope, converterInput));
    }

    void WriteUInt32Property(UInt32 value,
        OSStatus expectedStatus,
        AudioObjectID objectID,
        AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal)
    {
        SetPODProperty(value, expectedStatus, driver, objectID, selector, scope);
    }

    void WriteFloat32Property(Float32 value,
        OSStatus expectedStatus,
        AudioObjectID objectID,
        AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal)
    {
        SetPODProperty(value, expectedStatus, driver, objectID, selector, scope);
    }

    void WriteFloat64Property(Float64 value,
        OSStatus expectedStatus,
        AudioObjectID objectID,
        AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal)
    {
        SetPODProperty(value, expectedStatus, driver, objectID, selector, scope);
    }

    template <class Value>
    void WritePODProperty(Value value,
        OSStatus expectedStatus,
        AudioObjectID objectID,
        AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal)
    {
        SetPODProperty(value, expectedStatus, driver, objectID, selector, scope);
    }

    void WriteStringProperty(const std::string value,
        OSStatus expectedStatus,
        AudioObjectID objectID,
        AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal)
    {
        SetStringProperty(value, expectedStatus, driver, objectID, selector, scope);
    }
};

TEST_F(PropertiesTest, Classes)
{
    // Plugin
    ASSERT_EQ(kAudioPlugInClassID, plugin->GetClass());
    ExpectUInt32Property(kAudioPlugInClassID, plugin->GetID(), kAudioObjectPropertyClass);

    ASSERT_EQ(kAudioObjectClassID, plugin->GetBaseClass());
    ExpectUInt32Property(
        kAudioObjectClassID, plugin->GetID(), kAudioObjectPropertyBaseClass);

    // Device
    ASSERT_EQ(kAudioDeviceClassID, device->GetClass());
    ExpectUInt32Property(kAudioDeviceClassID, device->GetID(), kAudioObjectPropertyClass);

    ASSERT_EQ(kAudioObjectClassID, device->GetBaseClass());
    ExpectUInt32Property(
        kAudioObjectClassID, device->GetID(), kAudioObjectPropertyBaseClass);

    // Stream
    ASSERT_EQ(kAudioStreamClassID, stream->GetClass());
    ExpectUInt32Property(kAudioStreamClassID, stream->GetID(), kAudioObjectPropertyClass);

    ASSERT_EQ(kAudioObjectClassID, stream->GetBaseClass());
    ExpectUInt32Property(
        kAudioObjectClassID, stream->GetID(), kAudioObjectPropertyBaseClass);

    // VolumeControl
    ASSERT_EQ(kAudioVolumeControlClassID, volumeControl->GetClass());
    ExpectUInt32Property(
        kAudioVolumeControlClassID, volumeControl->GetID(), kAudioObjectPropertyClass);

    ASSERT_EQ(kAudioLevelControlClassID, volumeControl->GetBaseClass());
    ExpectUInt32Property(
        kAudioLevelControlClassID, volumeControl->GetID(), kAudioObjectPropertyBaseClass);

    // MuteControl
    ASSERT_EQ(kAudioMuteControlClassID, muteControl->GetClass());
    ExpectUInt32Property(
        kAudioMuteControlClassID, muteControl->GetID(), kAudioObjectPropertyClass);

    ASSERT_EQ(kAudioBooleanControlClassID, muteControl->GetBaseClass());
    ExpectUInt32Property(
        kAudioBooleanControlClassID, muteControl->GetID(), kAudioObjectPropertyBaseClass);
}

TEST_F(PropertiesTest, Plugin)
{
    ASSERT_EQ(plugin, driver->GetPlugin());

    // Manufacturer
    EXPECT_EQ(TestManufacturer, plugin->GetManufacturer());
    ExpectStringProperty(
        TestManufacturer, plugin->GetID(), kAudioObjectPropertyManufacturer);

    // ResourceBundlePath
    EXPECT_EQ(TestResourceBundle, plugin->GetResourceBundlePath());
    ExpectStringProperty(
        TestResourceBundle, plugin->GetID(), kAudioPlugInPropertyResourceBundle);
}

TEST_F(PropertiesTest, Device)
{
    ASSERT_EQ(device, driver->GetPlugin()->GetDeviceByIndex(0));

    // Name
    EXPECT_EQ(TestName, device->GetName());
    ExpectStringProperty(TestName, device->GetID(), kAudioObjectPropertyName);

    // Manufacturer
    EXPECT_EQ(TestManufacturer, device->GetManufacturer());
    ExpectStringProperty(
        TestManufacturer, device->GetID(), kAudioObjectPropertyManufacturer);

    // DeviceUID
    EXPECT_EQ(TestDeviceUID, device->GetDeviceUID());
    ExpectStringProperty(TestDeviceUID, device->GetID(), kAudioDevicePropertyDeviceUID);

    // ModelUID
    EXPECT_EQ(TestModelUID, device->GetModelUID());
    ExpectStringProperty(TestModelUID, device->GetID(), kAudioDevicePropertyModelUID);

    // SerialNumber
    EXPECT_EQ(TestSerialNumber, device->GetSerialNumber());
    ExpectStringProperty(
        TestSerialNumber, device->GetID(), kAudioObjectPropertySerialNumber);

    // FirmwareVersion
    EXPECT_EQ(TestFirmwareVersion, device->GetFirmwareVersion());
    ExpectStringProperty(
        TestFirmwareVersion, device->GetID(), kAudioObjectPropertyFirmwareVersion);

    // IconURL
    EXPECT_EQ(TestIconURL, device->GetIconURL());
    ExpectURLProperty(TestIconURL, device->GetID(), kAudioDevicePropertyIcon);

    // ConfigurationApplicationBundleID
    EXPECT_EQ(TestConfApp, device->GetConfigurationApplicationBundleID());
    ExpectStringProperty(
        TestConfApp, device->GetID(), kAudioDevicePropertyConfigurationApplication);

    // TransportType
    EXPECT_EQ(kAudioDeviceTransportTypeVirtual, device->GetTransportType());
    ExpectUInt32Property(kAudioDeviceTransportTypeVirtual,
        device->GetID(),
        kAudioDevicePropertyTransportType);

    // RelatedDeviceIDs
    ExpectVectorsEq({device->GetID()}, device->GetRelatedDeviceIDs());
    ExpectVectorProperty<AudioObjectID>(
        {device->GetID()}, device->GetID(), kAudioDevicePropertyRelatedDevices);

    // ClockIsStable
    EXPECT_EQ(TestClockIsStable, device->GetClockIsStable());
    ExpectUInt32Property(
        TestClockIsStable, device->GetID(), kAudioDevicePropertyClockIsStable);

    // ClockAlgorithm
    EXPECT_EQ(TestClockAlgorithm, device->GetClockAlgorithm());
    ExpectUInt32Property(
        TestClockAlgorithm, device->GetID(), kAudioDevicePropertyClockAlgorithm);

    // ClockDomain
    EXPECT_EQ(TestClockDomain, device->GetClockDomain());
    ExpectUInt32Property(
        TestClockDomain, device->GetID(), kAudioDevicePropertyClockDomain);

    // NominalSampleRate
    EXPECT_EQ(TestSampleRate, device->GetNominalSampleRate());
    ExpectFloat64Property(
        TestSampleRate, device->GetID(), kAudioDevicePropertyNominalSampleRate);

    EXPECT_EQ(kAudioHardwareUnsupportedOperationError,
        device->SetNominalSampleRateAsync(TestSampleRate / 2));
    WriteFloat64Property(TestSampleRate / 2,
        kAudioHardwareUnsupportedOperationError,
        device->GetID(),
        kAudioDevicePropertyNominalSampleRate);

    // AvailableSampleRates
    ExpectVectorsEq({MakeAudioValueRange(TestSampleRate, TestSampleRate)},
        device->GetAvailableSampleRates());
    ExpectVectorProperty<AudioValueRange>(
        {MakeAudioValueRange(TestSampleRate, TestSampleRate)},
        device->GetID(),
        kAudioDevicePropertyAvailableNominalSampleRates);

    // PreferredChannelsForStereo
    ExpectVectorsEq({UInt32(1), UInt32(2)}, device->GetPreferredChannelsForStereo());
    ExpectVectorProperty<UInt32>({UInt32(1), UInt32(2)},
        device->GetID(),
        kAudioDevicePropertyPreferredChannelsForStereo,
        kAudioObjectPropertyScopeInput);

    // PreferredChannels
    ExpectVectorsEq(
        MakeAudioChannelDescriptions(TestNumChannels), device->GetPreferredChannels());

    // IsIdentifying
    EXPECT_EQ(false, device->GetIsIdentifying());
    ExpectUInt32Property(0, device->GetID(), kAudioObjectPropertyIdentify);

    EXPECT_EQ(kAudioHardwareUnsupportedOperationError, device->SetIsIdentifying(true));
    WriteUInt32Property(true,
        kAudioHardwareUnsupportedOperationError,
        device->GetID(),
        kAudioObjectPropertyIdentify);

    // IsAlive
    EXPECT_TRUE(device->GetIsAlive());
    ExpectUInt32Property(1, device->GetID(), kAudioDevicePropertyDeviceIsAlive);

    device->SetIsAlive(false);
    EXPECT_FALSE(device->GetIsAlive());
    ExpectUInt32Property(0, device->GetID(), kAudioDevicePropertyDeviceIsAlive);

    // IsHidden
    EXPECT_FALSE(device->GetIsHidden());
    ExpectUInt32Property(0, device->GetID(), kAudioDevicePropertyIsHidden);

    device->SetIsHidden(true);
    EXPECT_TRUE(device->GetIsHidden());
    ExpectUInt32Property(1, device->GetID(), kAudioDevicePropertyIsHidden);

    // IsRunning
    EXPECT_FALSE(device->GetIsRunning());
    ExpectUInt32Property(0, device->GetID(), kAudioDevicePropertyDeviceIsRunning);

    // CanBeDefaultDevice
    EXPECT_TRUE(device->GetCanBeDefaultDevice());
    ExpectUInt32Property(1,
        device->GetID(),
        kAudioDevicePropertyDeviceCanBeDefaultDevice,
        kAudioObjectPropertyScopeInput);

    device->SetCanBeDefaultDevice(false);
    EXPECT_FALSE(device->GetCanBeDefaultDevice());
    ExpectUInt32Property(0,
        device->GetID(),
        kAudioDevicePropertyDeviceCanBeDefaultDevice,
        kAudioObjectPropertyScopeInput);

    // GetCanBeDefaultSystemDevice
    EXPECT_TRUE(device->GetCanBeDefaultSystemDevice());
    ExpectUInt32Property(1,
        device->GetID(),
        kAudioDevicePropertyDeviceCanBeDefaultSystemDevice,
        kAudioObjectPropertyScopeInput);

    device->SetCanBeDefaultSystemDevice(false);
    EXPECT_FALSE(device->GetCanBeDefaultSystemDevice());
    ExpectUInt32Property(0,
        device->GetID(),
        kAudioDevicePropertyDeviceCanBeDefaultSystemDevice,
        kAudioObjectPropertyScopeInput);

    // Latency
    EXPECT_EQ(TestLatency, device->GetLatency());
    ExpectUInt32Property(TestLatency,
        device->GetID(),
        kAudioDevicePropertyLatency,
        kAudioObjectPropertyScopeInput);

    // SafetyOffset
    EXPECT_EQ(TestSafetyOffset, device->GetSafetyOffset());
    ExpectUInt32Property(TestSafetyOffset,
        device->GetID(),
        kAudioDevicePropertySafetyOffset,
        kAudioObjectPropertyScopeInput);

    // ZeroTimeStampPeriod
    EXPECT_EQ(TestZeroTimeStampPeriod, device->GetZeroTimeStampPeriod());
    ExpectUInt32Property(TestZeroTimeStampPeriod,
        device->GetID(),
        kAudioDevicePropertyZeroTimeStampPeriod,
        kAudioObjectPropertyScopeInput);
}

TEST_F(PropertiesTest, Stream)
{
    ASSERT_EQ(stream,
        driver->GetPlugin()->GetDeviceByIndex(0)->GetStreamByIndex(
            aspl::Direction::Input, 0));

    // IsActive
    EXPECT_TRUE(stream->GetIsActive());
    ExpectUInt32Property(1, stream->GetID(), kAudioStreamPropertyIsActive);

    stream->SetIsActive(false);
    EXPECT_FALSE(stream->GetIsActive());
    ExpectUInt32Property(0, stream->GetID(), kAudioStreamPropertyIsActive);

    WriteUInt32Property(
        1, kAudioHardwareNoError, stream->GetID(), kAudioStreamPropertyIsActive);
    EXPECT_TRUE(stream->GetIsActive());
    ExpectUInt32Property(1, stream->GetID(), kAudioStreamPropertyIsActive);

    // Direction
    EXPECT_EQ(aspl::Direction::Input, stream->GetDirection());
    ExpectUInt32Property(
        UInt32(aspl::Direction::Input), stream->GetID(), kAudioStreamPropertyDirection);

    // TerminalType
    EXPECT_EQ(kAudioStreamTerminalTypeMicrophone, stream->GetTerminalType());
    ExpectUInt32Property(kAudioStreamTerminalTypeMicrophone,
        stream->GetID(),
        kAudioStreamPropertyTerminalType);

    // StartingChannel
    EXPECT_EQ(TestStartingChannel, stream->GetStartingChannel());
    ExpectUInt32Property(
        TestStartingChannel, stream->GetID(), kAudioStreamPropertyStartingChannel);

    // ChannelCount
    EXPECT_EQ(TestNumChannels, stream->GetChannelCount());

    // Latency
    EXPECT_EQ(TestLatency, stream->GetLatency());
    ExpectUInt32Property(TestLatency, stream->GetID(), kAudioStreamPropertyLatency);

    // PhysicalFormat
    EXPECT_TRUE(streamParams.Format == stream->GetPhysicalFormat());
    ExpectPODProperty(
        streamParams.Format, stream->GetID(), kAudioStreamPropertyPhysicalFormat);

    EXPECT_EQ(kAudioDeviceUnsupportedFormatError,
        stream->SetPhysicalFormatAsync(AudioStreamBasicDescription{}));
    WritePODProperty(AudioStreamBasicDescription{},
        kAudioDeviceUnsupportedFormatError,
        stream->GetID(),
        kAudioStreamPropertyPhysicalFormat);

    // AvailablePhysicalFormats
    ExpectVectorsEq({MakeRangedFormat(streamParams.Format,
                        MakeAudioValueRange(streamParams.Format.mSampleRate,
                            streamParams.Format.mSampleRate))},
        stream->GetAvailablePhysicalFormats());
    ExpectVectorProperty<AudioStreamRangedDescription>(
        {MakeRangedFormat(streamParams.Format,
            MakeAudioValueRange(
                streamParams.Format.mSampleRate, streamParams.Format.mSampleRate))},
        stream->GetID(),
        kAudioStreamPropertyAvailablePhysicalFormats);

    // VirtualFormat
    EXPECT_TRUE(streamParams.Format == stream->GetVirtualFormat());
    ExpectPODProperty(
        streamParams.Format, stream->GetID(), kAudioStreamPropertyVirtualFormat);

    EXPECT_EQ(kAudioDeviceUnsupportedFormatError,
        stream->SetVirtualFormatAsync(AudioStreamBasicDescription{}));
    WritePODProperty(AudioStreamBasicDescription{},
        kAudioDeviceUnsupportedFormatError,
        stream->GetID(),
        kAudioStreamPropertyVirtualFormat);

    // AvailableVirtualFormats
    ExpectVectorsEq({MakeRangedFormat(streamParams.Format,
                        MakeAudioValueRange(streamParams.Format.mSampleRate,
                            streamParams.Format.mSampleRate))},
        stream->GetAvailableVirtualFormats());
    ExpectVectorProperty<AudioStreamRangedDescription>(
        {MakeRangedFormat(streamParams.Format,
            MakeAudioValueRange(
                streamParams.Format.mSampleRate, streamParams.Format.mSampleRate))},
        stream->GetID(),
        kAudioStreamPropertyAvailableVirtualFormats);
}

TEST_F(PropertiesTest, VolumeControl)
{
    ASSERT_EQ(volumeControl,
        driver->GetPlugin()->GetDeviceByIndex(0)->GetVolumeControlByIndex(
            kAudioObjectPropertyScopeInput, 0));

    // Scope
    EXPECT_EQ(kAudioObjectPropertyScopeInput, volumeControl->GetScope());
    ExpectUInt32Property(kAudioObjectPropertyScopeInput,
        volumeControl->GetID(),
        kAudioControlPropertyScope);

    // Element
    EXPECT_EQ(kAudioObjectPropertyElementMain, volumeControl->GetElement());
    ExpectUInt32Property(kAudioObjectPropertyElementMain,
        volumeControl->GetID(),
        kAudioControlPropertyElement);

    // RawRange
    EXPECT_TRUE(MakeAudioValueRange(TestMinRawVolume, TestMaxRawVolume) ==
                volumeControl->GetRawRange());

    // DecibelRange
    EXPECT_TRUE(MakeAudioValueRange(TestMinDecibelVolume, TestMaxDecibelVolume) ==
                volumeControl->GetDecibelRange());
    ExpectPODProperty(MakeAudioValueRange(TestMinDecibelVolume, TestMaxDecibelVolume),
        volumeControl->GetID(),
        kAudioLevelControlPropertyDecibelRange);

    // Default value
    EXPECT_EQ(TestMaxRawVolume, volumeControl->GetRawValue());
    EXPECT_EQ(TestMaxDecibelVolume, volumeControl->GetDecibelValue());
    EXPECT_EQ(1.0f, volumeControl->GetScalarValue());

    // SetRawValue
    EXPECT_EQ(kAudioHardwareNoError, volumeControl->SetRawValue(TestMinRawVolume));

    EXPECT_EQ(TestMinRawVolume, volumeControl->GetRawValue());
    EXPECT_EQ(TestMinDecibelVolume, volumeControl->GetDecibelValue());
    EXPECT_EQ(0.0f, volumeControl->GetScalarValue());

    // SetDecibelValue
    EXPECT_EQ(
        kAudioHardwareNoError, volumeControl->SetDecibelValue(TestMaxDecibelVolume));

    EXPECT_EQ(TestMaxRawVolume, volumeControl->GetRawValue());
    EXPECT_EQ(TestMaxDecibelVolume, volumeControl->GetDecibelValue());
    EXPECT_EQ(1.0f, volumeControl->GetScalarValue());

    WriteFloat32Property(TestMinDecibelVolume,
        kAudioHardwareNoError,
        volumeControl->GetID(),
        kAudioLevelControlPropertyDecibelValue);

    EXPECT_EQ(TestMinRawVolume, volumeControl->GetRawValue());
    EXPECT_EQ(TestMinDecibelVolume, volumeControl->GetDecibelValue());
    EXPECT_EQ(0.0f, volumeControl->GetScalarValue());

    // SetScalarValue
    EXPECT_EQ(kAudioHardwareNoError, volumeControl->SetScalarValue(1.0f));

    EXPECT_EQ(TestMaxRawVolume, volumeControl->GetRawValue());
    EXPECT_EQ(TestMaxDecibelVolume, volumeControl->GetDecibelValue());
    EXPECT_EQ(1.0f, volumeControl->GetScalarValue());

    WriteFloat32Property(0.0f,
        kAudioHardwareNoError,
        volumeControl->GetID(),
        kAudioLevelControlPropertyScalarValue);

    EXPECT_EQ(TestMinRawVolume, volumeControl->GetRawValue());
    EXPECT_EQ(TestMinDecibelVolume, volumeControl->GetDecibelValue());
    EXPECT_EQ(0.0f, volumeControl->GetScalarValue());

    // ScalarToDecibels
    EXPECT_EQ(TestMinDecibelVolume, volumeControl->ConvertScalarToDecibels(0.0f));
    ExpectFloat32Convertion(TestMinDecibelVolume,
        0.0f,
        volumeControl->GetID(),
        kAudioLevelControlPropertyConvertScalarToDecibels);

    EXPECT_EQ(TestMaxDecibelVolume, volumeControl->ConvertScalarToDecibels(1.0f));
    ExpectFloat32Convertion(TestMaxDecibelVolume,
        1.0f,
        volumeControl->GetID(),
        kAudioLevelControlPropertyConvertScalarToDecibels);

    // DecibelsToScalar
    EXPECT_EQ(1.0f, volumeControl->ConvertDecibelsToScalar(TestMaxDecibelVolume));
    ExpectFloat32Convertion(1.0f,
        TestMaxDecibelVolume,
        volumeControl->GetID(),
        kAudioLevelControlPropertyConvertDecibelsToScalar);

    EXPECT_EQ(0.0f, volumeControl->ConvertDecibelsToScalar(TestMinDecibelVolume));
    ExpectFloat32Convertion(0.0f,
        TestMinDecibelVolume,
        volumeControl->GetID(),
        kAudioLevelControlPropertyConvertDecibelsToScalar);
}

TEST_F(PropertiesTest, MuteControl)
{
    ASSERT_EQ(muteControl,
        driver->GetPlugin()->GetDeviceByIndex(0)->GetMuteControlByIndex(
            kAudioObjectPropertyScopeInput, 0));

    // Scope
    EXPECT_EQ(kAudioObjectPropertyScopeInput, muteControl->GetScope());
    ExpectUInt32Property(
        kAudioObjectPropertyScopeInput, muteControl->GetID(), kAudioControlPropertyScope);

    // Element
    EXPECT_EQ(kAudioObjectPropertyElementMain, muteControl->GetElement());
    ExpectUInt32Property(kAudioObjectPropertyElementMain,
        muteControl->GetID(),
        kAudioControlPropertyElement);

    // IsMuted
    EXPECT_FALSE(muteControl->GetIsMuted());
    ExpectUInt32Property(0, muteControl->GetID(), kAudioBooleanControlPropertyValue);

    muteControl->SetIsMuted(true);
    EXPECT_TRUE(muteControl->GetIsMuted());
    ExpectUInt32Property(1, muteControl->GetID(), kAudioBooleanControlPropertyValue);

    WriteUInt32Property(0,
        kAudioHardwareNoError,
        muteControl->GetID(),
        kAudioBooleanControlPropertyValue);
    EXPECT_FALSE(muteControl->GetIsMuted());
    ExpectUInt32Property(0, muteControl->GetID(), kAudioBooleanControlPropertyValue);
}

TEST_F(PropertiesTest, CustomProperties)
{
    ExpectStringProperty("Foo", device->GetID(), TestDevice::FooSelector);
    ExpectStringProperty("Bar", device->GetID(), TestDevice::BarSelector);
    ExpectStringProperty("Baz", device->GetID(), TestDevice::BazSelector);

    WriteStringProperty(
        "test", kAudioHardwareNoError, device->GetID(), TestDevice::FooSelector);
    ExpectStringProperty("test", device->GetID(), TestDevice::FooSelector);

    WriteStringProperty(
        "test", kAudioHardwareNoError, device->GetID(), TestDevice::BarSelector);
    ExpectStringProperty("test", device->GetID(), TestDevice::BarSelector);

    // baz is not settable
    WriteStringProperty("test",
        kAudioHardwareUnknownPropertyError,
        device->GetID(),
        TestDevice::BazSelector);
}
