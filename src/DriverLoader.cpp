// Copyright (c) libASPL authors
// Licensed under MIT

#include "aspl/DriverLoader.hpp"

#include "PlistParser.hpp"
#include "Strings.hpp"

#include <CoreFoundation/CFPlugInCOM.h>
#include <CoreFoundation/CoreFoundation.h>

#include <dirent.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <vector>

namespace aspl {

namespace {

constexpr const char* HalPluginDirectory = "/Library/Audio/Plug-Ins/HAL";
constexpr const char* HalPluginTypeUUID = "443ABAB8-E7B3-491A-B985-BEB9187030DB";

std::vector<std::string> ListFilesAndDirs(const std::string& dirPath)
{
    std::vector<std::string> result;

    auto dir = opendir(dirPath.c_str());
    if (!dir) {
        return {};
    }

    while (auto entry = readdir(dir)) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        result.push_back(dirPath + "/" + entry->d_name);
    }
    closedir(dir);

    return result;
}

bool IsDir(const std::string& path)
{
    struct stat statBuf;
    if (stat(path.c_str(), &statBuf) != 0) {
        return false;
    }
    return S_ISDIR(statBuf.st_mode);
}

bool IsFile(const std::string& path)
{
    struct stat statBuf;
    if (stat(path.c_str(), &statBuf) != 0) {
        return false;
    }
    return S_ISREG(statBuf.st_mode);
}

bool HasSuffix(const std::string& str, const std::string& suffix)
{
    if (suffix.length() > str.length()) {
        return false;
    }
    return str.substr(str.length() - suffix.length()) == suffix;
}

std::string MakeStringField(const PlistParser& parser, const std::string& key)
{
    if (auto value = parser.GetValue(key)) {
        if (auto str = value->AsString()) {
            return *str;
        }
    }
    return {};
}

std::vector<std::string> MakeMachServicesField(const PlistParser& parser)
{
    std::vector<std::string> result;

    auto value = parser.GetValue("AudioServerPlugIn_MachServices");
    if (!value) {
        return result;
    }

    auto array = value->AsArray();
    if (!array) {
        return result;
    }

    for (const auto& item : *array) {
        auto str = item.AsString();
        if (!str) {
            continue;
        }
        result.push_back(*str);
    }

    return result;
}

std::vector<std::string> MakeFactoriesField(const PlistParser& parser)
{
    std::vector<std::string> result;

    auto pluginTypes = parser.GetValue("CFPlugInTypes");
    if (!pluginTypes) {
        return result;
    }

    auto pluginTypesDict = pluginTypes->AsDict();
    if (!pluginTypesDict) {
        return result;
    }

    // Find HalPluginTypeUUID in CFPlugInTypes
    auto halTypeIt = pluginTypesDict->find(HalPluginTypeUUID);
    if (halTypeIt == pluginTypesDict->end()) {
        return result;
    }

    // List factory UUIDs for plugin type HalPluginTypeUUID
    auto halTypeFactoryArray = halTypeIt->second.AsArray();
    if (!halTypeFactoryArray) {
        return result;
    }

    for (const auto& item : *halTypeFactoryArray) {
        auto factoryUUID = item.AsString();
        if (!factoryUUID) {
            continue;
        }
        result.push_back(*factoryUUID);
    }

    return result;
}

std::map<std::string, std::string> MakeEntrypointsField(const PlistParser& parser)
{
    std::map<std::string, std::string> result;

    auto pluginTypes = parser.GetValue("CFPlugInTypes");
    auto pluginFactories = parser.GetValue("CFPlugInFactories");

    if (!pluginTypes || !pluginFactories) {
        return result;
    }

    auto pluginTypesDict = pluginTypes->AsDict();
    auto pluginFactoriesDict = pluginFactories->AsDict();

    if (!pluginTypesDict || !pluginFactoriesDict) {
        return result;
    }

    // Find HalPluginTypeUUID in CFPlugInTypes
    auto halTypeIt = pluginTypesDict->find(HalPluginTypeUUID);
    if (halTypeIt == pluginTypesDict->end()) {
        return result;
    }

    // List factory UUIDs for plugin type HalPluginTypeUUID
    auto halTypeFactoryArray = halTypeIt->second.AsArray();
    if (!halTypeFactoryArray) {
        return result;
    }

    for (const auto& item : *halTypeFactoryArray) {
        auto factoryUUID = item.AsString();
        if (!factoryUUID) {
            continue;
        }

        // Find corresponding factory UUID in CFPlugInFactories
        auto factoryIt = pluginFactoriesDict->find(*factoryUUID);
        if (factoryIt == pluginFactoriesDict->end()) {
            continue;
        }

        // Finally, sigh
        auto entryPoint = factoryIt->second.AsString();
        if (!entryPoint) {
            continue;
        }

        result[*factoryUUID] = *entryPoint;
    }

    return result;
}

std::string MakeSharedLibField(const std::string& bundlePath,
    const std::string& bundleExecutable)
{
    if (bundlePath.empty() || bundleExecutable.empty()) {
        return {};
    }
    return bundlePath + "/Contents/MacOS/" + bundleExecutable;
}

} // anonymous namespace

