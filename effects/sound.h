#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace olagarro::effects
{

// A stretch of sound to work on: two channels, left and right, interleaved, `rate` frames a second, each sample from -1
// to 1 at full scale (louder is kept, not clipped).
struct Sound
{
    int rate{48000};
    std::vector<float> samples;

    Sound() = default;
    // Silence, `frames` of them.
    Sound(int rate_given, std::size_t frames) : rate{rate_given}, samples(frames * 2u) {}
    // Samples given, left and right in turn.
    Sound(int rate_given, std::vector<float> samples_given) : rate{rate_given}, samples{std::move(samples_given)} {}

    [[nodiscard]] std::size_t frames() const { return samples.size() / 2u; }
};

// The pieces sound effects are made of. Each takes a stretch and gives a new one as long, starting from silence before
// it: what came earlier is not seen. A piece whose sound depends on what came before (a filter rings, an echo repeats)
// has a tail, `<piece>_tail_seconds` of the same parameters and the sound's rate: given that much of what comes before
// a stretch, and those first frames then dropped, the stretch comes out as it does from the whole sound, to within a
// millionth. That is how a mixer renders a block of a longer sound exactly, wherever it is cut. A piece whose sound
// depends on where it is in the whole (crackle, bitcrush) is told the place of its first frame, `start`.

// Louder or quieter by `decibels`. No tail.
[[nodiscard]] Sound gain(const Sound& sound, double decibels);
[[nodiscard]] double gain_tail_seconds(int rate, double decibels);

// Frequencies above `frequency`, in Hz, taken away, 12 dB an octave; `q` is how much it rings at the corner, 0.707 not
// at all (Butterworth). A biquad, as the Audio EQ Cookbook's.
[[nodiscard]] Sound low_pass(const Sound& sound, double frequency, double q);
[[nodiscard]] double low_pass_tail_seconds(int rate, double frequency, double q);
// Frequencies below `frequency` taken away, as low_pass does those above.
[[nodiscard]] Sound high_pass(const Sound& sound, double frequency, double q);
[[nodiscard]] double high_pass_tail_seconds(int rate, double frequency, double q);
// Frequencies round `frequency` kept and the others taken away, 12 dB an octave each side: the band is `frequency / q`
// wide, its edges (3 dB down) at `frequency` times and divided by the same number. Two biquads in a row. For a band
// from low to high, `frequency` is sqrt(low * high) and `q` is frequency / (high - low).
[[nodiscard]] Sound band_pass(const Sound& sound, double frequency, double q);
[[nodiscard]] double band_pass_tail_seconds(int rate, double frequency, double q);

// A three band equaliser, each band louder or quieter by its decibels: lows by a shelf below 250 Hz, the middle by a
// bell round 1 kHz, highs by a shelf above 4 kHz. 0, 0, 0 changes nothing.
[[nodiscard]] Sound equalise(const Sound& sound, double low, double mid, double high);
[[nodiscard]] double equalise_tail_seconds(int rate, double low, double mid, double high);

// Repeats of the sound added to it, `delay` seconds apart: the first `mix` as loud as the sound, each next `feedback`
// as loud as the one before (kept under 1). The tail is the delay times the repeats still heard.
[[nodiscard]] Sound echo(const Sound& sound, double delay, double feedback, double mix);
[[nodiscard]] double echo_tail_seconds(int rate, double delay, double feedback, double mix);

// Coarser: each sample rounded to `bits` bits (fractions allowed), and held for a while so only `rate` samples a second
// are heard, in Hz (the sound's rate or more holds nothing). The holds fall on whole multiples of the held rate from
// the start of the whole sound, so `start` is the frame the stretch starts at in it.
[[nodiscard]] Sound bitcrush(const Sound& sound, double bits, double rate, std::int64_t start);
[[nodiscard]] double bitcrush_tail_seconds(int rate, double bits, double held_rate);

// Both channels their average. No tail.
[[nodiscard]] Sound mono(const Sound& sound);
[[nodiscard]] double mono_tail_seconds(int rate);

// An old record's crackle added: sparse clicks, alike on both channels, and a little hiss, both as many and as loud as
// `amount`, from 0 to 1. The same `seed` gives the same crackle; it comes from each frame's place in the whole sound,
// `start` that of the stretch's first, and not from a sequence, so it has no tail.
[[nodiscard]] Sound crackle(const Sound& sound, double amount, std::uint32_t seed, std::int64_t start);
[[nodiscard]] double crackle_tail_seconds(int rate, double amount, std::uint32_t seed);

// `above` over `below` by `opacity`, from 0 (`below` alone) to 1 (`above` alone): how much of a changed sound replaces
// the sound it was made from, as a dry and wet mix. The result is `below`'s length; where `above` does not reach,
// `below` stays. No tail of its own: each frame is the two frames at its place.
[[nodiscard]] Sound mix(const Sound& below, const Sound& above, double opacity);
[[nodiscard]] double mix_tail_seconds(int rate, double opacity);

} // namespace olagarro::effects
