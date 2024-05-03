#include <aspl/Context.hpp>
#include <aspl/Storage.hpp>

#include "Convert.hpp"

#include "TestTracer.hpp"

#include <gtest/gtest.h>

#include <string>
#include <unordered_map>

namespace {

std::unordered_map<std::string, CFPropertyListRef> MockStorage;
std::unordered_map<std::string, OSStatus> MockErrors;

OSStatus MockStorageRead(AudioServerPlugInHostRef host,
    CFStringRef key,
    CFPropertyListRef* value)
{
    std::string keyString;
    aspl::Convert::FromFoundation(key, keyString);

    if (MockErrors.count(keyString)) {
        *value = nullptr;
        return MockErrors[keyString];
    }

    if (!MockStorage.count(keyString)) {
        *value = nullptr;
        return kAudioHardwareNoError;
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

    if (MockErrors.count(keyString)) {
        return MockErrors[keyString];
    }

    MockStorage[keyString] = CFPropertyListCreateDeepCopy(
        kCFAllocatorDefault, value, kCFPropertyListImmutable);

    return kAudioHardwareNoError;
}

OSStatus MockStorageDelete(AudioServerPlugInHostRef host, CFStringRef key)
{
    std::string keyString;
    aspl::Convert::FromFoundation(key, keyString);

    if (MockErrors.count(keyString)) {
        return MockErrors[keyString];
    }

    if (!MockStorage.count(keyString)) {
        return kAudioHardwareUnknownPropertyError;
    }

    CFRelease(MockStorage[keyString]);

    MockStorage.erase(keyString);

    return kAudioHardwareNoError;
}

void MockStorageSetError(const std::string& key, OSStatus error)
{
    MockErrors[key] = error;
}

void MockStorageClear()
{
    for (auto [_, value] : MockStorage) {
        CFRelease(value);
    }

    MockStorage.clear();
    MockErrors.clear();
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
        auto [value, ok] = storage->ReadString("key1");
        EXPECT_FALSE(ok);
        EXPECT_EQ("", value);
    }

    {
        auto [value, ok] = storage->ReadString("key2");
        EXPECT_FALSE(ok);
        EXPECT_EQ("", value);
    }

    ASSERT_TRUE(storage->WriteString("key1", "value1"));
    ASSERT_TRUE(storage->WriteString("key2", "value2"));

    {
        auto [value, ok] = storage->ReadString("key1");
        EXPECT_TRUE(ok);
        EXPECT_EQ("value1", value);
    }

    {
        auto [value, ok] = storage->ReadString("key2");
        EXPECT_TRUE(ok);
        EXPECT_EQ("value2", value);
    }

    ASSERT_TRUE(storage->Delete("key1"));

    {
        auto [value, ok] = storage->ReadString("key1");
        EXPECT_FALSE(ok);
        EXPECT_EQ("", value);
    }

    {
        auto [value, ok] = storage->ReadString("key2");
        EXPECT_TRUE(ok);
        EXPECT_EQ("value2", value);
    }

    ASSERT_TRUE(storage->Delete("key2"));

    {
        auto [value, ok] = storage->ReadString("key1");
        EXPECT_FALSE(ok);
        EXPECT_EQ("", value);
    }

    {
        auto [value, ok] = storage->ReadString("key2");
        EXPECT_FALSE(ok);
        EXPECT_EQ("", value);
    }
}

TEST_F(StorageTest, ReadMultipleTimes)
{
    ASSERT_TRUE(storage->WriteString("key", "value"));

    for (int i = 0; i < 100; i++) {
        auto [value, ok] = storage->ReadString("key");
        EXPECT_TRUE(ok);
        EXPECT_EQ("value", value);
    }
}

TEST_F(StorageTest, WriteMultipleTimes)
{
    for (int i = 0; i < 100; i++) {
        ASSERT_TRUE(storage->WriteString("key", std::to_string(i)));
    }

    auto [value, ok] = storage->ReadString("key");
    EXPECT_TRUE(ok);
    EXPECT_EQ("99", value);
}

TEST_F(StorageTest, Bytes)
{
    {
        std::vector<UInt8> value{1, 2, 3, 4, 5};
        ASSERT_TRUE(storage->WriteBytes("key", value));

        auto [returnedValue, ok] = storage->ReadBytes("key");
        EXPECT_TRUE(ok);
        EXPECT_TRUE(returnedValue == value);
    }

    {
        std::vector<UInt8> value;
        ASSERT_TRUE(storage->WriteBytes("key", value));

        auto [returnedValue, ok] = storage->ReadBytes("key");
        EXPECT_TRUE(ok);
        EXPECT_TRUE(returnedValue.empty());
    }
}

TEST_F(StorageTest, String)
{
    {
        ASSERT_TRUE(storage->WriteString("key", "value"));

        auto [value, ok] = storage->ReadString("key");
        EXPECT_TRUE(ok);
        EXPECT_EQ("value", value);
    }

    {
        ASSERT_TRUE(storage->WriteString("key", ""));

        auto [value, ok] = storage->ReadString("key");
        EXPECT_TRUE(ok);
        EXPECT_EQ("", value);
    }
}

TEST_F(StorageTest, Boolean)
{
    ASSERT_TRUE(storage->WriteBoolean("key", true));

    auto [value, ok] = storage->ReadBoolean("key");
    EXPECT_TRUE(ok);
    EXPECT_EQ(true, value);
}

TEST_F(StorageTest, Int)
{
    ASSERT_TRUE(storage->WriteInt("key", 1234567890));

    auto [value, ok] = storage->ReadInt("key");
    EXPECT_TRUE(ok);
    EXPECT_EQ(1234567890, value);
}

TEST_F(StorageTest, Float)
{
    ASSERT_TRUE(storage->WriteFloat("key", 12345678.12345678));

    auto [value, ok] = storage->ReadFloat("key");
    EXPECT_TRUE(ok);
    EXPECT_EQ(12345678.12345678, value);
}

TEST_F(StorageTest, IntFloat)
{
    { // int -> float (good)
        ASSERT_TRUE(storage->WriteInt("key1", 12345678));

        auto [value, ok] = storage->ReadFloat("key1");
        EXPECT_TRUE(ok);
        EXPECT_EQ(12345678.0, value);
    }

    { // float -> int (good)
        ASSERT_TRUE(storage->WriteFloat("key2", 12345678.0));

        auto [value, ok] = storage->ReadInt("key2");
        EXPECT_TRUE(ok);
        EXPECT_EQ(12345678, value);
    }

    { // int -> float (bad)
        ASSERT_TRUE(storage->WriteInt("key3", 9223372036854705807));

        {
            auto [value, ok] = storage->ReadInt("key3");
            EXPECT_TRUE(ok);
            EXPECT_EQ(9223372036854705807, value);
        }

        { // value can't be represented as float without loss
            auto [value, ok] = storage->ReadFloat("key3");
            EXPECT_FALSE(ok);
            EXPECT_EQ(0, value);
        }
    }

    { // float -> int (bad)
        ASSERT_TRUE(storage->WriteFloat("key4", 12345678.123));

        {
            auto [value, ok] = storage->ReadFloat("key4");
            EXPECT_TRUE(ok);
            EXPECT_EQ(12345678.123, value);
        }

        { // value can't be represented as integer without loss
            auto [value, ok] = storage->ReadInt("key4");
            EXPECT_FALSE(ok);
            EXPECT_EQ(0, value);
        }
    }
}

TEST_F(StorageTest, Custom)
{
    {
        CFStringRef value = nullptr;
        aspl::Convert::ToFoundation(std::string("test"), value);

        ASSERT_TRUE(storage->WriteCustom("key", value));

        CFRelease(value);
    }

    {
        auto [valuePlist, ok] = storage->ReadCustom("key");
        EXPECT_TRUE(ok);

        std::string value;
        aspl::Convert::FromFoundation(valuePlist, value);

        CFRelease(valuePlist);

        EXPECT_EQ("test", value);
    }
}

TEST_F(StorageTest, EmptyKey)
{
    ASSERT_TRUE(storage->WriteString("", "value"));

    auto [value, ok] = storage->ReadString("");
    EXPECT_TRUE(ok);
    EXPECT_EQ("value", value);
}

TEST_F(StorageTest, ErrorNotFound)
{
    {
        auto [value, ok] = storage->ReadString("key");
        EXPECT_FALSE(ok);
        EXPECT_EQ("", value);
    }

    {
        auto ok = storage->Delete("key");
        EXPECT_FALSE(ok);
    }
}

TEST_F(StorageTest, ErrorWrongType)
{
    ASSERT_TRUE(storage->WriteString("key", "123"));

    auto [value, ok] = storage->ReadInt("key");
    EXPECT_FALSE(ok);
    EXPECT_EQ(0, value);
}

TEST_F(StorageTest, ErrorNoHost)
{
    context->Host = nullptr;

    {
        auto ok = storage->WriteString("key", "value");
        EXPECT_FALSE(ok);
    }

    {
        auto [value, ok] = storage->ReadString("key");
        EXPECT_FALSE(ok);
        EXPECT_EQ("", value);
    }

    {
        auto ok = storage->Delete("key");
        EXPECT_FALSE(ok);
    }
}

TEST_F(StorageTest, ErrorBackendRead)
{
    {
        auto ok = storage->WriteString("key", "value");
        EXPECT_TRUE(ok);
    }

    {
        auto [value, ok] = storage->ReadString("key");
        EXPECT_TRUE(ok);
        EXPECT_EQ("value", value);
    }

    MockStorageSetError("key", kAudioHardwareUnspecifiedError);

    {
        auto [value, ok] = storage->ReadString("key");
        EXPECT_FALSE(ok);
        EXPECT_EQ("", value);
    }
}

TEST_F(StorageTest, ErrorBackendWrite)
{
    {
        auto ok = storage->WriteString("key", "value");
        EXPECT_TRUE(ok);
    }

    MockStorageSetError("key", kAudioHardwareUnspecifiedError);

    {
        auto ok = storage->WriteString("key", "value");
        EXPECT_FALSE(ok);
    }
}

TEST_F(StorageTest, ErrorBackendDelete)
{
    {
        auto ok = storage->WriteString("key", "value");
        EXPECT_TRUE(ok);
    }

    MockStorageSetError("key", kAudioHardwareUnspecifiedError);

    {
        auto ok = storage->Delete("key");
        EXPECT_FALSE(ok);
    }
}