DriverLoader::DriverLoader(std::shared_ptr<Tracer> tracer,
    const DriverLoaderParameters& params)
    : tracer_(
          tracer ? std::move(tracer) : std::make_shared<Tracer>(Tracer::Output::Stderr))
    , params_(params)
{
}

std::vector<std::string> DriverLoader::ListBundles() const
{
    std::vector<std::string> drivers;

    const std::string& halDirectory =
        params_.HalDirectory.empty() ? HalPluginDirectory : params_.HalDirectory;

    auto items = ListFilesAndDirs(halDirectory);
    if (items.empty()) {
        tracer_->Message("DriverLoader::ListBundles() skipping \"%s\": failed to read",
            halDirectory.c_str());
        return drivers;
    }

    std::sort(items.begin(), items.end());

    for (const auto& bundlePath : items) {
        if (!HasSuffix(bundlePath, ".driver")) {
            continue;
        }

        if (!IsDir(bundlePath)) {
            tracer_->Message(
                "DriverLoader::ListBundles() skipping \"%s\": not a directory",
                bundlePath.c_str());
            continue;
        }

        std::string plistPath = bundlePath + "/Contents/Info.plist";
        if (!IsFile(plistPath)) {
            tracer_->Message(
                "DriverLoader::ListBundles() skipping \"%s\": missing Info.plist",
                bundlePath.c_str());
            continue;
        }

        drivers.push_back(bundlePath);
    }

    return drivers;
}

std::optional<std::string> DriverLoader::FindByIdentifier(
    const std::string& identifier) const
{
    auto drivers = ListBundles();

    for (const auto& bundlePath : drivers) {
        auto info = ReadBundleInfo(bundlePath);
        if (info && info->BundleIdentifier == identifier) {
            return bundlePath;
        }
    }

    return std::nullopt;
}

std::optional<std::string> DriverLoader::FindByName(const std::string& name) const
{
    auto drivers = ListBundles();

    for (const auto& bundlePath : drivers) {
        auto info = ReadBundleInfo(bundlePath);
        if (info && info->BundleName == name) {
            return bundlePath;
        }
    }

    return std::nullopt;
}

std::optional<std::string> DriverLoader::FindByUUID(const std::string& uuid) const
{
    auto drivers = ListBundles();

    for (const auto& bundlePath : drivers) {
        auto info = ReadBundleInfo(bundlePath);
        if (info) {
            for (const auto& factory : info->PlugInFactories) {
                if (factory == uuid) {
                    return bundlePath;
                }
            }
        }
    }

    return std::nullopt;
}

