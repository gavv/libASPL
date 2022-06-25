// Copyright (c) libASPL authors
// Licensed under MIT

#include <aspl/Device.hpp>
#include <aspl/Stream.hpp>

#include "Compare.hpp"

namespace aspl {

Stream::Stream(const std::shared_ptr<const Context>& context,
    const std::shared_ptr<Device>& device,
    const StreamParameters& params)
    : Object(context, "Stream")
    , params_(params)
    , device_(device)
    , latency_(params.Latency)
    , physicalFormat_(params.Format)
    , virtualFormat_(params.Format)
{
}

bool Stream::GetIsActive() const
{
    return isActive_;
}

OSStatus Stream::SetIsActiveImpl(bool isActive)
{
    isActive_ = isActive;

    return kAudioHardwareNoError;
}

Direction Stream::GetDirection() const
{
    return params_.Direction;
}

UInt32 Stream::GetTerminalType() const
{
    return GetDirection() == Direction::Input ? kAudioStreamTerminalTypeMicrophone
                                              : kAudioStreamTerminalTypeSpeaker;
}

UInt32 Stream::GetStartingChannel() const
{
    return params_.StartingChannel;
}

UInt32 Stream::GetChannelCount() const
{
    return GetPhysicalFormat().mChannelsPerFrame;
}

Float64 Stream::GetSampleRate() const
{
    return GetPhysicalFormat().mSampleRate;
}

UInt32 Stream::GetLatency() const
{
    return latency_;
}

OSStatus Stream::SetLatencyImpl(UInt32 latency)
{
    latency_ = latency;

    return kAudioHardwareNoError;
}

AudioStreamBasicDescription Stream::GetPhysicalFormat() const
{
    return physicalFormat_.Get();
}

OSStatus Stream::CheckPhysicalFormat(const AudioStreamBasicDescription& format) const
{
    const auto availFormats = GetAvailablePhysicalFormats();

    if (availFormats.empty()) {
        return kAudioHardwareNoError;
    }

    if (availFormats.end() !=
        std::find_if(availFormats.begin(), availFormats.end(), [&format](const auto& it) {
            return it.mFormat == format;
        })) {
        return kAudioHardwareNoError;
    }

    GetContext()->Tracer->Message("format is not supported");

    return kAudioDeviceUnsupportedFormatError;
}

OSStatus Stream::SetPhysicalFormatImpl(const AudioStreamBasicDescription& format)
{
    physicalFormat_.Set(format);

    OSStatus status = kAudioHardwareNoError;

    if (auto device = device_.lock()) {
        status = device->SetSampleRateAsync(format.mSampleRate);
    }

    return status;
}

OSStatus Stream::SetPhysicalSampleRateAsync(Float64 rate)
{
    std::lock_guard<decltype(writeMutex_)> writeLock(writeMutex_);

    Tracer::Operation op;
    op.Name = "Stream::SetPhysicalSampleRateAsync()";
    op.ObjectID = GetID();

    auto format = GetPhysicalFormat();
    format.mSampleRate = rate;

    const auto status = SetPhysicalFormatAsync(format);

    GetContext()->Tracer->OperationEnd(op, status);

    return status;
}

std::vector<AudioStreamRangedDescription> Stream::GetAvailablePhysicalFormats() const
{
    if (auto formats = availPhysicalFormats_.Get()) {
        return *formats;
    }

    AudioStreamRangedDescription format = {};
    format.mFormat = GetPhysicalFormat();
    format.mSampleRateRange.mMinimum = format.mFormat.mSampleRate;
    format.mSampleRateRange.mMaximum = format.mFormat.mSampleRate;

    return {format};
}

OSStatus Stream::SetAvailablePhysicalFormatsImpl(
    const std::vector<AudioStreamRangedDescription>& formats)
{
    availPhysicalFormats_.Set(formats);

    return kAudioHardwareNoError;
}

AudioStreamBasicDescription Stream::GetVirtualFormat() const
{
    return virtualFormat_.Get();
}

OSStatus Stream::CheckVirtualFormat(const AudioStreamBasicDescription& format) const
{
    const auto availFormats = GetAvailableVirtualFormats();

    if (availFormats.empty()) {
        return kAudioHardwareNoError;
    }

    if (availFormats.end() !=
        std::find_if(availFormats.begin(), availFormats.end(), [&format](const auto& it) {
            return it.mFormat == format;
        })) {
        return kAudioHardwareNoError;
    }

    GetContext()->Tracer->Message("format is not supported");

    return kAudioDeviceUnsupportedFormatError;
}

OSStatus Stream::SetVirtualFormatImpl(const AudioStreamBasicDescription& format)
{
    virtualFormat_.Set(format);

    OSStatus status = kAudioHardwareNoError;

    if (auto device = device_.lock()) {
        status = device->SetSampleRateAsync(format.mSampleRate);
    }

    return status;
}

OSStatus Stream::SetVirtualSampleRateAsync(Float64 rate)
{
    std::lock_guard<decltype(writeMutex_)> writeLock(writeMutex_);

    Tracer::Operation op;
    op.Name = "Stream::SetVirtualSampleRateAsync()";
    op.ObjectID = GetID();

    auto format = GetVirtualFormat();
    format.mSampleRate = rate;

    const auto status = SetVirtualFormatAsync(format);

    GetContext()->Tracer->OperationEnd(op, status);

    return status;
}

std::vector<AudioStreamRangedDescription> Stream::GetAvailableVirtualFormats() const
{
    if (auto formats = availVirtualFormats_.Get()) {
        return *formats;
    }

    AudioStreamRangedDescription format = {};
    format.mFormat = GetVirtualFormat();
    format.mSampleRateRange.mMinimum = format.mFormat.mSampleRate;
    format.mSampleRateRange.mMaximum = format.mFormat.mSampleRate;

    return {format};
}

OSStatus Stream::SetAvailableVirtualFormatsImpl(
    const std::vector<AudioStreamRangedDescription>& formats)
{
    availVirtualFormats_.Set(formats);

    return kAudioHardwareNoError;
}

UInt32 Stream::ConvertFramesToBytes(UInt32 numFrames) const
{
    const auto format = GetPhysicalFormat();

    return numFrames * format.mBytesPerFrame;
}

UInt32 Stream::ConvertBytesToFrames(UInt32 numBytes) const
{
    const auto format = GetPhysicalFormat();

    if (format.mBytesPerFrame == 0) {
        return 0;
    }

    return numBytes / format.mBytesPerFrame;
}

void Stream::AttachVolumeControl(const std::shared_ptr<VolumeControl>& control)
{
    volumeControl_.Set(control);
}

void Stream::AttachMuteControl(const std::shared_ptr<MuteControl>& control)
{
    muteControl_.Set(control);
}

void Stream::ApplyProcessing(Float32* frames,
    UInt32 frameCount,
    UInt32 channelCount) const
{
    if (auto control = volumeControl_.Get()) {
        control->ApplyProcessing(frames, frameCount, channelCount);
    }

    if (auto control = muteControl_.Get()) {
        control->ApplyProcessing(frames, frameCount, channelCount);
    }
}

void Stream::RequestConfigurationChange(const std::function<void()>& func)
{
    if (auto device = device_.lock()) {
        device->RequestConfigurationChange(func);
    }
}

} // namespace aspl
