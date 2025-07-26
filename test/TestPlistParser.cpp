// Copyright (c) libASPL authors
// Licensed under MIT

#include "PlistParser.hpp"

#include <gtest/gtest.h>

#include <fstream>
#include <string>
#include <vector>

using namespace aspl;

namespace {

const std::string testPlistContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
  <dict>
    <key>AudioServerPlugIn_MachServices</key>
    <array>
    </array>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleExecutable</key>
    <string>ExampleDriverExe</string>
    <key>CFBundleIdentifier</key>
    <string>com.example.driver</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>ExampleDriver</string>
    <key>CFBundlePackageType</key>
    <string>BNDL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.2.3</string>
    <key>CFBundleSignature</key>
    <string>????</string>
    <key>CFBundleSupportedPlatforms</key>
    <array>
        <string>MacOSX</string>
    </array>
    <key>CFBundleVersion</key>
    <string>1.2.3</string>
    <key>CFPlugInFactories</key>
    <dict>
        <key>95963308-3BB7-48FC-A58D-5EC8E7578083</key>
        <string>EntryPoint</string>
    </dict>
    <key>CFPlugInTypes</key>
    <dict>
        <key>443ABAB8-E7B3-491A-B985-BEB9187030DB</key>
        <array>
            <string>95963308-3BB7-48FC-A58D-5EC8E7578083</string>
        </array>
    </dict>
    <key>NSHumanReadableCopyright</key>
    <string>Copyright (c) Example Human.</string>
    <key>NSPrincipalClass</key>
    <string></string>
    <key>exampleTrueValue</key>
    <true/>
    <key>exampleFalseValue</key>
    <false/>
    <key>exampleIntegerValue</key>
    <integer>42</integer>
    <key>exampleRealValue</key>
    <real>3.14159</real>
  </dict>
</plist>)";

std::string CreateTempFile(const std::string& content)
{
    char tempPath[] = "/tmp/aspl_test_XXXXXX";
    int fd = mkstemp(tempPath);
    if (fd == -1) {
        return "";
    }

    std::ofstream file(tempPath);
    file << content;
    file.close();
    close(fd);

    return std::string(tempPath);
}

void RemoveFile(const std::string& path)
{
    if (!path.empty()) {
        unlink(path.c_str());
    }
}

} // namespace

struct PlistParserTest : testing::Test
{
    std::string plistPath;

    void SetUp() override
    {
        plistPath = CreateTempFile(testPlistContent);
        ASSERT_FALSE(plistPath.empty());
    }

    void TearDown() override
    {
        RemoveFile(plistPath);
    }
};

TEST_F(PlistParserTest, OpenClose)
{
    { // explicit
        PlistParser parser;

        EXPECT_TRUE(parser.Open(plistPath));
        parser.Close();
    }

    { // dtor
        PlistParser parser;

        EXPECT_TRUE(parser.Open(plistPath));
    }
}

TEST_F(PlistParserTest, OpenError)
{
    { // non-existent file
        PlistParser parser;
        EXPECT_FALSE(parser.Open("/tmp/nonexistent.plist"));
    }

    { // invalid plist content
        std::string invalidPlistPath = CreateTempFile("not a plist");
        ASSERT_FALSE(invalidPlistPath.empty());

        PlistParser parser;
        EXPECT_FALSE(parser.Open(invalidPlistPath));

        RemoveFile(invalidPlistPath);
    }

    { // empty file
        std::string emptyPlistPath = CreateTempFile("");
        ASSERT_FALSE(emptyPlistPath.empty());

        PlistParser parser;
        EXPECT_FALSE(parser.Open(emptyPlistPath));

        RemoveFile(emptyPlistPath);
    }
}

