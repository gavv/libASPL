#include <aspl/Device.hpp>
#include <aspl/Plugin.hpp>

#include <gtest/gtest.h>

struct ConstructionTest : ::testing::Test
{
    // Set to Mode::Stderr for debugging.
    std::shared_ptr<aspl::Tracer> tracer =
        std::make_shared<aspl::Tracer>(aspl::Tracer::Mode::Noop);

    std::shared_ptr<aspl::Context> context = std::make_shared<aspl::Context>(tracer);
};

TEST_F(ConstructionTest, Device)
{
    const auto plugin = std::make_shared<aspl::Plugin>(context);

    ASSERT_EQ(0, plugin->GetOwnedObjectIDs().size());
    ASSERT_EQ(0, plugin->GetDeviceIDs().size());
    ASSERT_EQ(0, plugin->GetDeviceCount());
    ASSERT_FALSE(plugin->GetDeviceByIndex(0));

    const auto device = std::make_shared<aspl::Device>(context);

    plugin->AddDevice(device);

    ASSERT_EQ(1, plugin->GetOwnedObjectIDs().size());
    ASSERT_EQ(1, plugin->GetDeviceIDs().size());
    ASSERT_EQ(1, plugin->GetDeviceCount());
    ASSERT_EQ(device, plugin->GetDeviceByIndex(0));
    ASSERT_EQ(device, plugin->GetDeviceByID(device->GetID()));

    plugin->RemoveDevice(device);

    ASSERT_EQ(0, plugin->GetOwnedObjectIDs().size());
    ASSERT_EQ(0, plugin->GetDeviceIDs().size());
    ASSERT_EQ(0, plugin->GetDeviceCount());
    ASSERT_FALSE(plugin->GetDeviceByIndex(0));
    ASSERT_FALSE(plugin->GetDeviceByID(device->GetID()));
}

TEST_F(ConstructionTest, StreamAndControls)
{
    const auto device = std::make_shared<aspl::Device>(context);

    const auto stream = device->AddStreamWithControlsAsync(aspl::Direction::Output);
    ASSERT_TRUE(stream);
    ASSERT_EQ(stream, device->GetStreamByIndex(aspl::Direction::Output, 0));

    const auto volumeControl =
        device->GetVolumeControlByIndex(kAudioObjectPropertyScopeOutput, 0);
    ASSERT_TRUE(volumeControl);

    const auto muteControl =
        device->GetMuteControlByIndex(kAudioObjectPropertyScopeOutput, 0);
    ASSERT_TRUE(muteControl);

    EXPECT_EQ(1, device->GetStreamIDs().size());
    EXPECT_EQ(2, device->GetControlIDs().size());

    EXPECT_EQ(3, device->GetOwnedObjectIDs().size());
    EXPECT_EQ(3, device->GetOwnedObjectIDs(kAudioObjectPropertyScopeOutput).size());

    EXPECT_EQ(1, device->GetStreamCount(aspl::Direction::Output));
    EXPECT_EQ(1, device->GetVolumeControlCount(kAudioObjectPropertyScopeOutput));
    EXPECT_EQ(1, device->GetMuteControlCount(kAudioObjectPropertyScopeOutput));

    EXPECT_EQ(stream, device->GetStreamByID(stream->GetID()));
    EXPECT_EQ(volumeControl, device->GetVolumeControlByID(volumeControl->GetID()));
    EXPECT_EQ(muteControl, device->GetMuteControlByID(muteControl->GetID()));

    device->RemoveStreamAsync(stream);
    device->RemoveVolumeControlAsync(volumeControl);
    device->RemoveMuteControlAsync(muteControl);

    EXPECT_EQ(0, device->GetStreamIDs().size());
    EXPECT_EQ(0, device->GetControlIDs().size());

    EXPECT_EQ(0, device->GetOwnedObjectIDs().size());
    EXPECT_EQ(0, device->GetOwnedObjectIDs(kAudioObjectPropertyScopeOutput).size());

    EXPECT_EQ(0, device->GetStreamCount(aspl::Direction::Output));
    EXPECT_EQ(0, device->GetVolumeControlCount(kAudioObjectPropertyScopeOutput));
    EXPECT_EQ(0, device->GetMuteControlCount(kAudioObjectPropertyScopeOutput));

    EXPECT_FALSE(device->GetStreamByIndex(aspl::Direction::Output, 0));
    EXPECT_FALSE(device->GetVolumeControlByIndex(kAudioObjectPropertyScopeOutput, 0));
    EXPECT_FALSE(device->GetMuteControlByIndex(kAudioObjectPropertyScopeOutput, 0));

    EXPECT_FALSE(device->GetStreamByID(stream->GetID()));
    EXPECT_FALSE(device->GetVolumeControlByID(volumeControl->GetID()));
    EXPECT_FALSE(device->GetMuteControlByID(muteControl->GetID()));
}

