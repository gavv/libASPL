#include <aspl/Context.hpp>
#include <aspl/Storage.hpp>

#include "Convert.hpp"

#include "TestTracer.hpp"

#include <gtest/gtest.h>

#include <string>
#include <unordered_map>

namespace {

std::unordered_map<std::string, CFPropertyListRef> MockStorage;

OSStatus MockStorageRead(AudioServerPlugInHostRef host,
    CFStringRef key,
    CFPropertyListRef* value)
{
    std::string keyString;
    aspl::Convert::FromFoundation(key, keyString);

    if (!MockStorage.count(keyString)) {
        return kAudioHardwareUnknownPropertyError;
    }

    *value = CFPropertyListCreateDeepCopy(
        kCFAllocatorDefault, MockStorage[keyString], kCFPropertyListImmutable);

    return kAudioHardwareNoError;
}

OSStatus MockStorageWrite(AudioServerPlugInHostRef host,
    CFStringRef key,
    CFPropertyListRef value)
{
    std::string keyString;
    aspl::Convert::FromFoundation(key, keyString);

    MockStorage[keyString] = CFPropertyListCreateDeepCopy(
        kCFAllocatorDefault, value, kCFPropertyListImmutable);

    return kAudioHardwareNoError;
}

OSStatus MockStorageDelete(AudioServerPlugInHostRef host, CFStringRef key)
{
    std::string keyString;
    aspl::Convert::FromFoundation(key, keyString);

    if (!MockStorage.count(keyString)) {
        return kAudioHardwareUnknownPropertyError;
    }

    CFRelease(MockStorage[keyString]);

    MockStorage.erase(keyString);

    return kAudioHardwareNoError;
}

void MockStorageClear()
{
    for (auto [_, value] : MockStorage) {
        CFRelease(value);
    }

    MockStorage.clear();
}

} // anonymous namespace

struct StorageTest : ::testing::Test
{
    AudioServerPlugInHostInterface host = {};

    std::shared_ptr<aspl::Tracer> tracer = std::make_shared<TestTracer>();
    std::shared_ptr<aspl::Context> context = std::make_shared<aspl::Context>(tracer);
    std::shared_ptr<aspl::Storage> storage = std::make_shared<aspl::Storage>(context);

    void SetUp() override
    {
        host.CopyFromStorage = MockStorageRead;
        host.WriteToStorage = MockStorageWrite;
        host.DeleteFromStorage = MockStorageDelete;

        context->Host = &host;
    }

    void TearDown() override
    {
        MockStorageClear();
    }
};

TEST_F(StorageTest, ReadWriteDelete)
{
    {
        auto [value, status] = storage->ReadString("key1");
        EXPECT_EQ(kAudioHardwareUnknownPropertyError, status);
        EXPECT_EQ("", value);
    }

    {
        auto [value, status] = storage->ReadString("key2");
        EXPECT_EQ(kAudioHardwareUnknownPropertyError, status);
        EXPECT_EQ("", value);
    }

    ASSERT_EQ(kAudioHardwareNoError, storage->WriteString("key1", "value1"));
    ASSERT_EQ(kAudioHardwareNoError, storage->WriteString("key2", "value2"));

    {
        auto [value, status] = storage->ReadString("key1");
        EXPECT_EQ(kAudioHardwareNoError, status);
        EXPECT_EQ("value1", value);
    }

    {
        auto [value, status] = storage->ReadString("key2");
        EXPECT_EQ(kAudioHardwareNoError, status);
        EXPECT_EQ("value2", value);
    }

    ASSERT_EQ(kAudioHardwareNoError, storage->Delete("key1"));

    {
        auto [value, status] = storage->ReadString("key1");
        EXPECT_EQ(kAudioHardwareUnknownPropertyError, status);
        EXPECT_EQ("", value);
    }

    {
        auto [value, status] = storage->ReadString("key2");
        EXPECT_EQ(kAudioHardwareNoError, status);
        EXPECT_EQ("value2", value);
    }

    ASSERT_EQ(kAudioHardwareNoError, storage->Delete("key2"));

    {
        auto [value, status] = storage->ReadString("key1");
        EXPECT_EQ(kAudioHardwareUnknownPropertyError, status);
        EXPECT_EQ("", value);
    }

    {
        auto [value, status] = storage->ReadString("key2");
        EXPECT_EQ(kAudioHardwareUnknownPropertyError, status);
        EXPECT_EQ("", value);
    }
}

TEST_F(StorageTest, Bytes)
{
    std::vector<UInt8> value{1, 2, 3, 4, 5};
    ASSERT_EQ(kAudioHardwareNoError, storage->WriteBytes("key", value));

    auto [returnedValue, status] = storage->ReadBytes("key");
    EXPECT_EQ(kAudioHardwareNoError, status);
    EXPECT_TRUE(returnedValue == value);
}

