// Copyright (c) libASPL authors
// Licensed under MIT

#include "PlistParser.hpp"

#include <CoreFoundation/CoreFoundation.h>

namespace aspl {

PlistParser::PlistParser()
{
}

PlistParser::~PlistParser()
{
    Close();
}

bool PlistParser::Open(const std::string& path)
{
    CFURLRef cfUrl = nullptr;
    CFReadStreamRef cfStream = nullptr;
    CFMutableDataRef cfData = nullptr;
    CFPropertyListRef cfPlist = nullptr;

    UInt8 buffer[4096];
    CFIndex bytesRead = 0;

    bool result = false;

    cfUrl = CFURLCreateFromFileSystemRepresentation(
        nullptr, reinterpret_cast<const UInt8*>(path.c_str()), path.length(), false);
    if (!cfUrl) {
        goto end;
    }

    cfStream = CFReadStreamCreateWithFile(nullptr, cfUrl);
    if (!cfStream) {
        goto end;
    }

    if (!CFReadStreamOpen(cfStream)) {
        goto end;
    }

    cfData = CFDataCreateMutable(nullptr, 0);
    while ((bytesRead = CFReadStreamRead(cfStream, buffer, sizeof(buffer))) > 0) {
        CFDataAppendBytes(cfData, buffer, bytesRead);
    }

    CFReadStreamClose(cfStream);

    if (bytesRead < 0 || CFDataGetLength(cfData) == 0) {
        goto end;
    }

    cfPlist = CFPropertyListCreateWithData(
        nullptr, cfData, kCFPropertyListImmutable, nullptr, nullptr);
    if (!cfPlist) {
        goto end;
    }

    {
        auto rootValue = PlistValue(cfPlist);
        rootDict_ = rootValue.AsDict();
        if (!rootDict_) {
            goto end;
        }
    }

    result = true;

end:
    if (cfPlist) {
        CFRelease(cfPlist);
    }
    if (cfData) {
        CFRelease(cfData);
    }
    if (cfStream) {
        CFRelease(cfStream);
    }
    if (cfUrl) {
        CFRelease(cfUrl);
    }

    return result;
}

void PlistParser::Close()
{
    rootDict_ = std::nullopt;
}

std::optional<PlistValue> PlistParser::GetValue(const std::string& key) const
{
    if (!rootDict_) {
        return std::nullopt;
    }

    auto it = rootDict_->find(key);
    if (it == rootDict_->end()) {
        return std::nullopt;
    }

    return it->second;
}

PlistValue::PlistValue(CFTypeRef value)
    : cfValue_(value)
{
    if (cfValue_) {
        CFRetain(cfValue_);
    }
}

PlistValue::~PlistValue()
{
    if (cfValue_) {
        CFRelease(cfValue_);
    }
}

PlistValue::PlistValue(const PlistValue& other)
{
    *this = other;
}

PlistValue& PlistValue::operator=(const PlistValue& other)
{
    if (this != &other) {
        if (cfValue_) {
            CFRelease(cfValue_);
        }
        cfValue_ = other.cfValue_;
        if (cfValue_) {
            CFRetain(cfValue_);
        }
    }
    return *this;
}

PlistValue::PlistValue(PlistValue&& other) noexcept
{
    *this = std::move(other);
}

PlistValue& PlistValue::operator=(PlistValue&& other) noexcept
{
    if (this != &other) {
        if (cfValue_) {
            CFRelease(cfValue_);
        }
        cfValue_ = other.cfValue_;
        other.cfValue_ = nullptr;
    }
    return *this;
}

std::optional<std::string> PlistValue::AsString() const
{
    if (!cfValue_) {
        return std::nullopt;
    }

    if (CFGetTypeID(cfValue_) != CFStringGetTypeID()) {
        return std::nullopt;
    }

    CFStringRef cfString = static_cast<CFStringRef>(cfValue_);

    CFIndex length = CFStringGetLength(cfString);
    CFIndex maxSize =
        CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;

    std::string result(maxSize + 1, '\0');
    Boolean success =
        CFStringGetCString(cfString, &result[0], maxSize, kCFStringEncodingUTF8);

    if (!success) {
        return std::nullopt;
    }

    // Remove extra null bytes
    result.resize(strlen(result.c_str()));

    return std::move(result);
}

std::optional<SInt64> PlistValue::AsInteger() const
{
    if (!cfValue_) {
        return std::nullopt;
    }

    if (CFGetTypeID(cfValue_) != CFNumberGetTypeID()) {
        return std::nullopt;
    }

    CFNumberRef cfNumber = static_cast<CFNumberRef>(cfValue_);

    SInt64 result;
    Boolean success = CFNumberGetValue(cfNumber, kCFNumberLongLongType, &result);
    if (!success) {
        return std::nullopt;
    }

    return result;
}

std::optional<Float64> PlistValue::AsReal() const
{
    if (!cfValue_) {
        return std::nullopt;
    }

    if (CFGetTypeID(cfValue_) != CFNumberGetTypeID()) {
        return std::nullopt;
    }

    CFNumberRef cfNumber = static_cast<CFNumberRef>(cfValue_);

    Float64 result;
    Boolean success = CFNumberGetValue(cfNumber, kCFNumberDoubleType, &result);
    if (!success) {
        return std::nullopt;
    }

    return result;
}

std::optional<Boolean> PlistValue::AsBoolean() const
{
    if (!cfValue_) {
        return std::nullopt;
    }

    CFTypeID cfTypeID = CFGetTypeID(cfValue_);
    if (cfTypeID != CFBooleanGetTypeID()) {
        return std::nullopt;
    }

    CFBooleanRef cfBoolean = static_cast<CFBooleanRef>(cfValue_);
    Boolean result = CFBooleanGetValue(cfBoolean);

    return result;
}

std::optional<std::vector<PlistValue>> PlistValue::AsArray() const
{
    if (!cfValue_) {
        return std::nullopt;
    }

    if (CFGetTypeID(cfValue_) != CFArrayGetTypeID()) {
        return std::nullopt;
    }

    CFArrayRef cfArray = static_cast<CFArrayRef>(cfValue_);

    CFIndex count = CFArrayGetCount(cfArray);
    std::vector<PlistValue> result;
    result.reserve(count);

    for (CFIndex i = 0; i < count; ++i) {
        CFTypeRef cfItem = CFArrayGetValueAtIndex(cfArray, i);
        result.push_back(PlistValue(cfItem));
    }

    return std::move(result);
}

std::optional<std::map<std::string, PlistValue>> PlistValue::AsDict() const
{
    if (!cfValue_) {
        return std::nullopt;
    }

    if (CFGetTypeID(cfValue_) != CFDictionaryGetTypeID()) {
        return std::nullopt;
    }

    CFDictionaryRef cfDict = static_cast<CFDictionaryRef>(cfValue_);

    CFIndex count = CFDictionaryGetCount(cfDict);
    std::vector<CFTypeRef> cfKeys(count);
    std::vector<CFTypeRef> cfValues(count);

    CFDictionaryGetKeysAndValues(cfDict, cfKeys.data(), cfValues.data());

    std::map<std::string, PlistValue> result;

    for (CFIndex i = 0; i < count; ++i) {
        CFStringRef keyString = static_cast<CFStringRef>(cfKeys[i]);
        if (CFGetTypeID(keyString) != CFStringGetTypeID()) {
            continue; // Skip non-string keys
        }

        CFIndex length = CFStringGetLength(keyString);
        CFIndex maxSize =
            CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;

        std::string key(maxSize + 1, '\0');
        Boolean success =
            CFStringGetCString(keyString, &key[0], maxSize, kCFStringEncodingUTF8);

        if (!success) {
            continue;
        }

        // Remove extra null bytes
        key.resize(strlen(key.c_str()));

        result.insert(std::make_pair(key, PlistValue(cfValues[i])));
    }

    return std::move(result);
}

} // namespace aspl