std::optional<DriverBundleInfo> DriverLoader::ReadBundleInfo(
    const std::string& bundlePath) const
{
    DriverBundleInfo info;
    info.BundlePath = bundlePath;
    info.PlistPath = bundlePath + "/Contents/Info.plist";

    PlistParser parser;
    if (!parser.Open(info.PlistPath)) {
        return std::nullopt;
    }

    info.BundleIdentifier = MakeStringField(parser, "CFBundleIdentifier");
    info.BundleName = MakeStringField(parser, "CFBundleName");
    info.BundleExecutable = MakeStringField(parser, "CFBundleExecutable");
    info.BundleVersion = MakeStringField(parser, "CFBundleVersion");
    info.BundleShortVersion = MakeStringField(parser, "CFBundleShortVersionString");
    info.BundleDevelopmentRegion = MakeStringField(parser, "CFBundleDevelopmentRegion");
    info.BundlePackageType = MakeStringField(parser, "CFBundlePackageType");
    info.Copyright = MakeStringField(parser, "NSHumanReadableCopyright");

    info.MachServices = MakeMachServicesField(parser);
    info.PlugInFactories = MakeFactoriesField(parser);
    info.PlugInEntrypoints = MakeEntrypointsField(parser);

    info.SharedLibPath = MakeSharedLibField(bundlePath, info.BundleExecutable);

    return info;
}

bool DriverLoader::ValidateBundleInfo(const DriverBundleInfo& info) const
{
    // BundlePath
    if (!IsDir(info.BundlePath)) {
        tracer_->Message(
            "DriverLoader::ValidateBundleInfo()"
            " invalid bundle: bundle directory not found: \"%s\"",
            info.BundlePath.c_str());
    }

    if (!HasSuffix(info.BundlePath, ".driver")) {
        tracer_->Message(
            "DriverLoader::ValidateBundleInfo()"
            " invalid bundle: bundle directory does not end with \".driver\": \"%s\"",
            info.BundlePath.c_str());
    }

    // PlistPath
    if (!IsFile(info.PlistPath)) {
        tracer_->Message(
            "DriverLoader::ValidateBundleInfo()"
            " invalid bundle: missing Info.plist: \"%s\"",
            info.PlistPath.c_str());
        return false;
    }

    // BundleIdentifier
    if (info.BundleIdentifier.empty()) {
        tracer_->Message(
            "DriverLoader::ValidateBundleInfo()"
            " invalid bundle: CFBundleIdentifier is empty");
    }

    // BundleName
    if (info.BundleName.empty()) {
        tracer_->Message(
            "DriverLoader::ValidateBundleInfo()"
            " invalid bundle: CFBundleName is empty");
    }

    // BundleExecutable
    if (info.BundleExecutable.empty()) {
        tracer_->Message(
            "DriverLoader::ValidateBundleInfo()"
            " invalid bundle: CFBundleExecutable is empty");
        return false;
    }

    if (!IsFile(info.SharedLibPath)) {
        tracer_->Message(
            "DriverLoader::ValidateBundleInfo()"
            " invalid bundle: CFBundleExecutable does not name a file: \"%s\"",
            info.SharedLibPath.c_str());
        return false;
    }

    // BundlePackageType
    if (!info.BundlePackageType.empty() && info.BundlePackageType != "BNDL") {
        tracer_->Message(
            "invalid bundle: CFBundlePackageType is not empty and not \"BNDL\": \"%s\"",
            info.BundlePackageType.c_str());
    }

    return true;
}

std::optional<std::string> DriverLoader::ResolveEntryPoint(const DriverBundleInfo& info,
    const std::string& explicitFactoryUUID) const
{
    std::string factoryUUID = explicitFactoryUUID;

    if (factoryUUID.empty()) {
        // by default use first factory UUID
        if (info.PlugInFactories.empty()) {
            tracer_->Message(
                "DriverLoader::ResolveEntryPoint()"
                " CFPlugInFactories is empty");
            return std::nullopt;
        }
        factoryUUID = info.PlugInFactories[0];
    }

    // factoryUUID should be in CFPlugInFactories
    auto it =
        std::find(info.PlugInFactories.begin(), info.PlugInFactories.end(), factoryUUID);

    if (it == info.PlugInFactories.end()) {
        tracer_->Message(
            "DriverLoader::ResolveEntryPoint()"
            " factoryUUID not found in CFPlugInFactories: \"%s\"",
            factoryUUID.c_str());
        return std::nullopt;
    }

    // find entry point by factoryUUID
    auto entryPointIt = info.PlugInEntrypoints.find(factoryUUID);
    if (entryPointIt == info.PlugInEntrypoints.end()) {
        tracer_->Message(
            "DriverLoader::ResolveEntryPoint()"
            " factory entry point not found in CFPlugInFactories: factoryUUID=\"%s\"",
            factoryUUID.c_str());
        return std::nullopt;
    }

    auto entryPoint = entryPointIt->second;
    if (entryPoint.empty()) {
        tracer_->Message(
            "DriverLoader::ResolveEntryPoint()"
            " factory entry point is empty in CFPlugInFactories: factoryUUID=\"%s\"",
            factoryUUID.c_str());
        return std::nullopt;
    }

    return entryPoint;
}