TEST_F(PlistParserTest, GetString)
{
    PlistParser parser;
    ASSERT_TRUE(parser.Open(plistPath));

    { // CFBundleExecutable
        auto bundleExe = parser.GetValue("CFBundleExecutable");
        ASSERT_TRUE(bundleExe);
        auto bundleExeStr = bundleExe->AsString();
        ASSERT_TRUE(bundleExeStr);
        EXPECT_EQ(*bundleExeStr, "ExampleDriverExe");
    }

    { // CFBundleIdentifier
        auto bundleId = parser.GetValue("CFBundleIdentifier");
        ASSERT_TRUE(bundleId);
        auto bundleIdStr = bundleId->AsString();
        ASSERT_TRUE(bundleIdStr);
        EXPECT_EQ(*bundleIdStr, "com.example.driver");
    }

    { // empty string
        auto principalClass = parser.GetValue("NSPrincipalClass");
        ASSERT_TRUE(principalClass);
        auto principalClassStr = principalClass->AsString();
        ASSERT_TRUE(principalClassStr);
        EXPECT_EQ(*principalClassStr, "");
    }

    { // string with special characters
        auto copyright = parser.GetValue("NSHumanReadableCopyright");
        ASSERT_TRUE(copyright);
        auto copyrightStr = copyright->AsString();
        ASSERT_TRUE(copyrightStr);
        EXPECT_EQ(*copyrightStr, "Copyright (c) Example Human.");
    }
}

TEST_F(PlistParserTest, GetNumber)
{
    PlistParser parser;
    ASSERT_TRUE(parser.Open(plistPath));

    { // integer
        auto intValue = parser.GetValue("exampleIntegerValue");
        ASSERT_TRUE(intValue);
        auto intInt = intValue->AsInteger();
        ASSERT_TRUE(intInt);
        EXPECT_EQ(*intInt, 42);
    }

    { // real
        auto exampleRealValue = parser.GetValue("exampleRealValue");
        ASSERT_TRUE(exampleRealValue);
        auto realReal = exampleRealValue->AsReal();
        ASSERT_TRUE(realReal);
        EXPECT_NEAR(*realReal, 3.14159, 0.00001);
    }

    { // integer as real
        auto intValue = parser.GetValue("exampleIntegerValue");
        ASSERT_TRUE(intValue);
        auto intReal = intValue->AsReal();
        ASSERT_TRUE(intReal);
        EXPECT_NEAR(*intReal, 42.0, 0.00001);
    }

    { // real as integer
        auto exampleRealValue = parser.GetValue("exampleRealValue");
        ASSERT_TRUE(exampleRealValue);
        auto realInt = exampleRealValue->AsInteger();
        EXPECT_FALSE(realInt);
    }
}

TEST_F(PlistParserTest, GetBoolean)
{
    PlistParser parser;
    ASSERT_TRUE(parser.Open(plistPath));

    { // exampleTrueValue
        auto exampleTrueValue = parser.GetValue("exampleTrueValue");
        ASSERT_TRUE(exampleTrueValue);
        auto exampleTrueValueBool = exampleTrueValue->AsBoolean();
        ASSERT_TRUE(exampleTrueValueBool);
        EXPECT_TRUE(*exampleTrueValueBool);
    }

    { // exampleFalseValue
        auto exampleFalseValue = parser.GetValue("exampleFalseValue");
        ASSERT_TRUE(exampleFalseValue);
        auto exampleFalseValueBool = exampleFalseValue->AsBoolean();
        ASSERT_TRUE(exampleFalseValueBool);
        EXPECT_FALSE(*exampleFalseValueBool);
    }
}

TEST_F(PlistParserTest, GetArray)
{
    PlistParser parser;
    ASSERT_TRUE(parser.Open(plistPath));

    { // empty array
        auto machServices = parser.GetValue("AudioServerPlugIn_MachServices");
        ASSERT_TRUE(machServices);
        auto machServicesArray = machServices->AsArray();
        ASSERT_TRUE(machServicesArray);
        EXPECT_TRUE(machServicesArray->empty());
    }

    { // array with one string element
        auto supportedPlatforms = parser.GetValue("CFBundleSupportedPlatforms");
        ASSERT_TRUE(supportedPlatforms);
        auto supportedPlatformsArray = supportedPlatforms->AsArray();
        ASSERT_TRUE(supportedPlatformsArray);
        EXPECT_EQ(supportedPlatformsArray->size(), 1);

        auto firstPlatform = supportedPlatformsArray->at(0).AsString();
        ASSERT_TRUE(firstPlatform);
        EXPECT_EQ(*firstPlatform, "MacOSX");
    }
}

