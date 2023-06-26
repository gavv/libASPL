#include <aspl/Device.hpp>
#include <aspl/Stream.hpp>

#include "Compare.hpp"

#include "TestTracer.hpp"

#include <gtest/gtest.h>

namespace {

void SetupAvailNominalRates(std::shared_ptr<aspl::Device> device,
    std::vector<Float64> rates)
{
    std::vector<AudioValueRange> ranges;

    for (auto r : rates) {
        AudioValueRange rng = {
            .mMinimum = r,
            .mMaximum = r,
        };
        ranges.push_back(rng);
    }

    EXPECT_EQ(kAudioHardwareNoError, device->SetAvailableSampleRatesAsync(ranges));
}

void SetupAvailPhysicalRates(std::shared_ptr<aspl::Stream> stream,
    std::vector<Float64> rates)
{
    std::vector<AudioStreamRangedDescription> formats;

    for (auto r : rates) {
        AudioStreamRangedDescription fmt = {
            .mFormat =
                {
                    .mSampleRate = r,
                    .mFormatID = kAudioFormatLinearPCM,
                    .mFormatFlags = kAudioFormatFlagIsSignedInteger |
                                    kAudioFormatFlagsNativeEndian |
                                    kAudioFormatFlagIsPacked,
                    .mBitsPerChannel = 16,
                    .mChannelsPerFrame = 2,
                    .mBytesPerFrame = 4,
                    .mFramesPerPacket = 1,
                    .mBytesPerPacket = 4,
                },
            .mSampleRateRange =
                {
                    .mMinimum = r,
                    .mMaximum = r,
                },
        };
        formats.push_back(fmt);
    }

    EXPECT_EQ(kAudioHardwareNoError, stream->SetAvailablePhysicalFormatsAsync(formats));
}

void SetupAvailVirtualRates(std::shared_ptr<aspl::Stream> stream,
    std::vector<Float64> rates)
{
    std::vector<AudioStreamRangedDescription> formats;

    for (auto r : rates) {
        AudioStreamRangedDescription fmt = {
            .mFormat =
                {
                    .mSampleRate = r,
                    .mFormatID = kAudioFormatLinearPCM,
                    .mFormatFlags = kAudioFormatFlagIsSignedInteger |
                                    kAudioFormatFlagsNativeEndian |
                                    kAudioFormatFlagIsPacked,
                    .mBitsPerChannel = 16,
                    .mChannelsPerFrame = 2,
                    .mBytesPerFrame = 4,
                    .mFramesPerPacket = 1,
                    .mBytesPerPacket = 4,
                },
            .mSampleRateRange =
                {
                    .mMinimum = r,
                    .mMaximum = r,
                },
        };
        formats.push_back(fmt);
    }

    EXPECT_EQ(kAudioHardwareNoError, stream->SetAvailableVirtualFormatsAsync(formats));
}

void ExpectNominalRate(Float64 rate, std::shared_ptr<aspl::Device> device)
{
    EXPECT_EQ(rate, device->GetNominalSampleRate());
}

void ExpectPhysicalRate(Float64 rate, std::shared_ptr<aspl::Stream> stream)
{
    EXPECT_EQ(rate, stream->GetPhysicalFormat().mSampleRate);
    EXPECT_EQ(rate, stream->GetPhysicalFormat().mSampleRate);

    EXPECT_EQ(rate, stream->GetSampleRate());
    EXPECT_EQ(rate, stream->GetSampleRate());
}

void ExpectVirtualRate(Float64 rate, std::shared_ptr<aspl::Stream> stream)
{
    EXPECT_EQ(rate, stream->GetVirtualFormat().mSampleRate);
    EXPECT_EQ(rate, stream->GetVirtualFormat().mSampleRate);
}

} // anonymous namespace

struct OperationsTest : ::testing::Test
{
    std::shared_ptr<aspl::Tracer> tracer = std::make_shared<TestTracer>();
    std::shared_ptr<aspl::Context> context = std::make_shared<aspl::Context>(tracer);
};

