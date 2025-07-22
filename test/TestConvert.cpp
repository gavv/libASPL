// Copyright (c) libASPL authors
// Licensed under MIT

#include "Convert.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

struct ConvertTest : testing::Test
{
};

TEST(ConvertTest, ToFrom_CFString)
{
    { // regular string
        std::string input = "Hello, World!";
        CFStringRef result = nullptr;

        aspl::Convert::ToFoundation(input, result);
        ASSERT_NE(result, nullptr);

        std::string backResult;
        EXPECT_TRUE(aspl::Convert::FromFoundation(result, backResult));
        EXPECT_EQ(backResult, "Hello, World!");

        CFRelease(result);
    }

    { // empty string
        std::string emptyInput = "";
        CFStringRef emptyResult = nullptr;

        aspl::Convert::ToFoundation(emptyInput, emptyResult);
        ASSERT_NE(emptyResult, nullptr);

        std::string emptyBackResult;
        EXPECT_TRUE(aspl::Convert::FromFoundation(emptyResult, emptyBackResult));
        EXPECT_EQ(emptyBackResult, "");

        CFRelease(emptyResult);
    }

    { // null input error
        std::string nullStringResult;
        EXPECT_FALSE(
            aspl::Convert::FromFoundation((CFStringRef) nullptr, nullStringResult));
    }
}

TEST(ConvertTest, ToFrom_CFURL)
{
    { // url string
        std::string input = "https://example.com/path";
        CFURLRef result = nullptr;

        aspl::Convert::ToFoundation(input, result);
        ASSERT_NE(result, nullptr);

        std::string backResult;
        EXPECT_TRUE(aspl::Convert::FromFoundation(result, backResult));
        EXPECT_EQ(backResult, "https://example.com/path");

        CFRelease(result);
    }

    { // null input error
        std::string nullResult;
        EXPECT_FALSE(aspl::Convert::FromFoundation((CFURLRef) nullptr, nullResult));
    }
}

TEST(ConvertTest, ToFrom_CFPropertyList_CFData)
{
    { // byte vector
        std::vector<UInt8> input = {0x01, 0x02, 0x03, 0xFF};
        CFPropertyListRef result = nullptr;

        aspl::Convert::ToFoundation(input, result);
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(CFGetTypeID(result), CFDataGetTypeID());

        std::vector<UInt8> backResult;
        EXPECT_TRUE(aspl::Convert::FromFoundation(result, backResult));
        EXPECT_EQ(backResult, input);

        CFRelease(result);
    }

    { // empty vector
        std::vector<UInt8> emptyInput;
        CFPropertyListRef emptyResult = nullptr;

        aspl::Convert::ToFoundation(emptyInput, emptyResult);
        ASSERT_NE(emptyResult, nullptr);

        std::vector<UInt8> emptyBackResult;
        EXPECT_TRUE(aspl::Convert::FromFoundation(emptyResult, emptyBackResult));
        EXPECT_TRUE(emptyBackResult.empty());

        CFRelease(emptyResult);
    }

    { // wrong type error
        CFStringRef testString =
            CFStringCreateWithCString(kCFAllocatorDefault, "test", kCFStringEncodingUTF8);

        std::vector<UInt8> byteResult;
        EXPECT_FALSE(aspl::Convert::FromFoundation(testString, byteResult));

        CFRelease(testString);
    }
}

TEST(ConvertTest, ToFrom_CFPropertyList_CFString)
{
    { // string to property list
        std::string input = "Test String";
        CFPropertyListRef result = nullptr;

        aspl::Convert::ToFoundation(input, result);
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(CFGetTypeID(result), CFStringGetTypeID());

        std::string backResult;
        EXPECT_TRUE(aspl::Convert::FromFoundation(result, backResult));
        EXPECT_EQ(backResult, "Test String");

        CFRelease(result);
    }

    { // wrong type error
        SInt64 testValue = 42;
        CFNumberRef testNumber =
            CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &testValue);

        std::string stringResult;
        EXPECT_FALSE(aspl::Convert::FromFoundation(testNumber, stringResult));

        CFRelease(testNumber);
    }
}

