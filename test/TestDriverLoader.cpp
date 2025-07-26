// Copyright (c) libASPL authors
// Licensed under MIT

#include "aspl/DriverLoader.hpp"

#include "MockTracer.hpp"

#include <gtest/gtest.h>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <string>
#include <vector>

using namespace aspl;

namespace {

const std::string exampleDriver1Plist = R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
  <dict>
    <key>AudioServerPlugIn_MachServices</key>
    <array>
        <string>com.example.service1</string>
        <string>com.example.service2</string>
    </array>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleExecutable</key>
    <string>ExampleDriver1Exe</string>
    <key>CFBundleIdentifier</key>
    <string>com.example.driver1</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>ExampleDriver1</string>
    <key>CFBundlePackageType</key>
    <string>BNDL</string>
    <key>CFBundleShortVersionString</key>
    <string>0.0.1</string>
    <key>CFBundleSignature</key>
    <string>????</string>
    <key>CFBundleSupportedPlatforms</key>
    <array>
        <string>MacOSX</string>
    </array>
    <key>CFBundleVersion</key>
    <string>0.0.1</string>
    <key>CFPlugInFactories</key>
    <dict>
        <key>95963308-3BB7-48FC-A58D-5EC8E7578083</key>
        <string>EntryPoint1</string>
    </dict>
    <key>CFPlugInTypes</key>
    <dict>
        <key>443ABAB8-E7B3-491A-B985-BEB9187030DB</key>
        <array>
            <string>95963308-3BB7-48FC-A58D-5EC8E7578083</string>
        </array>
    </dict>
    <key>NSHumanReadableCopyright</key>
    <string>Copyright (c) Example Human 1.</string>
    <key>NSPrincipalClass</key>
    <string></string>
  </dict>
</plist>)";

const std::string exampleDriver2Plist = R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
  <dict>
    <key>AudioServerPlugIn_MachServices</key>
    <array>
    </array>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleExecutable</key>
    <string>ExampleDriver2Exe</string>
    <key>CFBundleIdentifier</key>
    <string>com.example.driver2</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>ExampleDriver2</string>
    <key>CFBundlePackageType</key>
    <string>BNDL</string>
    <key>CFBundleShortVersionString</key>
    <string>0.0.2</string>
    <key>CFBundleSignature</key>
    <string>????</string>
    <key>CFBundleSupportedPlatforms</key>
    <array>
        <string>MacOSX</string>
    </array>
    <key>CFBundleVersion</key>
    <string>0.0.2</string>
    <key>CFPlugInFactories</key>
    <dict>
        <key>30F8E35D-FB97-4A9C-A469-9038A751B4B6</key>
        <string>EntryPoint2</string>
        <key>B1C2D3E4-F5A6-7890-BCDE-F12345678901</key>
        <string>EntryPoint2Alt</string>
    </dict>
    <key>CFPlugInTypes</key>
    <dict>
        <key>443ABAB8-E7B3-491A-B985-BEB9187030DB</key>
        <array>
            <string>30F8E35D-FB97-4A9C-A469-9038A751B4B6</string>
            <string>B1C2D3E4-F5A6-7890-BCDE-F12345678901</string>
        </array>
    </dict>
    <key>NSHumanReadableCopyright</key>
    <string>Copyright (c) Example Human 2.</string>
    <key>NSPrincipalClass</key>
    <string></string>
  </dict>
</plist>)";

std::string CreateTempDir()
{
    char tempPath[] = "/tmp/aspl_test_XXXXXX";
    if (mkdtemp(tempPath) == nullptr) {
        return "";
    }
    return std::string(tempPath);
}

bool CreateDir(const std::string& path)
{
    return mkdir(path.c_str(), 0755) == 0;
}

bool WriteFile(const std::string& path, const std::string& content)
{
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    file << content;
    return file.good();
}

void RemoveDir(const std::string& path)
{
    if (path.empty()) {
        return;
    }

    DIR* dir = opendir(path.c_str());
    if (dir == nullptr) {
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        std::string fullPath = path + "/" + entry->d_name;

        struct stat statBuf;
        if (stat(fullPath.c_str(), &statBuf) == 0) {
            if (S_ISDIR(statBuf.st_mode)) {
                RemoveDir(fullPath);
            } else {
                unlink(fullPath.c_str());
            }
        }
    }

    closedir(dir);
    rmdir(path.c_str());
}

} // namespace