TEST_F(OperationsTest, DeviceSampleRate)
{
    { // supported
        const auto device = std::make_shared<aspl::Device>(context);

        const auto stream1 = device->AddStreamWithControlsAsync(aspl::Direction::Output);
        const auto stream2 = device->AddStreamWithControlsAsync(aspl::Direction::Output);

        SetupAvailNominalRates(device, {44100, 48000});
        SetupAvailPhysicalRates(stream1, {44100, 48000});
        SetupAvailVirtualRates(stream1, {44100, 48000});
        SetupAvailPhysicalRates(stream2, {44100, 48000});
        SetupAvailVirtualRates(stream2, {44100, 48000});

        ExpectNominalRate(44100, device);
        ExpectPhysicalRate(44100, stream1);
        ExpectVirtualRate(44100, stream1);
        ExpectPhysicalRate(44100, stream2);
        ExpectVirtualRate(44100, stream2);

        EXPECT_EQ(kAudioHardwareNoError, device->SetNominalSampleRateAsync(48000));

        ExpectNominalRate(48000, device);
        ExpectPhysicalRate(44100, stream1);
        ExpectVirtualRate(44100, stream1);
        ExpectPhysicalRate(44100, stream2);
        ExpectVirtualRate(44100, stream2);
    }
    { // unsupported
        const auto device = std::make_shared<aspl::Device>(context);

        const auto stream1 = device->AddStreamWithControlsAsync(aspl::Direction::Output);
        const auto stream2 = device->AddStreamWithControlsAsync(aspl::Direction::Output);

        SetupAvailNominalRates(device, {44100});
        SetupAvailPhysicalRates(stream1, {44100, 48000});
        SetupAvailVirtualRates(stream1, {44100, 48000});
        SetupAvailPhysicalRates(stream2, {44100, 48000});
        SetupAvailVirtualRates(stream2, {44100, 48000});

        ExpectNominalRate(44100, device);
        ExpectPhysicalRate(44100, stream1);
        ExpectVirtualRate(44100, stream1);
        ExpectPhysicalRate(44100, stream2);
        ExpectVirtualRate(44100, stream2);

        EXPECT_EQ(kAudioHardwareUnsupportedOperationError,
            device->SetNominalSampleRateAsync(48000));

        ExpectNominalRate(44100, device);
        ExpectPhysicalRate(44100, stream1);
        ExpectVirtualRate(44100, stream1);
        ExpectPhysicalRate(44100, stream2);
        ExpectVirtualRate(44100, stream2);
    }
}

TEST_F(OperationsTest, StreamPhysicalFormat)
{
    { // supported
        const auto device = std::make_shared<aspl::Device>(context);

        const auto stream1 = device->AddStreamWithControlsAsync(aspl::Direction::Output);
        const auto stream2 = device->AddStreamWithControlsAsync(aspl::Direction::Output);

        SetupAvailNominalRates(device, {44100, 48000});
        SetupAvailPhysicalRates(stream1, {44100, 48000});
        SetupAvailVirtualRates(stream1, {44100, 48000});
        SetupAvailPhysicalRates(stream2, {44100, 48000});
        SetupAvailVirtualRates(stream2, {44100, 48000});

        ExpectNominalRate(44100, device);
        ExpectPhysicalRate(44100, stream1);
        ExpectVirtualRate(44100, stream1);
        ExpectPhysicalRate(44100, stream2);
        ExpectVirtualRate(44100, stream2);

        {
            auto format = stream1->GetPhysicalFormat();
            format.mSampleRate = 48000;
            EXPECT_EQ(kAudioHardwareNoError, stream1->SetPhysicalFormatAsync(format));
        }

        ExpectNominalRate(44100, device);
        ExpectPhysicalRate(48000, stream1);
        ExpectVirtualRate(44100, stream1);
        ExpectPhysicalRate(44100, stream2);
        ExpectVirtualRate(44100, stream2);
    }
    { // unsupported
        const auto device = std::make_shared<aspl::Device>(context);

        const auto stream1 = device->AddStreamWithControlsAsync(aspl::Direction::Output);
        const auto stream2 = device->AddStreamWithControlsAsync(aspl::Direction::Output);

        SetupAvailNominalRates(device, {44100, 48000});
        SetupAvailPhysicalRates(stream1, {44100});
        SetupAvailVirtualRates(stream1, {44100, 48000});
        SetupAvailPhysicalRates(stream2, {44100, 48000});
        SetupAvailVirtualRates(stream2, {44100, 48000});

        ExpectNominalRate(44100, device);
        ExpectPhysicalRate(44100, stream1);
        ExpectVirtualRate(44100, stream1);
        ExpectPhysicalRate(44100, stream2);
        ExpectVirtualRate(44100, stream2);

        {
            auto format = stream1->GetPhysicalFormat();
            format.mSampleRate = 48000;
            EXPECT_EQ(kAudioDeviceUnsupportedFormatError,
                stream1->SetPhysicalFormatAsync(format));
        }

        ExpectNominalRate(44100, device);
        ExpectPhysicalRate(44100, stream1);
        ExpectVirtualRate(44100, stream1);
        ExpectPhysicalRate(44100, stream2);
        ExpectVirtualRate(44100, stream2);
    }
}