TEST_F(ConstructionTest, Stream)
{
    constexpr UInt32 SampleRate = 48000, NumChannels = 4;

    aspl::DeviceParameters devParams;
    devParams.SampleRate = SampleRate;
    devParams.ChannelCount = NumChannels;

    const auto device = std::make_shared<aspl::Device>(context, devParams);

    const auto stream1 = device->AddStreamAsync(aspl::Direction::Output);
    ASSERT_TRUE(stream1);

    const auto stream2 = device->AddStreamAsync(aspl::Direction::Input);
    ASSERT_TRUE(stream2);

    const auto stream3 = device->AddStreamAsync(aspl::Direction::Input);
    ASSERT_TRUE(stream3);

    const auto stream4 = device->AddStreamAsync(aspl::Direction::Input);
    ASSERT_TRUE(stream4);

    ASSERT_EQ(4, device->GetStreamIDs().size());

    ASSERT_EQ(4, device->GetOwnedObjectIDs().size());
    ASSERT_EQ(1, device->GetOwnedObjectIDs(kAudioObjectPropertyScopeOutput).size());
    ASSERT_EQ(3, device->GetOwnedObjectIDs(kAudioObjectPropertyScopeInput).size());

    ASSERT_EQ(1, device->GetStreamCount(aspl::Direction::Output));
    ASSERT_EQ(3, device->GetStreamCount(aspl::Direction::Input));

    {
        ASSERT_EQ(stream1, device->GetStreamByIndex(aspl::Direction::Output, 0));

        EXPECT_EQ(aspl::Direction::Output, stream1->GetDirection());
        EXPECT_EQ(1, stream1->GetStartingChannel());
        EXPECT_EQ(NumChannels, stream1->GetChannelCount());

        const auto stream1Format = stream1->GetPhysicalFormat();

        EXPECT_EQ(SampleRate, stream1Format.mSampleRate);
        EXPECT_EQ(NumChannels, stream1Format.mChannelsPerFrame);
        EXPECT_EQ(NumChannels * sizeof(UInt16), stream1Format.mBytesPerFrame);
        EXPECT_EQ(NumChannels * sizeof(UInt16), stream1Format.mBytesPerPacket);
        EXPECT_EQ(1, stream1Format.mFramesPerPacket);
    }

    {
        ASSERT_EQ(stream2, device->GetStreamByIndex(aspl::Direction::Input, 0));

        EXPECT_EQ(aspl::Direction::Input, stream2->GetDirection());
        EXPECT_EQ(1, stream2->GetStartingChannel());
        EXPECT_EQ(NumChannels, stream2->GetChannelCount());

        const auto stream2Format = stream2->GetPhysicalFormat();

        EXPECT_EQ(SampleRate, stream2Format.mSampleRate);
        EXPECT_EQ(NumChannels, stream2Format.mChannelsPerFrame);
        EXPECT_EQ(NumChannels * sizeof(UInt16), stream2Format.mBytesPerFrame);
        EXPECT_EQ(NumChannels * sizeof(UInt16), stream2Format.mBytesPerPacket);
        EXPECT_EQ(1, stream2Format.mFramesPerPacket);
    }

    {
        ASSERT_EQ(stream3, device->GetStreamByIndex(aspl::Direction::Input, 1));

        EXPECT_EQ(aspl::Direction::Input, stream3->GetDirection());
        EXPECT_EQ(NumChannels + 1, stream3->GetStartingChannel());
        EXPECT_EQ(NumChannels, stream3->GetChannelCount());

        const auto stream3Format = stream3->GetPhysicalFormat();

        EXPECT_EQ(SampleRate, stream3Format.mSampleRate);
        EXPECT_EQ(NumChannels, stream3Format.mChannelsPerFrame);
        EXPECT_EQ(NumChannels * sizeof(UInt16), stream3Format.mBytesPerFrame);
        EXPECT_EQ(NumChannels * sizeof(UInt16), stream3Format.mBytesPerPacket);
        EXPECT_EQ(1, stream3Format.mFramesPerPacket);
    }

    {
        ASSERT_EQ(stream4, device->GetStreamByIndex(aspl::Direction::Input, 2));

        EXPECT_EQ(aspl::Direction::Input, stream4->GetDirection());
        EXPECT_EQ(NumChannels + NumChannels + 1, stream4->GetStartingChannel());
        EXPECT_EQ(NumChannels, stream4->GetChannelCount());

        const auto stream4Format = stream4->GetPhysicalFormat();

        EXPECT_EQ(SampleRate, stream4Format.mSampleRate);
        EXPECT_EQ(NumChannels, stream4Format.mChannelsPerFrame);
        EXPECT_EQ(NumChannels * sizeof(UInt16), stream4Format.mBytesPerFrame);
        EXPECT_EQ(NumChannels * sizeof(UInt16), stream4Format.mBytesPerPacket);
        EXPECT_EQ(1, stream4Format.mFramesPerPacket);
    }
}