TEST(ConvertTest, ToFrom_CFPropertyList_CFBoolean)
{
    { // true value
        bool inputTrue = true;
        CFPropertyListRef resultTrue = nullptr;

        aspl::Convert::ToFoundation(inputTrue, resultTrue);
        ASSERT_NE(resultTrue, nullptr);
        EXPECT_EQ(CFGetTypeID(resultTrue), CFBooleanGetTypeID());

        bool backResultTrue;
        EXPECT_TRUE(aspl::Convert::FromFoundation(resultTrue, backResultTrue));
        EXPECT_TRUE(backResultTrue);

        CFRelease(resultTrue);
    }

    { // false value
        bool inputFalse = false;
        CFPropertyListRef resultFalse = nullptr;

        aspl::Convert::ToFoundation(inputFalse, resultFalse);
        ASSERT_NE(resultFalse, nullptr);
        EXPECT_EQ(CFGetTypeID(resultFalse), CFBooleanGetTypeID());

        bool backResultFalse;
        EXPECT_TRUE(aspl::Convert::FromFoundation(resultFalse, backResultFalse));
        EXPECT_FALSE(backResultFalse);

        CFRelease(resultFalse);
    }

    { // wrong type error
        CFStringRef testString =
            CFStringCreateWithCString(kCFAllocatorDefault, "test", kCFStringEncodingUTF8);

        bool boolResult;
        EXPECT_FALSE(aspl::Convert::FromFoundation(testString, boolResult));

        CFRelease(testString);
    }
}

TEST(ConvertTest, ToFrom_CFPropertyList_CFNumber_SInt64)
{
    { // negative value
        SInt64 input = -12345678901234LL;
        CFPropertyListRef result = nullptr;

        aspl::Convert::ToFoundation(input, result);
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(CFGetTypeID(result), CFNumberGetTypeID());

        SInt64 backResult;
        EXPECT_TRUE(aspl::Convert::FromFoundation(result, backResult));
        EXPECT_EQ(backResult, -12345678901234LL);

        CFRelease(result);
    }

    { // zero value
        SInt64 zeroInput = 0;
        CFPropertyListRef zeroResult = nullptr;

        aspl::Convert::ToFoundation(zeroInput, zeroResult);
        ASSERT_NE(zeroResult, nullptr);

        SInt64 zeroBackResult;
        EXPECT_TRUE(aspl::Convert::FromFoundation(zeroResult, zeroBackResult));
        EXPECT_EQ(zeroBackResult, 0);

        CFRelease(zeroResult);
    }

    { // wrong type error
        CFStringRef testString =
            CFStringCreateWithCString(kCFAllocatorDefault, "test", kCFStringEncodingUTF8);

        SInt64 numberResult;
        EXPECT_FALSE(aspl::Convert::FromFoundation(testString, numberResult));

        CFRelease(testString);
    }
}

TEST(ConvertTest, ToFrom_CFPropertyList_CFNumber_Float64)
{
    { // positive value
        Float64 input = 3.14159265359;
        CFPropertyListRef result = nullptr;

        aspl::Convert::ToFoundation(input, result);
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(CFGetTypeID(result), CFNumberGetTypeID());

        Float64 backResult;
        EXPECT_TRUE(aspl::Convert::FromFoundation(result, backResult));
        EXPECT_DOUBLE_EQ(backResult, 3.14159265359);

        CFRelease(result);
    }

    { // negative value
        Float64 negInput = -123.456;
        CFPropertyListRef negResult = nullptr;

        aspl::Convert::ToFoundation(negInput, negResult);
        ASSERT_NE(negResult, nullptr);

        Float64 negBackResult;
        EXPECT_TRUE(aspl::Convert::FromFoundation(negResult, negBackResult));
        EXPECT_DOUBLE_EQ(negBackResult, -123.456);

        CFRelease(negResult);
    }
}

TEST(ConvertTest, Serde_CFString)
{
    { // string serialization
        CFStringRef originalString = CFStringCreateWithCString(
            kCFAllocatorDefault, "Test serialization", kCFStringEncodingUTF8);
        ASSERT_NE(originalString, nullptr);

        std::vector<UInt8> serialized;
        aspl::Convert::Serialize(originalString, serialized);
        EXPECT_FALSE(serialized.empty());

        CFPropertyListRef deserialized = nullptr;
        EXPECT_TRUE(aspl::Convert::Deserialize(serialized, deserialized));
        ASSERT_NE(deserialized, nullptr);
        EXPECT_EQ(CFGetTypeID(deserialized), CFStringGetTypeID());

        std::string originalStr, deserializedStr;
        EXPECT_TRUE(aspl::Convert::FromFoundation(originalString, originalStr));
        EXPECT_TRUE(aspl::Convert::FromFoundation(deserialized, deserializedStr));
        EXPECT_EQ(originalStr, deserializedStr);

        CFRelease(originalString);
        CFRelease(deserialized);
    }

    { // empty data error
        std::vector<UInt8> emptyData;
        CFPropertyListRef emptyResult = nullptr;
        EXPECT_FALSE(aspl::Convert::Deserialize(emptyData, emptyResult));
        EXPECT_EQ(emptyResult, nullptr);
    }

    { // null input
        std::vector<UInt8> nullSerialized;
        aspl::Convert::Serialize(nullptr, nullSerialized);
        EXPECT_TRUE(nullSerialized.empty());
    }
}