TEST_F(StorageTest, String)
{
    ASSERT_EQ(kAudioHardwareNoError, storage->WriteString("key", "value"));

    auto [value, status] = storage->ReadString("key");
    EXPECT_EQ(kAudioHardwareNoError, status);
    EXPECT_EQ("value", value);
}

TEST_F(StorageTest, Boolean)
{
    ASSERT_EQ(kAudioHardwareNoError, storage->WriteBoolean("key", true));

    auto [value, status] = storage->ReadBoolean("key");
    EXPECT_EQ(kAudioHardwareNoError, status);
    EXPECT_EQ(true, value);
}

TEST_F(StorageTest, Int)
{
    ASSERT_EQ(kAudioHardwareNoError, storage->WriteInt("key", 1234567890));

    auto [value, status] = storage->ReadInt("key");
    EXPECT_EQ(kAudioHardwareNoError, status);
    EXPECT_EQ(1234567890, value);
}

TEST_F(StorageTest, Float)
{
    ASSERT_EQ(kAudioHardwareNoError, storage->WriteFloat("key", 12345678.12345678));

    auto [value, status] = storage->ReadFloat("key");
    EXPECT_EQ(kAudioHardwareNoError, status);
    EXPECT_EQ(12345678.12345678, value);
}

TEST_F(StorageTest, IntFloat)
{
    { // int -> float (good)
        ASSERT_EQ(kAudioHardwareNoError, storage->WriteInt("key1", 12345678));

        auto [value, status] = storage->ReadFloat("key1");
        EXPECT_EQ(kAudioHardwareNoError, status);
        EXPECT_EQ(12345678.0, value);
    }

    { // float -> int (good)
        ASSERT_EQ(kAudioHardwareNoError, storage->WriteFloat("key2", 12345678.0));

        auto [value, status] = storage->ReadInt("key2");
        EXPECT_EQ(kAudioHardwareNoError, status);
        EXPECT_EQ(12345678, value);
    }

    { // int -> float (bad)
        ASSERT_EQ(kAudioHardwareNoError, storage->WriteInt("key3", 9223372036854775807));

        {
            auto [value, status] = storage->ReadInt("key3");
            EXPECT_EQ(kAudioHardwareNoError, status);
            EXPECT_EQ(9223372036854775807, value);
        }

        { // value can'be represented as float without loss
            auto [value, status] = storage->ReadFloat("key3");
            EXPECT_EQ(kAudioHardwareIllegalOperationError, status);
            EXPECT_EQ(0, value);
        }
    }

    { // float -> int (bad)
        ASSERT_EQ(kAudioHardwareNoError, storage->WriteFloat("key4", 12345678.123));

        {
            auto [value, status] = storage->ReadFloat("key4");
            EXPECT_EQ(kAudioHardwareNoError, status);
            EXPECT_EQ(12345678.123, value);
        }

        { // value can'be represented as integer without loss
            auto [value, status] = storage->ReadInt("key4");
            EXPECT_EQ(kAudioHardwareIllegalOperationError, status);
            EXPECT_EQ(0, value);
        }
    }
}

TEST_F(StorageTest, Custom)
{
    {
        CFStringRef value = nullptr;
        aspl::Convert::ToFoundation(std::string("test"), value);

        ASSERT_EQ(kAudioHardwareNoError, storage->WriteCustom("key", value));

        CFRelease(value);
    }

    {
        auto [valuePlist, status] = storage->ReadCustom("key");
        EXPECT_EQ(kAudioHardwareNoError, status);

        std::string value;
        aspl::Convert::FromFoundation(valuePlist, value);

        CFRelease(valuePlist);

        EXPECT_EQ("test", value);
    }
}

TEST_F(StorageTest, ErrorNotFound)
{
    {
        auto [value, status] = storage->ReadString("key");
        EXPECT_EQ(kAudioHardwareUnknownPropertyError, status);
        EXPECT_EQ("", value);
    }

    {
        auto status = storage->Delete("key");
        EXPECT_EQ(kAudioHardwareUnknownPropertyError, status);
    }
}

TEST_F(StorageTest, ErrorWrongType)
{
    ASSERT_EQ(kAudioHardwareNoError, storage->WriteString("key", "123"));

    auto [value, status] = storage->ReadInt("key");
    EXPECT_EQ(kAudioHardwareIllegalOperationError, status);
    EXPECT_EQ(0, value);
}

TEST_F(StorageTest, ErrorNoHost)
{
    context->Host = nullptr;

    {
        auto status = storage->WriteString("key", "value");
        EXPECT_EQ(kAudioHardwareNotReadyError, status);
    }

    {
        auto [value, status] = storage->ReadString("key");
        EXPECT_EQ(kAudioHardwareNotReadyError, status);
        EXPECT_EQ("", value);
    }

    {
        auto status = storage->Delete("key");
        EXPECT_EQ(kAudioHardwareNotReadyError, status);
    }
}
