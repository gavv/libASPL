// Copyright (c) libASPL authors
// Licensed under MIT

//! @file aspl/DriverLoader.hpp
//! @brief Driver bundle loader.

#pragma once

#include <aspl/Tracer.hpp>

#include <CoreAudio/AudioServerPlugIn.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace aspl {

//! Information about driver bundle.
//!
//! This struct contains metadata extracted from a macOS bundle that implements
//! an AudioServer HAL plugin (driver). The information is populated by reading
//! the bundle's Info.plist file and examining the bundle structure.
//!
//! A driver bundle is a macOS bundle (directory with .driver extension) that
//! contains a shared library implementing the AudioServer plugin interface.
//! The bundle follows the standard macOS bundle layout:
//!
//! @code
//!   /Library/Audio/Plug-Ins/HAL/Example.driver
//!   └── Contents
//!       ├── MacOS
//!       │   └── ExampleExecutable
//!       └── Info.plist
//! @endcode
struct DriverBundleInfo
{
    //! Bundle path.
    //! Full path to the bundle directory.
    //! Example: "/Library/Audio/Plug-Ins/HAL/Example.driver".
    std::string BundlePath;

    //! Path to shared library.
    //! Full path to the executable shared library within the bundle.
    //! Typically located at `{BundlePath}/Contents/MacOS/{BundleExecutable}`.
    std::string SharedLibPath;

    //! Path to Info.plist.
    //! Full path to the bundle's property list file.
    //! Typically located at `{BundlePath}/Contents/Info.plist`.
    std::string PlistPath;

    //! Bundle identifier.
    //! Unique identifier for the bundle, typically in reverse DNS format.
    //! Example: "com.example.driver".
    //! @remarks
    //!  Comes from CFBundleIdentifier plist field.
    std::string BundleIdentifier;

    //! Bundle name.
    //! Human-readable name of the bundle.
    //! Typically matches bundle directory name without ".driver" extension.
    //! @remarks
    //!  Comes from CFBundleName plist field.
    std::string BundleName;

    //! Bundle executable name.
    //! Name of the bundle executable (shared library in case of audio drivers).
    //! @remarks
    //!  Comes from CFBundleExecutable plist field.
    std::string BundleExecutable;

    //! Bundle version.
    //! Version string of the bundle.
    //! @remarks
    //!  Comes from CFBundleVersion plist field.
    std::string BundleVersion;

    //! Bundle short version.
    //! Short version string of the bundle, typically user-facing.
    //! @remarks
    //!  Comes from CFBundleShortVersionString plist field.
    std::string BundleShortVersion;

    //! Bundle development region.
    //! Primary language/region for the bundle's localization.
    //! @remarks
    //!  Comes from CFBundleDevelopmentRegion plist field.
    std::string BundleDevelopmentRegion;

    //! Bundle package type.
    //! Should be "BNDL" for drivers, which stands for Mach-O bundle.
    //! @remarks
    //!  Comes from CFBundlePackageType plist field.
    std::string BundlePackageType;

    //! Copyright notice.
    //! Human-readable copyright information.
    //! @remarks
    //!  Comes from NSHumanReadableCopyright plist field.
    std::string Copyright;

    //! Mach services.
    //! List of Mach service names that the plugin may use.
    //! @remarks
    //!  Comes from AudioServerPlugIn_MachServices plist field.
    std::vector<std::string> MachServices;

    //! Plugin factory UUIDs.
    //! List of plugin factory UUIDs in the order they appear in the plist.
    //! These UUIDs identify factory functions that can construct driver instances.
    //! Example:
    //!  ["95963308-3BB7-48FC-A58D-5EC8E7578083", ...]
    //! @remarks
    //!  Based on CFPlugInTypes plist field.
    std::vector<std::string> PlugInFactories;

    //! Plugin entry points mapping UUID to function name.
    //! Maps plugin factory UUIDs to corresponding factory function name (entry point
    //! function). HAL (and DriverLoader) finds these entry points in the shared
    //! library and invokes to construct driver.
    //! Example mapping:
    //!  {"95963308-3BB7-48FC-A58D-5EC8E7578083" => "MyEntryPoint", ...}
    //! @remarks
    //!  Based on CFPlugInTypes and CFPlugInFactories plist fields.
    std::map<std::string, std::string> PlugInEntrypoints;
};

//! Driver loader parameters.
struct DriverLoaderParameters
{
    //! Directory to search for driver bundles.
    //! Defaults to /Library/Audio/Plug-Ins/HAL if empty.
    std::string HalDirectory;

    //! Enable QueryInterface when loading drivers.
    //! If true, AudioServerPlugInDriverRef is obtained from QueryInterface call
    //! on the IUnknown returned from entry point.
    //! Otherwise, entry point result is treated directly as AudioServerPlugInDriverRef.
    //! Normally should be enabled, disable if driver doesn't implement it.
    bool EnableQueryInterface = true;
};

//! Driver bundle loader.
//!
//! This class provides functionality to discover, inspect, and load AudioServer
//! HAL plugin bundles from the filesystem. It can search for driver bundles in
//! a specified directory or in the standard system location, extract metadata
//! from bundle Info.plist files, and dynamically load drivers.
//!
//! The loader searches for bundles with the .driver extension and validates
//! that they contain the required AudioServer plugin components.
class DriverLoader
{
public:
    //! Construct loader with optional tracer and parameters.
    explicit DriverLoader(std::shared_ptr<Tracer> tracer = {},
        const DriverLoaderParameters& params = {});

