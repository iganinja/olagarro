#include <effects/sound.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>

using namespace olagarro::effects;

namespace
{

constexpr auto pi{3.14159265358979323846};

// A sine on both channels, `seconds` long.
Sound sine(int rate, double frequency, double seconds, double amplitude = 0.5)
{
    Sound sound{rate, static_cast<std::size_t>(seconds * rate)};
    for(std::size_t frame{}; frame < sound.frames(); ++ frame)
    {
        const auto value{static_cast<float>(amplitude * std::sin(2.0 * pi * frequency * frame / rate))};
        sound.samples[frame * 2u] = value;
        sound.samples[frame * 2u + 1u] = value;
    }
    return sound;
}

// The loudness of the second half, in dB: past whatever a filter does when the sound starts.
double level(const Sound& sound)
{
    auto sum{0.0};
    const auto from{sound.samples.size() / 2u};
    for(auto index{from}; index < sound.samples.size(); ++ index)
    {
        sum += static_cast<double>(sound.samples[index]) * sound.samples[index];
    }
    return 10.0 * std::log10(sum / static_cast<double>(sound.samples.size() - from));
}

// How much louder `after` is than `before`, in dB.
double change(const Sound& before, const Sound& after)
{
    return level(after) - level(before);
}

// Low, middle and high sines and something rough, left and right apart: what every piece has to work on.
Sound music(int rate, double seconds)
{
    Sound sound{rate, static_cast<std::size_t>(seconds * rate)};
    for(std::size_t frame{}; frame < sound.frames(); ++ frame)
    {
        const auto t{static_cast<double>(frame) / rate};
        const auto rough{static_cast<double>((frame * 7919u) % 101u) / 100.0 - 0.5};
        sound.samples[frame * 2u] = static_cast<float>(0.3 * std::sin(2.0 * pi * 55.0 * t) +
                                                       0.2 * std::sin(2.0 * pi * 1200.0 * t) + 0.1 * rough);
        sound.samples[frame * 2u + 1u] = static_cast<float>(0.3 * std::sin(2.0 * pi * 220.0 * t) +
                                                            0.2 * std::sin(2.0 * pi * 7000.0 * t) - 0.1 * rough);
    }
    return sound;
}

// The frames from `from` to `to` of a sound.
Sound stretch(const Sound& sound, std::size_t from, std::size_t to)
{
    return Sound{sound.rate, std::vector<float>(sound.samples.begin() + static_cast<std::ptrdiff_t>(from * 2u),
                                                sound.samples.begin() + static_cast<std::ptrdiff_t>(to * 2u))};
}

struct Piece
{
    std::string name;
    std::function<Sound(const Sound& sound, std::int64_t start)> run;
    double tail_seconds;
};

} // namespace

TEST(EffectsSound, ALowPassKeepsLowsAndTakesAwayHighs)
{
    const auto low{sine(48000, 100.0, 0.5)};
    const auto high{sine(48000, 10000.0, 0.5)};
    EXPECT_NEAR(change(low, low_pass(low, 1000.0, 0.707)), 0.0, 0.5);
    EXPECT_LT(change(high, low_pass(high, 1000.0, 0.707)), -30.0);
}

TEST(EffectsSound, AHighPassKeepsHighsAndTakesAwayLows)
{
    const auto low{sine(48000, 100.0, 0.5)};
    const auto high{sine(48000, 10000.0, 0.5)};
    EXPECT_NEAR(change(high, high_pass(high, 1000.0, 0.707)), 0.0, 0.5);
    EXPECT_LT(change(low, high_pass(low, 1000.0, 0.707)), -30.0);
}

TEST(EffectsSound, ABandPassKeepsItsBandAndTakesAwayBothSides)
{
    // 300 to 3000 Hz, at a rate with room for a decade above it.
    const auto frequency{std::sqrt(300.0 * 3000.0)};
    const auto q{frequency / (3000.0 - 300.0)};
    const auto middle{sine(96000, 1000.0, 1.0)};
    const auto below{sine(96000, 30.0, 1.0)};
    const auto above{sine(96000, 30000.0, 1.0)};
    EXPECT_NEAR(change(middle, band_pass(middle, frequency, q)), 0.0, 0.5);
    EXPECT_LT(change(below, band_pass(below, frequency, q)), -30.0);
    EXPECT_LT(change(above, band_pass(above, frequency, q)), -30.0);
    // Its edges 3 dB down.
    const auto edge{sine(96000, 300.0, 1.0)};
    EXPECT_NEAR(change(edge, band_pass(edge, frequency, q)), -3.0, 0.5);
}

TEST(EffectsSound, GainIsInDecibels)
{
    const auto sound{music(48000, 0.1)};
    const auto louder{gain(sound, 20.0 * std::log10(2.0))};
    const auto same{gain(sound, 0.0)};
    for(std::size_t index{}; index < sound.samples.size(); ++ index)
    {
        EXPECT_FLOAT_EQ(louder.samples[index], 2.0f * sound.samples[index]);
        EXPECT_FLOAT_EQ(same.samples[index], sound.samples[index]);
    }
}

