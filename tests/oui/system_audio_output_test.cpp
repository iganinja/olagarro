#include <oui/graphics/system_audio_output.h>

#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace olagarro;

TEST(OuiSystemAudioOutputTests, OpeningWithNoDeviceSaysWhy)
{
    try
    {
        const auto output{oui::system_audio_output({48000, "no_such_driver"})};
        FAIL() << "a driver SDL does not have must not open";
    }
    catch(const std::runtime_error& error)
    {
        const std::string message{error.what()};
        EXPECT_NE(message.find("Cannot play sound"), std::string::npos) << message;
        EXPECT_NE(message.find("no_such_driver"), std::string::npos) << message;
    }
}

// SDL's dummy driver plays nothing, but takes the frames in time as a device would.
TEST(OuiSystemAudioOutputTests, WhatIsQueuedIsPlayedInTime)
{
    const auto output{oui::system_audio_output({48000, "dummy"})};
    EXPECT_EQ(output->sample_rate(), 48000);
    EXPECT_FALSE(output->is_paused());
    const std::vector<float> samples(4800 * 2, 0.25f);
    output->queue(samples.data(), 4800);
    EXPECT_GT(output->queued() + output->played(), 0);

    const auto give_up{std::chrono::steady_clock::now() + std::chrono::seconds{5}};
    while(output->played() < 4800 && std::chrono::steady_clock::now() < give_up)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    EXPECT_EQ(output->played(), 4800);
    EXPECT_EQ(output->queued(), 0);
}

TEST(OuiSystemAudioOutputTests, APausedOutputKeepsWhatIsQueuedAndClearingDropsIt)
{
    const auto output{oui::system_audio_output({48000, "dummy"})};
    output->set_paused(true);
    EXPECT_TRUE(output->is_paused());
    const std::vector<float> samples(4800 * 2, 0.25f);
    output->queue(samples.data(), 4800);
    std::this_thread::sleep_for(std::chrono::milliseconds{200});
    EXPECT_EQ(output->played(), 0);
    EXPECT_EQ(output->queued(), 4800);
    output->clear();
    EXPECT_EQ(output->queued(), 0);
    EXPECT_EQ(output->played(), 0);
}