TEST_F(ConstructionTest, VolumeControl)
{
    const auto device = std::make_shared<aspl::Device>(context);

    const auto control1 = device->AddVolumeControlAsync(kAudioObjectPropertyScopeOutput);
    ASSERT_TRUE(control1);

    const auto control2 = device->AddVolumeControlAsync(kAudioObjectPropertyScopeInput);
    ASSERT_TRUE(control2);

    const auto control3 = device->AddVolumeControlAsync(kAudioObjectPropertyScopeInput);
    ASSERT_TRUE(control3);

    ASSERT_EQ(3, device->GetControlIDs().size());

    ASSERT_EQ(3, device->GetOwnedObjectIDs().size());
    ASSERT_EQ(1, device->GetOwnedObjectIDs(kAudioObjectPropertyScopeOutput).size());
    ASSERT_EQ(2, device->GetOwnedObjectIDs(kAudioObjectPropertyScopeInput).size());

    ASSERT_EQ(1, device->GetVolumeControlCount(kAudioObjectPropertyScopeOutput));
    ASSERT_EQ(2, device->GetVolumeControlCount(kAudioObjectPropertyScopeInput));

    {
        ASSERT_EQ(control1,
            device->GetVolumeControlByIndex(kAudioObjectPropertyScopeOutput, 0));
        EXPECT_EQ(kAudioObjectPropertyScopeOutput, control1->GetScope());
    }

    {
        ASSERT_EQ(
            control2, device->GetVolumeControlByIndex(kAudioObjectPropertyScopeInput, 0));
        EXPECT_EQ(kAudioObjectPropertyScopeInput, control2->GetScope());
    }

    {
        ASSERT_EQ(
            control3, device->GetVolumeControlByIndex(kAudioObjectPropertyScopeInput, 1));
        EXPECT_EQ(kAudioObjectPropertyScopeInput, control3->GetScope());
    }
}

TEST_F(ConstructionTest, MuteControl)
{
    const auto device = std::make_shared<aspl::Device>(context);

    const auto control1 = device->AddMuteControlAsync(kAudioObjectPropertyScopeOutput);
    ASSERT_TRUE(control1);

    const auto control2 = device->AddMuteControlAsync(kAudioObjectPropertyScopeInput);
    ASSERT_TRUE(control2);

    const auto control3 = device->AddMuteControlAsync(kAudioObjectPropertyScopeInput);
    ASSERT_TRUE(control3);

    ASSERT_EQ(3, device->GetControlIDs().size());

    ASSERT_EQ(3, device->GetOwnedObjectIDs().size());
    ASSERT_EQ(1, device->GetOwnedObjectIDs(kAudioObjectPropertyScopeOutput).size());
    ASSERT_EQ(2, device->GetOwnedObjectIDs(kAudioObjectPropertyScopeInput).size());

    ASSERT_EQ(1, device->GetMuteControlCount(kAudioObjectPropertyScopeOutput));
    ASSERT_EQ(2, device->GetMuteControlCount(kAudioObjectPropertyScopeInput));

    {
        ASSERT_EQ(
            control1, device->GetMuteControlByIndex(kAudioObjectPropertyScopeOutput, 0));
        EXPECT_EQ(kAudioObjectPropertyScopeOutput, control1->GetScope());
    }

    {
        ASSERT_EQ(
            control2, device->GetMuteControlByIndex(kAudioObjectPropertyScopeInput, 0));
        EXPECT_EQ(kAudioObjectPropertyScopeInput, control2->GetScope());
    }

    {
        ASSERT_EQ(
            control3, device->GetMuteControlByIndex(kAudioObjectPropertyScopeInput, 1));
        EXPECT_EQ(kAudioObjectPropertyScopeInput, control3->GetScope());
    }
}