struct DriverLoaderTest : testing::Test
{
    std::shared_ptr<aspl::Tracer> tracer;

    std::string halDir;

    void SetUp() override
    {
        tracer = std::make_shared<MockTracer>();

        halDir = CreateTempDir();
        ASSERT_FALSE(halDir.empty());

        { // example_driver1.driver
            std::string bundlePath = halDir + "/example_driver1.driver";
            std::string contentsPath = bundlePath + "/Contents";
            std::string macosPath = contentsPath + "/MacOS";

            ASSERT_TRUE(CreateDir(bundlePath));
            ASSERT_TRUE(CreateDir(contentsPath));
            ASSERT_TRUE(CreateDir(macosPath));

            ASSERT_TRUE(WriteFile(macosPath + "/ExampleDriver1Exe", ""));
            ASSERT_TRUE(WriteFile(contentsPath + "/Info.plist", exampleDriver1Plist));
        }

        { // example_driver2.driver
            std::string bundlePath = halDir + "/example_driver2.driver";
            std::string contentsPath = bundlePath + "/Contents";
            std::string macosPath = contentsPath + "/MacOS";

            ASSERT_TRUE(CreateDir(bundlePath));
            ASSERT_TRUE(CreateDir(contentsPath));
            ASSERT_TRUE(CreateDir(macosPath));

            ASSERT_TRUE(WriteFile(macosPath + "/ExampleDriver2Exe", ""));
            ASSERT_TRUE(WriteFile(contentsPath + "/Info.plist", exampleDriver2Plist));
        }
    }

    void TearDown() override
    {
        RemoveDir(halDir);
    }
};

TEST_F(DriverLoaderTest, ListBundles)
{
    DriverLoaderParameters params;
    params.HalDirectory = halDir;

    DriverLoader loader(tracer, params);

    auto bundles = loader.ListBundles();

    ASSERT_EQ(bundles.size(), 2);
    EXPECT_EQ(bundles[0], halDir + "/example_driver1.driver");
    EXPECT_EQ(bundles[1], halDir + "/example_driver2.driver");
}

TEST_F(DriverLoaderTest, FindBundle)
{
    struct TestCase
    {
        std::string BundleIdentifier;
        std::string BundleName;
        std::string FactoryUUID;
        std::optional<std::string> ExpectedPath;
    };

    const std::vector<TestCase> testCases = {
        // driver 1
        {
            "com.example.driver1",
            "ExampleDriver1",
            "95963308-3BB7-48FC-A58D-5EC8E7578083",
            halDir + "/example_driver1.driver",
        },
        // driver 2
        {
            "com.example.driver2",
            "ExampleDriver2",
            "30F8E35D-FB97-4A9C-A469-9038A751B4B6",
            halDir + "/example_driver2.driver",
        },
        // non-existent
        {
            "com.nonexistent.driver",
            "NonExistentDriver",
            "00000000-0000-0000-0000-000000000000",
            std::nullopt,
        },
    };

    DriverLoaderParameters params;
    params.HalDirectory = halDir;

    DriverLoader loader(tracer, params);

    for (const auto& testCase : testCases) {
        { // FindByIdentifier
            auto result = loader.FindByIdentifier(testCase.BundleIdentifier);
            EXPECT_EQ(result, testCase.ExpectedPath);
        }

        { // FindByName
            auto result = loader.FindByName(testCase.BundleName);
            EXPECT_EQ(result, testCase.ExpectedPath);
        }

        { // FindByUUID
            auto result = loader.FindByUUID(testCase.FactoryUUID);
            EXPECT_EQ(result, testCase.ExpectedPath);
        }
    }
}

