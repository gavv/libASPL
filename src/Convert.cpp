// Copyright (c) libASPL authors
// Licensed under MIT

#include "Convert.hpp"
#include "Strings.hpp"

#include <vector>

namespace aspl {

void Convert::ToFoundation(const std::string& value, CFStringRef& result)
{
    result = CFStringCreateWithCString(
        kCFAllocatorDefault, value.c_str(), kCFStringEncodingUTF8);
}

void Convert::FromFoundation(CFStringRef value, std::string& result)
{
    if (!value) {
        result = {};
        return;
    }

    CFIndex length = CFStringGetLength(value);
    CFIndex maxSize =
        CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;

    if (maxSize <= 0) {
        result = {};
        return;
    }

    std::vector<char> buffer(size_t(maxSize) + 1);
    if (!CFStringGetCString(value, &buffer[0], maxSize, kCFStringEncodingUTF8)) {
        result = {};
        return;
    }

    result.assign(&buffer[0]);
}

void Convert::ToFoundation(const std::string& value, CFURLRef& result)
{
    result = CFURLCreateWithBytes(kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(value.c_str()),
        CFIndex(value.size()),
        kCFStringEncodingUTF8,
        nullptr);
}

void Convert::FromFoundation(CFURLRef value, std::string& result)
{
    CFStringRef stringValue = CFURLGetString(value);

    FromFoundation(stringValue, result);
}

std::string Convert::FormatToString(const AudioStreamBasicDescription& format)
{
    std::ostringstream ss;

    ss << "{";
    ss << "id:" << FormatIDToString(format.mFormatID) << " ";
    ss << "flags:" << FormatFlagsToString(format.mFormatFlags) << " ";
    ss << "rate:" << format.mSampleRate << " ";
    ss << "bits:" << format.mBitsPerChannel << " ";
    ss << "chans:" << format.mChannelsPerFrame << " ";
    ss << "pkt:" << format.mFramesPerPacket;
    ss << "}";

    return ss.str();
}

std::string Convert::FormatToString(const AudioStreamRangedDescription& format)
{
    std::ostringstream ss;

    ss << "{";
    ss << "id:" << FormatIDToString(format.mFormat.mFormatID) << " ";
    ss << "flags:" << FormatFlagsToString(format.mFormat.mFormatFlags) << " ";
    ss << "rate:" << format.mFormat.mSampleRate << " ";
    ss << "bits:" << format.mFormat.mBitsPerChannel << " ";
    ss << "chans:" << format.mFormat.mChannelsPerFrame << " ";
    ss << "pkt:" << format.mFormat.mFramesPerPacket << " ";
    ss << "minRate:" << format.mSampleRateRange.mMinimum << " ";
    ss << "maxRate:" << format.mSampleRateRange.mMaximum;
    ss << "}";

    return ss.str();
}

} // namespace aspl