TEST(ConvertTest, Serde_CFData)
{
    { // data serialization
        std::vector<UInt8> originalData = {0x01, 0x02, 0x03, 0xFF};
        CFDataRef originalCFData =
            CFDataCreate(kCFAllocatorDefault, originalData.data(), originalData.size());
        ASSERT_NE(originalCFData, nullptr);

        std::vector<UInt8> serialized;
        aspl::Convert::Serialize(originalCFData, serialized);
        EXPECT_FALSE(serialized.empty());

        CFPropertyListRef deserialized = nullptr;
        EXPECT_TRUE(aspl::Convert::Deserialize(serialized, deserialized));
        ASSERT_NE(deserialized, nullptr);
        EXPECT_EQ(CFGetTypeID(deserialized), CFDataGetTypeID());

        std::vector<UInt8> deserializedData;
        EXPECT_TRUE(aspl::Convert::FromFoundation(deserialized, deserializedData));
        EXPECT_EQ(originalData, deserializedData);

        CFRelease(originalCFData);
        CFRelease(deserialized);
    }

    { // empty data error
        std::vector<UInt8> emptyData;
        CFPropertyListRef emptyResult = nullptr;
        EXPECT_FALSE(aspl::Convert::Deserialize(emptyData, emptyResult));
        EXPECT_EQ(emptyResult, nullptr);
    }

    { // null input
        std::vector<UInt8> nullSerialized;
        aspl::Convert::Serialize(nullptr, nullSerialized);
        EXPECT_TRUE(nullSerialized.empty());
    }
}

TEST(ConvertTest, Serde_CFBoolean)
{
    { // boolean serialization
        CFBooleanRef originalBool = kCFBooleanTrue;

        std::vector<UInt8> serialized;
        aspl::Convert::Serialize(originalBool, serialized);
        EXPECT_FALSE(serialized.empty());

        CFPropertyListRef deserialized = nullptr;
        EXPECT_TRUE(aspl::Convert::Deserialize(serialized, deserialized));
        ASSERT_NE(deserialized, nullptr);
        EXPECT_EQ(CFGetTypeID(deserialized), CFBooleanGetTypeID());

        bool deserializedBool;
        EXPECT_TRUE(aspl::Convert::FromFoundation(deserialized, deserializedBool));
        EXPECT_TRUE(deserializedBool);

        CFRelease(deserialized);
    }

    { // empty data error
        std::vector<UInt8> emptyData;
        CFPropertyListRef emptyResult = nullptr;
        EXPECT_FALSE(aspl::Convert::Deserialize(emptyData, emptyResult));
        EXPECT_EQ(emptyResult, nullptr);
    }

    { // null input
        std::vector<UInt8> nullSerialized;
        aspl::Convert::Serialize(nullptr, nullSerialized);
        EXPECT_TRUE(nullSerialized.empty());
    }
}

TEST(ConvertTest, Serde_CFNumber)
{
    { // number serialization
        SInt64 originalValue = 42;
        CFNumberRef originalNumber =
            CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &originalValue);
        ASSERT_NE(originalNumber, nullptr);

        std::vector<UInt8> serialized;
        aspl::Convert::Serialize(originalNumber, serialized);
        EXPECT_FALSE(serialized.empty());

        CFPropertyListRef deserialized = nullptr;
        EXPECT_TRUE(aspl::Convert::Deserialize(serialized, deserialized));
        ASSERT_NE(deserialized, nullptr);
        EXPECT_EQ(CFGetTypeID(deserialized), CFNumberGetTypeID());

        SInt64 deserializedValue;
        EXPECT_TRUE(aspl::Convert::FromFoundation(deserialized, deserializedValue));
        EXPECT_EQ(originalValue, deserializedValue);

        CFRelease(originalNumber);
        CFRelease(deserialized);
    }

    { // empty data error
        std::vector<UInt8> emptyData;
        CFPropertyListRef emptyResult = nullptr;
        EXPECT_FALSE(aspl::Convert::Deserialize(emptyData, emptyResult));
        EXPECT_EQ(emptyResult, nullptr);
    }

    { // null input
        std::vector<UInt8> nullSerialized;
        aspl::Convert::Serialize(nullptr, nullSerialized);
        EXPECT_TRUE(nullSerialized.empty());
    }
}