std::shared_ptr<DriverLoader::Handle> DriverLoader::LoadDriver(
    const std::string& bundlePath,
    const std::string& factoryUUID)
{
    Tracer::Operation op;
    op.Name = "DriverLoader::LoadDriver()";

    tracer_->OperationBegin(op);

    OSStatus status = kAudioHardwareNoError;
    std::shared_ptr<Handle> result;

    std::optional<DriverBundleInfo> bundleInfo;
    std::string libPath;
    std::optional<std::string> entryPoint;
    void* libHandle = nullptr;
    void* entryPointResult = nullptr;
    AudioServerPlugInDriverRef driverRef = nullptr;
    std::shared_ptr<Handle> driverHandle;

    bundleInfo = ReadBundleInfo(bundlePath);
    if (!bundleInfo) {
        tracer_->Message("can't read bundle info: path=\"%s\"", bundlePath.c_str());
        status = kAudioHardwareUnspecifiedError;
        goto end;
    }

    tracer_->Message(
        "bundle info: path=\"%s\""
        " CFBundleIdentifier=\"%s\" CFBundleName=\"%s\""
        " CFBundleExecutable=\"%s\" CFBundleVersion=\"%s\"",
        bundlePath.c_str(),
        bundleInfo->BundleIdentifier.c_str(),
        bundleInfo->BundleName.c_str(),
        bundleInfo->BundleExecutable.c_str(),
        bundleInfo->BundleVersion.c_str());

    if (!ValidateBundleInfo(*bundleInfo)) {
        status = kAudioHardwareUnspecifiedError;
        goto end;
    }

    libPath = std::move(bundleInfo->SharedLibPath);

    entryPoint = ResolveEntryPoint(*bundleInfo, factoryUUID);
    if (!entryPoint) {
        status = kAudioHardwareUnspecifiedError;
        goto end;
    }

    libHandle = OpenLibrary(libPath);
    if (!libHandle) {
        status = kAudioHardwareUnspecifiedError;
        goto end;
    }

    entryPointResult = InvokeEntryPoint(libHandle, *entryPoint);
    if (!entryPointResult) {
        status = kAudioHardwareUnspecifiedError;
        goto end;
    }

    driverRef = OpenDriver(entryPointResult);
    if (!driverRef) {
        status = kAudioHardwareUnspecifiedError;
        goto end;
    }

    tracer_->Message("successfully loaded driver: driverRef=%p libPath=\"%s\"",
        driverRef,
        libPath.c_str());

    driverHandle.reset(new Handle);

    driverHandle->tracer = tracer_;
    driverHandle->libPath = libPath;
    driverHandle->libHandle = libHandle;
    driverHandle->driverRef = driverRef;

    // ownership goes to Handle
    libHandle = nullptr;
    driverRef = nullptr;

end:
    if (driverRef) {
        CloseDriver(tracer_, driverRef);
    }
    if (libHandle) {
        CloseLibrary(tracer_, libPath, libHandle);
    }

    tracer_->OperationEnd(op, status);

    return driverHandle;
}

DriverLoader::Handle::~Handle()
{
    Tracer::Operation op;
    op.Name = "DriverLoader::Handle::~Handle()";

    tracer->OperationBegin(op);

    if (driverRef) {
        DriverLoader::CloseDriver(tracer, driverRef);
    }
    DriverLoader::CloseLibrary(tracer, libPath, libHandle);

    tracer->OperationEnd(op, kAudioHardwareNoError);
}