TEST_F(PlistParserTest, GetDict)
{
    PlistParser parser;
    ASSERT_TRUE(parser.Open(plistPath));

    { // CFPlugInFactories dict
        auto pluginFactories = parser.GetValue("CFPlugInFactories");
        ASSERT_TRUE(pluginFactories);
        auto pluginFactoriesDict = pluginFactories->AsDict();
        ASSERT_TRUE(pluginFactoriesDict);
        EXPECT_EQ(pluginFactoriesDict->size(), 1);

        auto entryPoint =
            pluginFactoriesDict->find("95963308-3BB7-48FC-A58D-5EC8E7578083");
        ASSERT_NE(entryPoint, pluginFactoriesDict->end());
        auto entryPointStr = entryPoint->second.AsString();
        ASSERT_TRUE(entryPointStr);
        EXPECT_EQ(*entryPointStr, "EntryPoint");
    }

    { // CFPlugInTypes dict with nested array
        auto pluginTypes = parser.GetValue("CFPlugInTypes");
        ASSERT_TRUE(pluginTypes);
        auto pluginTypesDict = pluginTypes->AsDict();
        ASSERT_TRUE(pluginTypesDict);
        EXPECT_EQ(pluginTypesDict->size(), 1);

        auto halPluginArray =
            pluginTypesDict->find("443ABAB8-E7B3-491A-B985-BEB9187030DB");
        ASSERT_NE(halPluginArray, pluginTypesDict->end());
        auto halPluginArrayValue = halPluginArray->second.AsArray();
        ASSERT_TRUE(halPluginArrayValue);
        EXPECT_EQ(halPluginArrayValue->size(), 1);

        auto firstUUID = halPluginArrayValue->at(0).AsString();
        ASSERT_TRUE(firstUUID);
        EXPECT_EQ(*firstUUID, "95963308-3BB7-48FC-A58D-5EC8E7578083");
    }
}

TEST_F(PlistParserTest, NestedStructures)
{
    PlistParser parser;
    ASSERT_TRUE(parser.Open(plistPath));

    // navigate through nested dict -> array -> string
    auto pluginTypes = parser.GetValue("CFPlugInTypes");
    ASSERT_TRUE(pluginTypes);
    auto pluginTypesDict = pluginTypes->AsDict();
    ASSERT_TRUE(pluginTypesDict);

    auto halEntry = pluginTypesDict->find("443ABAB8-E7B3-491A-B985-BEB9187030DB");
    ASSERT_NE(halEntry, pluginTypesDict->end());

    auto halArray = halEntry->second.AsArray();
    ASSERT_TRUE(halArray);
    ASSERT_FALSE(halArray->empty());

    auto firstUUID = halArray->at(0).AsString();
    ASSERT_TRUE(firstUUID);
    EXPECT_EQ(*firstUUID, "95963308-3BB7-48FC-A58D-5EC8E7578083");
}

TEST_F(PlistParserTest, GetErrors)
{
    { // not opened
        PlistParser parser;

        auto value = parser.GetValue("CFBundleExecutable");
        EXPECT_FALSE(value);
    }

    { // not found
        PlistParser parser;
        ASSERT_TRUE(parser.Open(plistPath));

        auto nonExistent = parser.GetValue("NonExistentKey");
        EXPECT_FALSE(nonExistent);
    }
}