TEST_F(OperationsTest, StreamVirtualFormat)
{
    { // supported
        const auto device = std::make_shared<aspl::Device>(context);

        const auto stream1 = device->AddStreamWithControlsAsync(aspl::Direction::Output);
        const auto stream2 = device->AddStreamWithControlsAsync(aspl::Direction::Output);

        SetupAvailNominalRates(device, {44100, 48000});
        SetupAvailPhysicalRates(stream1, {44100, 48000});
        SetupAvailVirtualRates(stream1, {44100, 48000});
        SetupAvailPhysicalRates(stream2, {44100, 48000});
        SetupAvailVirtualRates(stream2, {44100, 48000});

        ExpectNominalRate(44100, device);
        ExpectPhysicalRate(44100, stream1);
        ExpectVirtualRate(44100, stream1);
        ExpectPhysicalRate(44100, stream2);
        ExpectVirtualRate(44100, stream2);

        {
            auto format = stream1->GetVirtualFormat();
            format.mSampleRate = 48000;
            EXPECT_EQ(kAudioHardwareNoError, stream1->SetVirtualFormatAsync(format));
        }

        ExpectNominalRate(44100, device);
        ExpectPhysicalRate(44100, stream1);
        ExpectVirtualRate(48000, stream1);
        ExpectPhysicalRate(44100, stream2);
        ExpectVirtualRate(44100, stream2);
    }
    { // unsupported
        const auto device = std::make_shared<aspl::Device>(context);

        const auto stream1 = device->AddStreamWithControlsAsync(aspl::Direction::Output);
        const auto stream2 = device->AddStreamWithControlsAsync(aspl::Direction::Output);

        SetupAvailNominalRates(device, {44100, 48000});
        SetupAvailPhysicalRates(stream1, {44100, 48000});
        SetupAvailVirtualRates(stream1, {44100});
        SetupAvailPhysicalRates(stream2, {44100, 48000});
        SetupAvailVirtualRates(stream2, {44100, 48000});

        ExpectNominalRate(44100, device);
        ExpectPhysicalRate(44100, stream1);
        ExpectVirtualRate(44100, stream1);
        ExpectPhysicalRate(44100, stream2);
        ExpectVirtualRate(44100, stream2);

        {
            auto format = stream1->GetVirtualFormat();
            format.mSampleRate = 48000;
            EXPECT_EQ(kAudioDeviceUnsupportedFormatError,
                stream1->SetVirtualFormatAsync(format));
        }

        ExpectNominalRate(44100, device);
        ExpectPhysicalRate(44100, stream1);
        ExpectVirtualRate(44100, stream1);
        ExpectPhysicalRate(44100, stream2);
        ExpectVirtualRate(44100, stream2);
    }
}