TEST(EffectsSound, AnEqualiserAtZeroChangesNothingAndEachBandMovesItsOwn)
{
    const auto sound{music(48000, 0.2)};
    const auto flat{equalise(sound, 0.0, 0.0, 0.0)};
    for(std::size_t index{}; index < sound.samples.size(); ++ index)
    {
        EXPECT_NEAR(flat.samples[index], sound.samples[index], 1e-6);
    }
    const auto low{sine(48000, 40.0, 1.0)};
    const auto mid{sine(48000, 1000.0, 1.0)};
    const auto high{sine(48000, 15000.0, 1.0)};
    EXPECT_NEAR(change(low, equalise(low, 6.0, 0.0, 0.0)), 6.0, 0.5);
    EXPECT_NEAR(change(high, equalise(high, 6.0, 0.0, 0.0)), 0.0, 0.5);
    EXPECT_NEAR(change(mid, equalise(mid, 0.0, -6.0, 0.0)), -6.0, 0.5);
    EXPECT_NEAR(change(low, equalise(low, 0.0, -6.0, 0.0)), 0.0, 0.5);
    EXPECT_NEAR(change(high, equalise(high, 0.0, 0.0, 6.0)), 6.0, 0.5);
    EXPECT_NEAR(change(low, equalise(low, 0.0, 0.0, 6.0)), 0.0, 0.5);
}

TEST(EffectsSound, AnEchosFirstRepeatIsAtItsDelay)
{
    Sound click{48000, std::size_t{2000}};
    click.samples[0] = 1.0f;
    click.samples[1] = -1.0f;
    const auto echoed{echo(click, 0.01, 0.5, 0.25)};
    for(std::size_t frame{}; frame < echoed.frames(); ++ frame)
    {
        // The click, then every 480 frames a quarter of it, halving each time.
        const auto expected{frame == 0u          ? 1.0
                            : frame % 480u == 0u ? 0.25 * std::pow(0.5, static_cast<double>(frame / 480u) - 1.0)
                                                 : 0.0};
        EXPECT_NEAR(echoed.samples[frame * 2u], expected, 1e-7) << frame;
        EXPECT_NEAR(echoed.samples[frame * 2u + 1u], -expected, 1e-7) << frame;
    }
}

TEST(EffectsSound, MonoGivesBothChannelsTheirAverage)
{
    const auto sound{music(48000, 0.1)};
    const auto both{mono(sound)};
    for(std::size_t frame{}; frame < sound.frames(); ++ frame)
    {
        EXPECT_EQ(both.samples[frame * 2u], both.samples[frame * 2u + 1u]);
        EXPECT_FLOAT_EQ(both.samples[frame * 2u], (sound.samples[frame * 2u] + sound.samples[frame * 2u + 1u]) / 2.0f);
    }
}

TEST(EffectsSound, BitcrushRoundsToItsBitsAndHoldsAtItsRate)
{
    const auto sound{music(48000, 0.1)};
    const auto crushed{bitcrush(sound, 3.0, 12000.0, 0)};
    for(std::size_t frame{}; frame < sound.frames(); ++ frame)
    {
        for(std::size_t channel{}; channel < 2u; ++ channel)
        {
            const auto value{crushed.samples[frame * 2u + channel]};
            // Three bits: steps of a quarter from -1 to 1.
            EXPECT_FLOAT_EQ(value * 4.0f, std::round(value * 4.0f));
            // Held over four frames, from the first of each four.
            const auto held{sound.samples[(frame / 4u * 4u) * 2u + channel]};
            EXPECT_FLOAT_EQ(value, std::round(held * 4.0f) / 4.0f);
        }
    }
}

TEST(EffectsSound, TheSameSeedGivesTheSameCrackle)
{
    const Sound silence{48000, std::size_t{48000}};
    const auto one{crackle(silence, 1.0, 7, 0)};
    const auto again{crackle(silence, 1.0, 7, 0)};
    const auto other{crackle(silence, 1.0, 8, 0)};
    EXPECT_EQ(one.samples, again.samples);
    EXPECT_NE(one.samples, other.samples);
    // Clicks stand out over the hiss, a few of them a second.
    auto clicks{0};
    for(std::size_t frame{1}; frame < one.frames(); ++ frame)
    {
        if(std::abs(one.samples[frame * 2u]) > 0.1f && std::abs(one.samples[frame * 2u - 2u]) <= 0.1f)
        {
            ++ clicks;
        }
    }
    EXPECT_GT(clicks, 3);
    EXPECT_LT(clicks, 100);
    // None at all changes nothing.
    EXPECT_EQ(crackle(silence, 0.0, 7, 0).samples, silence.samples);
}

