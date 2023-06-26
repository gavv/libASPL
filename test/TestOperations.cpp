#include <aspl/Device.hpp>
#include <aspl/Stream.hpp>

#include "Compare.hpp"

#include "TestTracer.hpp"

#include <gtest/gtest.h>

namespace {

template <class Vector>
void ExpectVectorsEq(const Vector& a, const Vector& b)
{
    ASSERT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); i++) {
        ASSERT_TRUE(a[i] == b[i]);
    }
}

} // anonymous namespace

struct OperationsTest : ::testing::Test
{
    std::shared_ptr<aspl::Tracer> tracer = std::make_shared<TestTracer>();
    std::shared_ptr<aspl::Context> context = std::make_shared<aspl::Context>(tracer);
};

TEST_F(OperationsTest, StreamFormat)
{
    const auto device = std::make_shared<aspl::Device>(context);

    const auto stream1 = device->AddStreamWithControlsAsync(aspl::Direction::Output);
    const auto stream2 = device->AddStreamWithControlsAsync(aspl::Direction::Output);

    ASSERT_TRUE(stream1);
    ASSERT_TRUE(stream2);

    {
        // SetAvailableSampleRatesAsync
        auto rate_list = device->GetAvailableSampleRates();
        ASSERT_EQ(1, rate_list.size());
        ASSERT_EQ(44100, rate_list[0].mMinimum);
        ASSERT_EQ(44100, rate_list[0].mMaximum);

        {
            auto rate48 = rate_list[0];

            rate48.mMinimum = 48000;
            rate48.mMaximum = 48000;

            rate_list.push_back(rate48);
        }

        EXPECT_EQ(kAudioHardwareNoError, device->SetAvailableSampleRatesAsync(rate_list));
    }

    {
        // SetAvailablePhysicalFormatsAsync
        auto format_list = stream1->GetAvailablePhysicalFormats();
        ASSERT_EQ(1, format_list.size());
        ASSERT_EQ(44100, format_list[0].mFormat.mSampleRate);

        {
            auto format48 = format_list[0];

            format48.mFormat.mSampleRate = 48000;
            format48.mSampleRateRange.mMinimum = 48000;
            format48.mSampleRateRange.mMaximum = 48000;

            format_list.push_back(format48);
        }

        EXPECT_EQ(kAudioHardwareNoError,
            stream1->SetAvailablePhysicalFormatsAsync(format_list));

        EXPECT_EQ(kAudioHardwareNoError,
            stream2->SetAvailablePhysicalFormatsAsync(format_list));

        ExpectVectorsEq(format_list, stream1->GetAvailablePhysicalFormats());
        ExpectVectorsEq(format_list, stream2->GetAvailablePhysicalFormats());
    }

    {
        // SetPhysicalFormatAsync
        auto format1 = stream1->GetPhysicalFormat();
        auto format2 = stream2->GetPhysicalFormat();

        ASSERT_EQ(44100, format1.mSampleRate);
        ASSERT_EQ(44100, format2.mSampleRate);

        ASSERT_EQ(44100, stream1->GetSampleRate());
        ASSERT_EQ(44100, stream2->GetSampleRate());

        ASSERT_EQ(44100, device->GetSampleRate());

        format1.mSampleRate = 48000;
        EXPECT_EQ(kAudioHardwareNoError, stream1->SetPhysicalFormatAsync(format1));

        ASSERT_EQ(48000, stream1->GetPhysicalFormat().mSampleRate);
        ASSERT_EQ(48000, stream2->GetPhysicalFormat().mSampleRate);

        ASSERT_EQ(48000, stream1->GetSampleRate());
        ASSERT_EQ(48000, stream2->GetSampleRate());

        ASSERT_EQ(48000, device->GetSampleRate());
    }
}