AudioServerPlugInDriverRef DriverLoader::Handle::GetReference()
{
    return driverRef;
}

void DriverLoader::Handle::Disown()
{
    driverRef = nullptr;
}

void* DriverLoader::OpenLibrary(const std::string& libPath)
{
    tracer_->Message("loading library: libPath=\"%s\"", libPath.c_str());

    void* libHandle = dlopen(libPath.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_FIRST);
    if (!libHandle) {
        tracer_->Message("dlopen() failed: %s", dlerror());
    }

    return libHandle;
}

void DriverLoader::CloseLibrary(const std::shared_ptr<Tracer>& tracer,
    const std::string& libPath,
    void* libHandle)
{
    tracer->Message("unloading library: libPath=\"%s\"", libPath.c_str());

    if (dlclose(libHandle) != 0) {
        tracer->Message("dlclose() failed: %s", dlerror());
    }
}

AudioServerPlugInDriverRef DriverLoader::OpenDriver(void* entryPointResult)
{
    if (params_.EnableQueryInterface) {
        IUnknownVTbl** unknownRef = reinterpret_cast<IUnknownVTbl**>(entryPointResult);
        if ((*unknownRef)->QueryInterface == nullptr) {
            tracer_->Message("vtbl::QueryInterface is null");
            return nullptr;
        }

        tracer_->Message(
            "invoking QueryInterface(kAudioServerPlugInDriverInterfaceUUID)");

        LPVOID driverInterface;
        CFUUIDBytes interfaceUUID =
            CFUUIDGetUUIDBytes(kAudioServerPlugInDriverInterfaceUUID);
        HRESULT hr =
            (*unknownRef)->QueryInterface(unknownRef, interfaceUUID, &driverInterface);
        if (hr != S_OK) {
            tracer_->Message("QueryInterface() failed: %s", HresultToString(hr).c_str());
            return nullptr;
        }

        // QueryInterface() already called AddRef
        return reinterpret_cast<AudioServerPlugInDriverRef>(driverInterface);
    } else {
        AudioServerPlugInDriverRef driverRef =
            reinterpret_cast<AudioServerPlugInDriverRef>(entryPointResult);

        if ((*driverRef)->AddRef == nullptr) {
            tracer_->Message("vtbl::AddRef is null");
            return nullptr;
        }

        ULONG refCount = (*driverRef)->AddRef(driverRef);
        tracer_->Message("AddRef: refCount=%lu", static_cast<unsigned long>(refCount));

        return driverRef;
    }
}

void DriverLoader::CloseDriver(const std::shared_ptr<Tracer>& tracer,
    AudioServerPlugInDriverRef driverRef)
{
    if (driverRef == nullptr) {
        tracer->Message("vtbl is null");
        return;
    }

    if ((*driverRef)->Release == nullptr) {
        tracer->Message("vtbl::Release is null");
        return;
    }

    ULONG refCount = (*driverRef)->Release(driverRef);

    tracer->Message("Release: driverRef=%p refCount=%lu",
        driverRef,
        static_cast<unsigned long>(refCount));
}

void* DriverLoader::InvokeEntryPoint(void* libHandle, const std::string& entryPoint)
{
    using EntryPointFunc = void* (*)(CFAllocatorRef, CFUUIDRef);

    EntryPointFunc entryPointFunc =
        reinterpret_cast<EntryPointFunc>(dlsym(libHandle, entryPoint.c_str()));

    if (!entryPointFunc) {
        tracer_->Message(
            "dlsym() failed for entry point \"%s\": %s", entryPoint.c_str(), dlerror());
        return nullptr;
    }

    tracer_->Message("invoking %s(kCFAllocatorDefault, kAudioServerPlugInTypeUUID)",
        entryPoint.c_str());

    void* result = entryPointFunc(kCFAllocatorDefault, kAudioServerPlugInTypeUUID);
    if (!result) {
        tracer_->Message("entry point returned null");
        return nullptr;
    }

    return result;
}

} // namespace aspl