TEST(EffectsSound, MixGoesFromTheSoundBelowToTheOneAbove)
{
    const auto dry{music(48000, 0.1)};
    const auto wet{mono(low_pass(dry, 500.0, 0.707))};
    EXPECT_EQ(mix(dry, wet, 0.0).samples, dry.samples);
    EXPECT_EQ(mix(dry, wet, 1.0).samples, wet.samples);
    // Past either end it is held there.
    EXPECT_EQ(mix(dry, wet, -1.0).samples, dry.samples);
    EXPECT_EQ(mix(dry, wet, 2.0).samples, wet.samples);
    const auto quarter{mix(dry, wet, 0.25)};
    for(std::size_t index{}; index < dry.samples.size(); ++ index)
    {
        EXPECT_NEAR(quarter.samples[index], 0.75 * dry.samples[index] + 0.25 * wet.samples[index], 1e-6);
    }
    // Where the sound above stops, the one below goes on.
    const auto short_one{stretch(wet, 0, 100)};
    const auto mixed{mix(dry, short_one, 1.0)};
    ASSERT_EQ(mixed.samples.size(), dry.samples.size());
    EXPECT_EQ(mixed.samples[198], wet.samples[198]);
    EXPECT_EQ(mixed.samples[200], dry.samples[200]);
    EXPECT_EQ(mix_tail_seconds(48000, 0.5), 0.0);
}

TEST(EffectsSound, EveryPieceCutIntoBlocksWithItsTailEqualsItWhole)
{
    const auto rate{48000};
    const auto sound{music(rate, 2.0)};
    const auto frequency{std::sqrt(300.0 * 3000.0)};
    const auto q{frequency / 2700.0};
    const std::vector<Piece> pieces{
        {"gain", [](const Sound& s, std::int64_t) { return gain(s, -6.0); }, gain_tail_seconds(rate, -6.0)},
        {"low_pass", [](const Sound& s, std::int64_t) { return low_pass(s, 1000.0, 0.707); },
         low_pass_tail_seconds(rate, 1000.0, 0.707)},
        {"low_pass at 20 Hz", [](const Sound& s, std::int64_t) { return low_pass(s, 20.0, 2.0); },
         low_pass_tail_seconds(rate, 20.0, 2.0)},
        {"high_pass", [](const Sound& s, std::int64_t) { return high_pass(s, 300.0, 0.5); },
         high_pass_tail_seconds(rate, 300.0, 0.5)},
        {"band_pass", [&](const Sound& s, std::int64_t) { return band_pass(s, frequency, q); },
         band_pass_tail_seconds(rate, frequency, q)},
        {"equalise", [](const Sound& s, std::int64_t) { return equalise(s, 6.0, -4.0, 3.0); },
         equalise_tail_seconds(rate, 6.0, -4.0, 3.0)},
        {"echo", [](const Sound& s, std::int64_t) { return echo(s, 0.05, 0.6, 0.5); },
         echo_tail_seconds(rate, 0.05, 0.6, 0.5)},
        {"bitcrush", [](const Sound& s, std::int64_t start) { return bitcrush(s, 6.0, 7000.0, start); },
         bitcrush_tail_seconds(rate, 6.0, 7000.0)},
        {"mono", [](const Sound& s, std::int64_t) { return mono(s); }, mono_tail_seconds(rate)},
        {"crackle", [](const Sound& s, std::int64_t start) { return crackle(s, 0.8, 3, start); },
         crackle_tail_seconds(rate, 0.8, 3)},
        {"mix", [](const Sound& s, std::int64_t) { return mix(s, mono(s), 0.3); }, mix_tail_seconds(rate, 0.3)},
    };
    EXPECT_EQ(gain_tail_seconds(rate, -6.0), 0.0);
    EXPECT_EQ(mono_tail_seconds(rate), 0.0);
    EXPECT_EQ(crackle_tail_seconds(rate, 0.8, 3), 0.0);
    EXPECT_GT(low_pass_tail_seconds(rate, 1000.0, 0.707), 0.0);
    EXPECT_GE(echo_tail_seconds(rate, 0.05, 0.6, 0.5), 0.05 * 20);
    // Blocks of uneven lengths, some shorter than a tail.
    const std::vector<std::size_t> lengths{1000, 4096, 77, 30000, 12345, 512};
    for(const auto& piece : pieces)
    {
        const auto whole{piece.run(sound, 0)};
        ASSERT_EQ(whole.samples.size(), sound.samples.size()) << piece.name;
        const auto tail{static_cast<std::size_t>(std::ceil(piece.tail_seconds * rate))};
        std::size_t from{};
        std::size_t block{};
        auto worst{0.0};
        while(from < sound.frames())
        {
            const auto to{std::min(sound.frames(), from + lengths[block % lengths.size()])};
            const auto earlier{from - std::min(from, tail)};
            const auto cut{piece.run(stretch(sound, earlier, to), static_cast<std::int64_t>(earlier))};
            ASSERT_EQ(cut.frames(), to - earlier) << piece.name;
            for(auto frame{from}; frame < to; ++ frame)
            {
                for(std::size_t channel{}; channel < 2u; ++ channel)
                {
                    worst =
                        std::max(worst, std::abs(static_cast<double>(cut.samples[(frame - earlier) * 2u + channel]) -
                                                 whole.samples[frame * 2u + channel]));
                }
            }
            from = to;
            ++ block;
        }
        EXPECT_LT(worst, 1e-6) << piece.name;
    }
}
