// Copyright (c) libASPL authors
// Licensed under MIT

#include <aspl/Storage.hpp>

#include "Convert.hpp"
#include "Strings.hpp"

#include <type_traits>

namespace aspl {

Storage::Storage(std::shared_ptr<Context> context)
    : context_(context)
{
}

std::shared_ptr<const Context> Storage::GetContext() const
{
    return context_;
}

std::pair<std::vector<UInt8>, OSStatus> Storage::ReadBytes(std::string key) const
{
    return CopyFromStorage_<std::vector<UInt8>>("CFData", key);
}

std::pair<std::string, OSStatus> Storage::ReadString(std::string key) const
{
    return CopyFromStorage_<std::string>("CFString", key);
}

std::pair<bool, OSStatus> Storage::ReadBoolean(std::string key) const
{
    return CopyFromStorage_<bool>("CFBoolean", key);
}

std::pair<SInt64, OSStatus> Storage::ReadInt(std::string key) const
{
    return CopyFromStorage_<SInt64>("CFNumber", key);
}

std::pair<Float64, OSStatus> Storage::ReadFloat(std::string key) const
{
    return CopyFromStorage_<Float64>("CFNumber", key);
}

std::pair<CFPropertyListRef, OSStatus> Storage::ReadCustom(std::string key) const
{
    return CopyFromStorage_<CFPropertyListRef>("CFPropertyList", key);
}

OSStatus Storage::WriteBytes(std::string key, std::vector<UInt8> value)
{
    return WriteToStorage_("CFData", key, value);
}

OSStatus Storage::WriteString(std::string key, std::string value)
{
    return WriteToStorage_("CFString", key, value);
}

OSStatus Storage::WriteBoolean(std::string key, bool value)
{
    return WriteToStorage_("CFBoolean", key, value);
}

OSStatus Storage::WriteInt(std::string key, SInt64 value)
{
    return WriteToStorage_("CFNumber", key, value);
}

OSStatus Storage::WriteFloat(std::string key, Float64 value)
{
    return WriteToStorage_("CFNumber", key, value);
}

OSStatus Storage::WriteCustom(std::string key, CFPropertyListRef value)
{
    return WriteToStorage_("CFPropertyList", key, value);
}

OSStatus Storage::Delete(std::string key)
{
    return DeleteFromStorage_(key);
}

template <class T>
std::pair<T, OSStatus> Storage::CopyFromStorage_(const char* type, std::string key) const
{
    OSStatus status = kAudioHardwareNoError;

    AudioServerPlugInHostRef host = nullptr;
    CFStringRef keyString = nullptr;
    CFPropertyListRef valuePlist = nullptr;
    T value = {};

    Convert::ToFoundation(key, keyString);
    if (!keyString) {
        GetContext()->Tracer->Message(
            "Storage::CopyFromStorage() type=%s key=\"%s\" can't encode key",
            type,
            key.c_str());
        status = kAudioHardwareIllegalOperationError;
        goto end;
    }

    host = GetContext()->Host.load();
    if (!host) {
        GetContext()->Tracer->Message(
            "Storage::CopyFromStorage() type=%s key=\"%s\" Context.Host is NULL",
            type,
            key.c_str());
        status = kAudioHardwareNotReadyError;
        goto end;
    }

    status = host->CopyFromStorage(host, keyString, &valuePlist);

    GetContext()->Tracer->Message(
        "Storage::CopyFromStorage() type=%s key=\"%s\" status=%s",
        type,
        key.c_str(),
        StatusToString(status).c_str());

    if (status != kAudioHardwareNoError) {
        goto end;
    }

    if constexpr (!std::is_same<T, CFPropertyListRef>::value) {
        if (!Convert::FromFoundation(valuePlist, value)) {
            GetContext()->Tracer->Message(
                "Storage::CopyFromStorage() type=%s key=\"%s\" can't decode value",
                type,
                key.c_str());
            status = kAudioHardwareIllegalOperationError;
            goto end;
        }
    } else {
        value = valuePlist;
    }

end:
    if (keyString) {
        CFRelease(keyString);
    }

    if constexpr (!std::is_same<T, CFPropertyListRef>::value) {
        if (valuePlist) {
            CFRelease(valuePlist);
        }
    }

    return std::make_pair(value, status);
}

template <class T>
OSStatus Storage::WriteToStorage_(const char* type, std::string key, T value)
{
    OSStatus status = kAudioHardwareNoError;

    AudioServerPlugInHostRef host = nullptr;
    CFStringRef keyString = nullptr;
    CFPropertyListRef valuePlist = nullptr;

    Convert::ToFoundation(key, keyString);
    if (!keyString) {
        GetContext()->Tracer->Message(
            "Storage::WriteToStorage() type=%s key=\"%s\" can't encode key",
            type,
            key.c_str());
        status = kAudioHardwareIllegalOperationError;
        goto end;
    }

    if constexpr (!std::is_same<T, CFPropertyListRef>::value) {
        Convert::ToFoundation(value, valuePlist);
        if (!valuePlist) {
            GetContext()->Tracer->Message(
                "Storage::WriteToStorage() type=%s key=\"%s\" can't encode value",
                type,
                key.c_str());
            status = kAudioHardwareIllegalOperationError;
            goto end;
        }
    } else {
        valuePlist = value;
    }

    host = GetContext()->Host.load();
    if (!host) {
        GetContext()->Tracer->Message(
            "Storage::WriteToStorage() type=%s key=\"%s\" Context.Host is NULL",
            type,
            key.c_str());
        status = kAudioHardwareNotReadyError;
        goto end;
    }

    status = host->WriteToStorage(host, keyString, valuePlist);

    GetContext()->Tracer->Message(
        "Storage::WriteToStorage() type=%s key=\"%s\" status=%s",
        type,
        key.c_str(),
        StatusToString(status).c_str());

end:
    if (keyString) {
        CFRelease(keyString);
    }

    if constexpr (!std::is_same<T, CFPropertyListRef>::value) {
        if (valuePlist) {
            CFRelease(valuePlist);
        }
    }

    return status;
}

OSStatus Storage::DeleteFromStorage_(std::string key)
{
    OSStatus status = kAudioHardwareNoError;

    AudioServerPlugInHostRef host = nullptr;
    CFStringRef keyString = nullptr;

    Convert::ToFoundation(key, keyString);
    if (!keyString) {
        GetContext()->Tracer->Message(
            "Storage::DeleteFromStorage() key=\"%s\" can't encode key", key.c_str());
        status = kAudioHardwareIllegalOperationError;
        goto end;
    }

    host = GetContext()->Host.load();
    if (!host) {
        GetContext()->Tracer->Message(
            "Storage::DeleteFromStorage() key=\"%s\" Context.Host is NULL", key.c_str());
        status = kAudioHardwareNotReadyError;
        goto end;
    }

    status = host->DeleteFromStorage(host, keyString);

    GetContext()->Tracer->Message("Storage::DeleteFromStorage() key=\"%s\" status=%s",
        key.c_str(),
        StatusToString(status).c_str());

end:
    if (keyString) {
        CFRelease(keyString);
    }

    return status;
}

} // namespace aspl
