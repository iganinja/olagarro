#include <oui/audio_output.h>

#include <gtest/gtest.h>

#include <vector>

using namespace olagarro;

namespace
{

// `frames` stereo frames of silence.
std::vector<float> block(std::size_t frames)
{
    return std::vector<float>(frames * 2, 0.0f);
}

} // namespace

TEST(OuiAudioOutputTests, ASilentOutputQueuesBlocksAndPlaysThemAsItIsTold)
{
    oui::SilentAudioOutput output{44100};
    EXPECT_EQ(output.sample_rate(), 44100);
    EXPECT_EQ(output.played(), 0);
    EXPECT_EQ(output.queued(), 0);

    const auto first{block(4800)};
    const auto second{block(1200)};
    output.queue(first.data(), 4800);
    output.queue(second.data(), 1200);
    EXPECT_EQ(output.queued(), 6000);
    EXPECT_EQ(output.played(), 0);

    output.advance(1000);
    EXPECT_EQ(output.played(), 1000);
    EXPECT_EQ(output.queued(), 5000);
    output.advance(4500);
    EXPECT_EQ(output.played(), 5500);
    EXPECT_EQ(output.queued(), 500);
}

TEST(OuiAudioOutputTests, ASilentOutputPlaysNoMoreThanIsQueued)
{
    oui::SilentAudioOutput output;
    EXPECT_EQ(output.sample_rate(), 48000);
    const auto samples{block(100)};
    output.queue(samples.data(), 100);
    output.advance(250);
    EXPECT_EQ(output.played(), 100);
    EXPECT_EQ(output.queued(), 0);
    // What comes after an underrun plays from where it is, the frames it lacked not counted.
    output.queue(samples.data(), 100);
    output.advance(40);
    EXPECT_EQ(output.played(), 140);
}

TEST(OuiAudioOutputTests, APausedSilentOutputPlaysNothingAndKeepsWhatIsQueued)
{
    oui::SilentAudioOutput output;
    EXPECT_FALSE(output.is_paused());
    const auto samples{block(1000)};
    output.queue(samples.data(), 1000);
    output.set_paused(true);
    EXPECT_TRUE(output.is_paused());
    output.advance(500);
    EXPECT_EQ(output.played(), 0);
    EXPECT_EQ(output.queued(), 1000);
    output.set_paused(false);
    output.advance(500);
    EXPECT_EQ(output.played(), 500);
}

TEST(OuiAudioOutputTests, ClearingDropsWhatIsQueuedAndKeepsWhatWasPlayed)
{
    oui::SilentAudioOutput output;
    const auto samples{block(1000)};
    output.queue(samples.data(), 1000);
    output.advance(300);
    output.clear();
    EXPECT_EQ(output.played(), 300);
    EXPECT_EQ(output.queued(), 0);
    output.advance(300);
    EXPECT_EQ(output.played(), 300);
}