TEST_F(PlistParserTest, WrongType)
{
    PlistParser parser;
    ASSERT_TRUE(parser.Open(plistPath));

    { // try to get string as other types
        auto bundleExe = parser.GetValue("CFBundleExecutable");
        ASSERT_TRUE(bundleExe);

        auto bundleExeArray = bundleExe->AsArray();
        EXPECT_FALSE(bundleExeArray);

        auto bundleExeDict = bundleExe->AsDict();
        EXPECT_FALSE(bundleExeDict);

        auto bundleExeBool = bundleExe->AsBoolean();
        EXPECT_FALSE(bundleExeBool);

        auto bundleExeInt = bundleExe->AsInteger();
        EXPECT_FALSE(bundleExeInt);

        auto bundleExeReal = bundleExe->AsReal();
        EXPECT_FALSE(bundleExeReal);
    }

    { // try to get integer as other types
        auto intValue = parser.GetValue("exampleIntegerValue");
        ASSERT_TRUE(intValue);

        auto intString = intValue->AsString();
        EXPECT_FALSE(intString);

        auto intArray = intValue->AsArray();
        EXPECT_FALSE(intArray);

        auto intDict = intValue->AsDict();
        EXPECT_FALSE(intDict);

        auto intBool = intValue->AsBoolean();
        EXPECT_FALSE(intBool);
    }

    { // try to get real as other types
        auto exampleRealValue = parser.GetValue("exampleRealValue");
        ASSERT_TRUE(exampleRealValue);

        auto realString = exampleRealValue->AsString();
        EXPECT_FALSE(realString);

        auto realArray = exampleRealValue->AsArray();
        EXPECT_FALSE(realArray);

        auto realDict = exampleRealValue->AsDict();
        EXPECT_FALSE(realDict);

        auto realBool = exampleRealValue->AsBoolean();
        EXPECT_FALSE(realBool);
    }

    { // try to get boolean as other types
        auto boolValue = parser.GetValue("exampleTrueValue");
        ASSERT_TRUE(boolValue);

        auto boolString = boolValue->AsString();
        EXPECT_FALSE(boolString);

        auto boolArray = boolValue->AsArray();
        EXPECT_FALSE(boolArray);

        auto boolDict = boolValue->AsDict();
        EXPECT_FALSE(boolDict);

        auto boolInt = boolValue->AsInteger();
        EXPECT_FALSE(boolInt);

        auto boolReal = boolValue->AsReal();
        EXPECT_FALSE(boolReal);
    }

    { // try to get array as other types
        auto arrayValue = parser.GetValue("AudioServerPlugIn_MachServices");
        ASSERT_TRUE(arrayValue);

        auto arrayString = arrayValue->AsString();
        EXPECT_FALSE(arrayString);

        auto arrayInt = arrayValue->AsInteger();
        EXPECT_FALSE(arrayInt);

        auto arrayReal = arrayValue->AsReal();
        EXPECT_FALSE(arrayReal);

        auto arrayBool = arrayValue->AsBoolean();
        EXPECT_FALSE(arrayBool);

        auto arrayDict = arrayValue->AsDict();
        EXPECT_FALSE(arrayDict);
    }

    { // try to get dict as other types
        auto dictValue = parser.GetValue("CFPlugInFactories");
        ASSERT_TRUE(dictValue);

        auto dictString = dictValue->AsString();
        EXPECT_FALSE(dictString);

        auto dictInt = dictValue->AsInteger();
        EXPECT_FALSE(dictInt);

        auto dictReal = dictValue->AsReal();
        EXPECT_FALSE(dictReal);

        auto dictBool = dictValue->AsBoolean();
        EXPECT_FALSE(dictBool);

        auto dictArray = dictValue->AsArray();
        EXPECT_FALSE(dictArray);
    }
}

TEST_F(PlistParserTest, CopyAndMove)
{
    PlistParser parser;
    ASSERT_TRUE(parser.Open(plistPath));

    auto original = parser.GetValue("CFBundleExecutable");
    ASSERT_TRUE(original);

    { // copy constructor
        auto copied = *original;
        auto copiedStr = copied.AsString();
        ASSERT_TRUE(copiedStr);
        EXPECT_EQ(*copiedStr, "ExampleDriverExe");

        // original should still work
        auto originalStr = original->AsString();
        ASSERT_TRUE(originalStr);
        EXPECT_EQ(*originalStr, "ExampleDriverExe");
    }

    { // copy assignment
        auto assigned = parser.GetValue("CFBundleIdentifier");
        ASSERT_TRUE(assigned);
        assigned = *original;
        auto assignedStr = assigned->AsString();
        ASSERT_TRUE(assignedStr);
        EXPECT_EQ(*assignedStr, "ExampleDriverExe");
    }

    { // move constructor
        auto copied = *original;
        auto moved = std::move(copied);
        auto movedStr = moved.AsString();
        ASSERT_TRUE(movedStr);
        EXPECT_EQ(*movedStr, "ExampleDriverExe");

        // moved-from object should return nullopt
        auto copiedStr = copied.AsString();
        EXPECT_FALSE(copiedStr);
    }

    { // move assignment
        auto assigned = parser.GetValue("CFBundleIdentifier");
        ASSERT_TRUE(assigned);
        auto toMove = *original;
        assigned = std::move(toMove);
        auto assignedStr = assigned->AsString();
        ASSERT_TRUE(assignedStr);
        EXPECT_EQ(*assignedStr, "ExampleDriverExe");

        // moved-from object should return nullopt
        auto toMoveStr = toMove.AsString();
        EXPECT_FALSE(toMoveStr);
    }
}
