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

std::string Convert::FormatValue(const AudioValueRange& value)
{
    std::ostringstream ss;

    ss << "{";
    ss << "min:" << value.mMinimum << " ";
    ss << "max:" << value.mMaximum;
    ss << "}";

    return ss.str();
}

std::string Convert::FormatValue(const AudioStreamBasicDescription& value)
{
    std::ostringstream ss;

    ss << "{";
    ss << "id:" << FormatIDToString(value.mFormatID) << " ";
    ss << "flags:" << FormatFlagsToString(value.mFormatFlags) << " ";
    ss << "rate:" << value.mSampleRate << " ";
    ss << "bits:" << value.mBitsPerChannel << " ";
    ss << "chans:" << value.mChannelsPerFrame << " ";
    ss << "pkt:" << value.mFramesPerPacket;
    ss << "}";

    return ss.str();
}

std::string Convert::FormatValue(const AudioStreamRangedDescription& value)
{
    std::ostringstream ss;

    ss << "{";
    ss << "id:" << FormatIDToString(value.mFormat.mFormatID) << " ";
    ss << "flags:" << FormatFlagsToString(value.mFormat.mFormatFlags) << " ";
    ss << "rate:" << value.mFormat.mSampleRate << " ";
    ss << "bits:" << value.mFormat.mBitsPerChannel << " ";
    ss << "chans:" << value.mFormat.mChannelsPerFrame << " ";
    ss << "pkt:" << value.mFormat.mFramesPerPacket << " ";
    ss << "minRate:" << value.mSampleRateRange.mMinimum << " ";
    ss << "maxRate:" << value.mSampleRateRange.mMaximum;
    ss << "}";

    return ss.str();
}

} // namespace aspl
