#include <effects/sound.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace olagarro::effects
{

namespace
{

constexpr auto pi{3.14159265358979323846};

// How far a filter's ringing must have died down for its tail to end: well under the millionth a stretch is kept to,
// since what is left over can be many times what is still ringing.
constexpr auto filter_left{1e-12};
// What an echo's unheard repeats may add up to, together.
constexpr auto echo_left{1e-7};

// A second order filter, its coefficients divided by a0, as the Audio EQ Cookbook gives them.
struct Biquad
{
    double b0{1.0};
    double b1{0.0};
    double b2{0.0};
    double a1{0.0};
    double a2{0.0};
};

Biquad normalised(double b0, double b1, double b2, double a0, double a1, double a2)
{
    return Biquad{b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}

// The angle a frequency turns by in a frame, kept under the sound's highest.
double angle(int rate, double frequency)
{
    return 2.0 * pi * std::clamp(frequency, 1.0, 0.49 * rate) / rate;
}

double alpha(double omega, double q)
{
    return std::sin(omega) / (2.0 * std::max(q, 0.01));
}

Biquad low_pass_filter(int rate, double frequency, double q)
{
    const auto omega{angle(rate, frequency)};
    const auto cosine{std::cos(omega)};
    const auto a{alpha(omega, q)};
    return normalised((1.0 - cosine) / 2.0, 1.0 - cosine, (1.0 - cosine) / 2.0, 1.0 + a, -2.0 * cosine, 1.0 - a);
}

Biquad high_pass_filter(int rate, double frequency, double q)
{
    const auto omega{angle(rate, frequency)};
    const auto cosine{std::cos(omega)};
    const auto a{alpha(omega, q)};
    return normalised((1.0 + cosine) / 2.0, -(1.0 + cosine), (1.0 + cosine) / 2.0, 1.0 + a, -2.0 * cosine, 1.0 - a);
}

// A band pass whose peak is 0 dB. Two of them in a row, each with this much of the band's q, are 3 dB down at the
// band's edges: each alone is then 1 / 2^(1/4) there.
constexpr auto band_q_share{0.64359425290558262}; // sqrt(sqrt(2) - 1)

Biquad band_pass_filter(int rate, double frequency, double q)
{
    const auto omega{angle(rate, frequency)};
    const auto cosine{std::cos(omega)};
    const auto a{alpha(omega, q * band_q_share)};
    return normalised(a, 0.0, -a, 1.0 + a, -2.0 * cosine, 1.0 - a);
}

// Shelves with the cookbook's slope of 1.
Biquad low_shelf(int rate, double frequency, double decibels)
{
    const auto omega{angle(rate, frequency)};
    const auto cosine{std::cos(omega)};
    const auto a{std::pow(10.0, decibels / 40.0)};
    const auto root{2.0 * std::sqrt(a) * std::sin(omega) / 2.0 * std::sqrt(2.0)};
    return normalised(a * ((a + 1.0) - (a - 1.0) * cosine + root), 2.0 * a * ((a - 1.0) - (a + 1.0) * cosine),
                      a * ((a + 1.0) - (a - 1.0) * cosine - root), (a + 1.0) + (a - 1.0) * cosine + root,
                      -2.0 * ((a - 1.0) + (a + 1.0) * cosine), (a + 1.0) + (a - 1.0) * cosine - root);
}

Biquad high_shelf(int rate, double frequency, double decibels)
{
    const auto omega{angle(rate, frequency)};
    const auto cosine{std::cos(omega)};
    const auto a{std::pow(10.0, decibels / 40.0)};
    const auto root{2.0 * std::sqrt(a) * std::sin(omega) / 2.0 * std::sqrt(2.0)};
    return normalised(a * ((a + 1.0) + (a - 1.0) * cosine + root), -2.0 * a * ((a - 1.0) + (a + 1.0) * cosine),
                      a * ((a + 1.0) + (a - 1.0) * cosine - root), (a + 1.0) - (a - 1.0) * cosine + root,
                      2.0 * ((a - 1.0) - (a + 1.0) * cosine), (a + 1.0) - (a - 1.0) * cosine - root);
}

Biquad peak(int rate, double frequency, double q, double decibels)
{
    const auto omega{angle(rate, frequency)};
    const auto cosine{std::cos(omega)};
    const auto a{std::pow(10.0, decibels / 40.0)};
    const auto al{alpha(omega, q)};
    return normalised(1.0 + al * a, -2.0 * cosine, 1.0 - al * a, 1.0 + al / a, -2.0 * cosine, 1.0 - al / a);
}

// The equaliser's bands.
constexpr auto low_band{250.0};
constexpr auto mid_band{1000.0};
constexpr auto mid_q{0.7};
constexpr auto high_band{4000.0};

std::array<Biquad, 3> equaliser(int rate, double low, double mid, double high)
{
    return {low_shelf(rate, low_band, low), peak(rate, mid_band, mid_q, mid), high_shelf(rate, high_band, high)};
}

// The sound through the filters one after the other, each channel from silence.
template <std::size_t count> Sound filtered(const Sound& sound, const std::array<Biquad, count>& filters)
{
    Sound result{sound};
    for(const auto& filter : filters)
    {
        for(std::size_t channel{}; channel < 2u; ++ channel)
        {
            double x1{};
            double x2{};
            double y1{};
            double y2{};
            for(auto index{channel}; index < result.samples.size(); index += 2u)
            {
                const auto x0{static_cast<double>(result.samples[index])};
                const auto y0{filter.b0 * x0 + filter.b1 * x1 + filter.b2 * x2 - filter.a1 * y1 - filter.a2 * y2};
                x2 = x1;
                x1 = x0;
                y2 = y1;
                y1 = y0;
                result.samples[index] = static_cast<float>(y0);
            }
        }
    }
    return result;
}

// How long a filter rings: until its slowest pole, the largest root of z² + a1 z + a2, has died down.
double ringing_seconds(int rate, const Biquad& filter)
{
    const auto discriminant{filter.a1 * filter.a1 - 4.0 * filter.a2};
    auto radius{0.0};
    if(discriminant < 0.0)
    {
        radius = std::sqrt(filter.a2);
    }
    else
    {
        const auto root{std::sqrt(discriminant)};
        radius = std::max(std::abs(-filter.a1 + root), std::abs(-filter.a1 - root)) / 2.0;
    }
    // The two frames the filter remembers, and the ringing.
    auto frames{2.0};
    if(radius > 0.0 && radius < 1.0)
    {
        frames += std::ceil(std::log(filter_left) / std::log(radius));
    }
    return frames / rate;
}

template <std::size_t count> double ringing_seconds(int rate, const std::array<Biquad, count>& filters)
{
    auto seconds{0.0};
    for(const auto& filter : filters)
    {
        seconds += ringing_seconds(rate, filter);
    }
    return seconds;
}

std::size_t echo_frames(int rate, double delay)
{
    return std::max<std::size_t>(1u, static_cast<std::size_t>(std::llround(std::max(delay, 0.0) * rate)));
}

double echo_feedback(double feedback)
{
    return std::clamp(feedback, 0.0, 0.99);
}

// A number from 0 to 1 from a frame's place, a stream (what it is for) and the seed alone.
double random(std::int64_t frame, std::uint64_t stream, std::uint32_t seed)
{
    auto state{static_cast<std::uint64_t>(frame) * 0x9e3779b97f4a7c15u ^ stream * 0xc2b2ae3d27d4eb4fu ^
               (static_cast<std::uint64_t>(seed) + 1u) * 0x165667b19e3779f9u};
    state ^= state >> 30u;
    state *= 0xbf58476d1ce4e5b9u;
    state ^= state >> 27u;
    state *= 0x94d049bb133111ebu;
    state ^= state >> 31u;
    return static_cast<double>(state >> 11u) / 9007199254740992.0;
}

} // namespace

Sound gain(const Sound& sound, double decibels)
{
    Sound result{sound};
    const auto factor{std::pow(10.0, decibels / 20.0)};
    for(auto& sample : result.samples)
    {
        sample = static_cast<float>(sample * factor);
    }
    return result;
}

double gain_tail_seconds(int, double)
{
    return 0.0;
}

Sound low_pass(const Sound& sound, double frequency, double q)
{
    return filtered(sound, std::array{low_pass_filter(sound.rate, frequency, q)});
}

double low_pass_tail_seconds(int rate, double frequency, double q)
{
    return ringing_seconds(rate, low_pass_filter(rate, frequency, q));
}

Sound high_pass(const Sound& sound, double frequency, double q)
{
    return filtered(sound, std::array{high_pass_filter(sound.rate, frequency, q)});
}

double high_pass_tail_seconds(int rate, double frequency, double q)
{
    return ringing_seconds(rate, high_pass_filter(rate, frequency, q));
}

Sound band_pass(const Sound& sound, double frequency, double q)
{
    const auto filter{band_pass_filter(sound.rate, frequency, q)};
    return filtered(sound, std::array{filter, filter});
}

double band_pass_tail_seconds(int rate, double frequency, double q)
{
    return 2.0 * ringing_seconds(rate, band_pass_filter(rate, frequency, q));
}

Sound equalise(const Sound& sound, double low, double mid, double high)
{
    return filtered(sound, equaliser(sound.rate, low, mid, high));
}

double equalise_tail_seconds(int rate, double low, double mid, double high)
{
    return ringing_seconds(rate, equaliser(rate, low, mid, high));
}

Sound echo(const Sound& sound, double delay, double feedback, double mix)
{
    Sound result{sound};
    const auto distance{echo_frames(sound.rate, delay) * 2u};
    const auto again{echo_feedback(feedback)};
    // The repeats alone, each sample those of the one `distance` before and the sound there.
    std::vector<double> repeats(sound.samples.size());
    for(auto index{distance}; index < sound.samples.size(); ++ index)
    {
        repeats[index] = sound.samples[index - distance] + again * repeats[index - distance];
        result.samples[index] = static_cast<float>(sound.samples[index] + mix * repeats[index]);
    }
    return result;
}

double echo_tail_seconds(int rate, double delay, double feedback, double mix)
{
    if(mix == 0.0)
    {
        return 0.0;
    }
    // With `heard` repeats in the tail, those after it add up to at most mix * feedback^heard / (1 - feedback).
    const auto again{echo_feedback(feedback)};
    auto heard{1};
    auto left{std::abs(mix) * again / (1.0 - again)};
    while(left > echo_left)
    {
        ++ heard;
        left *= again;
    }
    return static_cast<double>(heard * echo_frames(rate, delay)) / rate;
}

Sound bitcrush(const Sound& sound, double bits, double rate, std::int64_t start)
{
    Sound result{sound};
    const auto step{1.0 / std::pow(2.0, std::max(bits, 1.0) - 1.0)};
    const auto holds{rate > 0.0 && rate < sound.rate};
    std::int64_t hold{-1};
    std::array<double, 2> held{};
    for(std::size_t frame{}; frame < sound.frames(); ++ frame)
    {
        // The hold a frame is in, counted from the start of the whole sound; a new one takes the frame it starts at.
        const auto place{start + static_cast<std::int64_t>(frame)};
        const auto now{
            holds ? static_cast<std::int64_t>(std::floor(static_cast<long double>(place) * rate / sound.rate)) : place};
        if(frame == 0u || now != hold)
        {
            hold = now;
            held = {sound.samples[frame * 2u], sound.samples[frame * 2u + 1u]};
        }
        for(std::size_t channel{}; channel < 2u; ++ channel)
        {
            result.samples[frame * 2u + channel] = static_cast<float>(std::round(held[channel] / step) * step);
        }
    }
    return result;
}

double bitcrush_tail_seconds(int rate, double, double held_rate)
{
    if(held_rate <= 0.0 || held_rate >= rate)
    {
        return 0.0;
    }
    return std::ceil(rate / held_rate) / rate;
}

Sound mono(const Sound& sound)
{
    Sound result{sound};
    for(std::size_t frame{}; frame < sound.frames(); ++ frame)
    {
        const auto both{(sound.samples[frame * 2u] + sound.samples[frame * 2u + 1u]) / 2.0f};
        result.samples[frame * 2u] = both;
        result.samples[frame * 2u + 1u] = both;
    }
    return result;
}

double mono_tail_seconds(int)
{
    return 0.0;
}

Sound crackle(const Sound& sound, double amount, std::uint32_t seed, std::int64_t start)
{
    Sound result{sound};
    if(amount <= 0.0)
    {
        return result;
    }
    // Up to 30 clicks a second, each half a millisecond long; a frame hears the clicks that started in the half
    // millisecond before it, found from their places alone.
    const auto chance{std::min(amount, 1.0) * 30.0 / sound.rate};
    const auto length{std::max<std::int64_t>(1, sound.rate / 2000)};
    const auto hiss{0.004 * amount};
    for(std::size_t frame{}; frame < sound.frames(); ++ frame)
    {
        const auto place{start + static_cast<std::int64_t>(frame)};
        auto click{0.0};
        for(std::int64_t back{}; back < length; ++ back)
        {
            if(random(place - back, 0u, seed) < chance)
            {
                const auto loudness{(0.3 + 0.7 * random(place - back, 1u, seed)) * amount};
                const auto sign{random(place - back, 2u, seed) < 0.5 ? -1.0 : 1.0};
                click += sign * loudness * std::exp(-6.0 * static_cast<double>(back) / static_cast<double>(length));
            }
        }
        for(std::size_t channel{}; channel < 2u; ++ channel)
        {
            const auto noise{(random(place, 3u + channel, seed) * 2.0 - 1.0) * hiss};
            result.samples[frame * 2u + channel] =
                static_cast<float>(sound.samples[frame * 2u + channel] + click + noise);
        }
    }
    return result;
}

double crackle_tail_seconds(int, double, std::uint32_t)
{
    return 0.0;
}

} // namespace olagarro::effects