    DriverLoader(const DriverLoader&) = delete;
    DriverLoader& operator=(const DriverLoader&) = delete;

    //! Destroy driver loader.
    //! Does NOT release or unload any loaded drivers.
    ~DriverLoader() = default;

    //! List all driver bundles.
    //!
    //! Scans the configured directory for .driver bundles and returns their paths.
    //! Only returns bundles that appear to be valid AudioServer plugins.
    //!
    //! @returns
    //!  vector of bundle paths found in the search directory.
    std::vector<std::string> ListBundles() const;

    //! Find bundle by CFBundleIdentifier.
    //!
    //! Searches for a driver bundle with the specified bundle identifier.
    //! See DriverBundleInfo::BundleIdentifier.
    //!
    //! @returns
    //!  path to the bundle if found, std::nullopt otherwise.
    std::optional<std::string> FindByIdentifier(const std::string& identifier) const;

    //! Find bundle by CFBundleName.
    //!
    //! Searches for a driver bundle with the specified bundle name.
    //! See DriverBundleInfo::BundleName.
    //!
    //! @returns
    //!  path to the bundle if found, std::nullopt otherwise.
    std::optional<std::string> FindByName(const std::string& name) const;

    //! Find bundle by CFPlugInFactories.
    //!
    //! Searches for a driver bundle that provides specified factory UUID.
    //! See DriverBundleInfo::PlugInFactories.
    //!
    //! @returns
    //!  path to the bundle if found, std::nullopt otherwise.
    std::optional<std::string> FindByUUID(const std::string& uuid) const;

    //! Read bundle information.
    //!
    //! Extracts metadata from the specified driver bundle by parsing its Info.plist
    //! and examining the bundle structure.
    //!
    //! @returns
    //!  bundle information if successfully parsed, std::nullopt on error.
    std::optional<DriverBundleInfo> ReadBundleInfo(const std::string& bundlePath) const;

    //! Validate bundle information.
    //!
    //! Check if bundle satisfies minimal requirements so that it can be loaded.
    //!
    //! @returns
    //!  true if bundle is fine and OpenDriver() can load it.
    bool ValidateBundleInfo(const DriverBundleInfo& info) const;

    //! Find entry point name.
    //!
    //! If factoryUUID is non-empty, it defines key in CFPlugInFactories dictionary.
    //! Otherwise, the first key is used. (Usually there is only one).
    //!
    //! @returns
    //!  non-empty entry point name, if found.
    std::optional<std::string> ResolveEntryPoint(const DriverBundleInfo& info,
        const std::string& factoryUUID = {}) const;

    //! Handle for opened driver.
    //! Holds AudioServerPlugInDriverRef and shared library handle.
    //! @see DriverLoader.
    class Handle
    {
    public:
        //! Release and unload driver.
        //!
        //! 1. Calls Release() method of AudioServerPlugInDriverRef
        //! 2. Unloads driver's shared library from memory
        //!
        //! @note
        //!  Driver's reference counter is 1 upon initialization ans is expected to remain
        //!  so upon destruction. If you call driver's AddRef() manually, be sure to call
        //!  Release() before destroying Handle. Otherwise, driver deinitialization
        //!  won't be trigerred, but driver's library will be unloaded anyway.
        ~Handle();

        //! Get driver reference.
        //! @note
        //!  Does not touch driver's reference counter.
        AudioServerPlugInDriverRef GetReference();

        //! Abandon driver reference ownership.
        //! Handle forgets about driver reference. Destructor won't call Release(),
        //! but will still unload driver's library.
        //! @note
        //!  Does not touch driver's reference counter.
        void Disown();

    private:
        friend class DriverLoader;

        std::shared_ptr<Tracer> tracer;
        std::string libPath;
        void* libHandle = nullptr;
        AudioServerPlugInDriverRef driverRef = nullptr;
    };

    //! Load and open driver from bundle.
    //!
    //! 1. Calls ReadBundleInfo(), ValidateBundleInfo(), and ResolveEntryPoint()
    //! 2. Loads driver's shared library using dlopen()
    //! 3. Obtains drivers' entry point using dlsym()
    //! 4. Invokes entry point to obtain driver's IUnknown object
    //! 5. Invokes QueryInterface() and returns obtained AudioServerPlugInDriverRef
    //!
    //! QueryInterface() step can be disabled, see DriverLoaderParameters.
    //!
    //! If factoryUUID is non-empty, it defines which entry point to use. Otherwise,
    //! the first one is used. (Usually there is only one). See ResolveEntryPoint().
    //!
    //! This method does NOT call Initialize() or any other driver methods besides
    //! QueryInterface(), AddRef(), and Release().
    //!
    //! Returns Handle object, which owns both driver and its shared library.
    //! When Handle is destroyed, it automatically calls Release() on driver
    //! and unloads library using dlclose().
    std::shared_ptr<Handle> LoadDriver(const std::string& bundlePath,
        const std::string& factoryUUID = {});

private:
    void* OpenLibrary(const std::string& libPath);
    static void CloseLibrary(const std::shared_ptr<Tracer>& tracer,
        const std::string& libPath,
        void* libHandle);

    AudioServerPlugInDriverRef OpenDriver(void* entryPointResult);
    static void CloseDriver(const std::shared_ptr<Tracer>& tracer,
        AudioServerPlugInDriverRef driverRef);

    void* InvokeEntryPoint(void* libHandle, const std::string& entryPoint);

    const std::shared_ptr<Tracer> tracer_;
    const DriverLoaderParameters params_;
};

} // namespace aspl