TEST_F(DriverLoaderTest, BundleInfo)
{
    DriverLoaderParameters params;
    params.HalDirectory = halDir;

    DriverLoader loader(tracer, params);

    { // driver1
        auto info = loader.ReadBundleInfo(halDir + "/example_driver1.driver");
        ASSERT_TRUE(info);

        // fields
        EXPECT_EQ(info->BundlePath, halDir + "/example_driver1.driver");
        EXPECT_EQ(info->SharedLibPath,
            halDir + "/example_driver1.driver/Contents/MacOS/ExampleDriver1Exe");
        EXPECT_EQ(
            info->PlistPath, halDir + "/example_driver1.driver/Contents/Info.plist");

        EXPECT_EQ(info->BundleIdentifier, "com.example.driver1");
        EXPECT_EQ(info->BundleName, "ExampleDriver1");
        EXPECT_EQ(info->BundleExecutable, "ExampleDriver1Exe");
        EXPECT_EQ(info->BundleVersion, "0.0.1");
        EXPECT_EQ(info->BundleShortVersion, "0.0.1");
        EXPECT_EQ(info->BundleDevelopmentRegion, "en");
        EXPECT_EQ(info->BundlePackageType, "BNDL");
        EXPECT_EQ(info->Copyright, "Copyright (c) Example Human 1.");

        ASSERT_EQ(info->MachServices.size(), 2);
        EXPECT_EQ(info->MachServices[0], "com.example.service1");
        EXPECT_EQ(info->MachServices[1], "com.example.service2");
        ASSERT_EQ(info->PlugInFactories.size(), 1);
        EXPECT_EQ(info->PlugInFactories[0], "95963308-3BB7-48FC-A58D-5EC8E7578083");
        ASSERT_EQ(info->PlugInEntrypoints.size(), 1);
        EXPECT_EQ(info->PlugInEntrypoints.at("95963308-3BB7-48FC-A58D-5EC8E7578083"),
            "EntryPoint1");

        // entry points
        EXPECT_EQ(loader.ResolveEntryPoint(*info), "EntryPoint1");
        EXPECT_EQ(loader.ResolveEntryPoint(*info, "95963308-3BB7-48FC-A58D-5EC8E7578083"),
            "EntryPoint1");
        EXPECT_FALSE(
            loader.ResolveEntryPoint(*info, "00000000-0000-0000-0000-000000000000"));

        // validation
        EXPECT_TRUE(loader.ValidateBundleInfo(*info));
    }

    { // driver2
        auto info = loader.ReadBundleInfo(halDir + "/example_driver2.driver");
        ASSERT_TRUE(info);

        // fields
        EXPECT_EQ(info->BundlePath, halDir + "/example_driver2.driver");
        EXPECT_EQ(info->SharedLibPath,
            halDir + "/example_driver2.driver/Contents/MacOS/ExampleDriver2Exe");
        EXPECT_EQ(
            info->PlistPath, halDir + "/example_driver2.driver/Contents/Info.plist");

        EXPECT_EQ(info->BundleIdentifier, "com.example.driver2");
        EXPECT_EQ(info->BundleName, "ExampleDriver2");
        EXPECT_EQ(info->BundleExecutable, "ExampleDriver2Exe");
        EXPECT_EQ(info->BundleVersion, "0.0.2");
        EXPECT_EQ(info->BundleShortVersion, "0.0.2");
        EXPECT_EQ(info->BundleDevelopmentRegion, "en");
        EXPECT_EQ(info->BundlePackageType, "BNDL");
        EXPECT_EQ(info->Copyright, "Copyright (c) Example Human 2.");

        EXPECT_TRUE(info->MachServices.empty());
        ASSERT_EQ(info->PlugInFactories.size(), 2);
        EXPECT_EQ(info->PlugInFactories[0], "30F8E35D-FB97-4A9C-A469-9038A751B4B6");
        EXPECT_EQ(info->PlugInFactories[1], "B1C2D3E4-F5A6-7890-BCDE-F12345678901");
        ASSERT_EQ(info->PlugInEntrypoints.size(), 2);
        EXPECT_EQ(info->PlugInEntrypoints.at("30F8E35D-FB97-4A9C-A469-9038A751B4B6"),
            "EntryPoint2");
        EXPECT_EQ(info->PlugInEntrypoints.at("B1C2D3E4-F5A6-7890-BCDE-F12345678901"),
            "EntryPoint2Alt");

        // entry points
        EXPECT_EQ(loader.ResolveEntryPoint(*info), "EntryPoint2");
        EXPECT_EQ(loader.ResolveEntryPoint(*info, "30F8E35D-FB97-4A9C-A469-9038A751B4B6"),
            "EntryPoint2");
        EXPECT_EQ(loader.ResolveEntryPoint(*info, "B1C2D3E4-F5A6-7890-BCDE-F12345678901"),
            "EntryPoint2Alt");
        EXPECT_FALSE(
            loader.ResolveEntryPoint(*info, "00000000-0000-0000-0000-000000000000"));

        // validation
        EXPECT_TRUE(loader.ValidateBundleInfo(*info));
    }
}
