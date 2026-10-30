# Effects

The pieces effects are made of. For pictures, `effects/image.h`: blurs, noise, tables, colour changes, vignettes and
mixing one picture over another with a blend mode. For sounds, `effects/sound.h`: gain, filters, an equaliser, echo,
bitcrush, mono and crackle. Namespace `olagarro::effects`, CMake target `olagarro::Effects`, in the build tree and from
an installed package alike. It depends on [Concurrency](concurrency.md) and nothing else: no OUI, no files, no scripts.
A program chains the pieces into its effects and binds them to its scripts itself.

- [The image](#the-image)
- [Threads](#threads)
- [The pieces](#the-pieces)
- [Blend modes](#blend-modes)
- [The sound](#the-sound)
- [Tails](#tails)
- [The sound pieces](#the-sound-pieces)

## The image

```cpp
#include <effects/image.h>

olagarro::effects::Image frame{1920, 1080};          // see-through black
float* pixel{frame.at(10, 20)};                      // red, green, blue, alpha
olagarro::effects::Image given{2, 1, {1, 0, 0, 1,  0, 0, 0.5f, 0.5f}};
```

`Image` is `{int width, height; std::vector<float> pixels;}`: four floats a pixel, row after row from the top. Colour
is **linear** light and **multiplied by alpha** (premultiplied), each channel from 0 to 1. A program whose pixels are
bytes in the screen's curve converts them on the way in and out; one that renders in linear light already, as a video
editor does, hands its buffers over as they are. `at(x, y)` points at a pixel; `clamped(x, y)` gives the nearest edge
pixel past the edges, as the blurs read them.

The pieces work on straight colour where a colour is changed (tables, hue, tints, mixing, `convolve` and so
`box_blur`, which keep alpha) and on the premultiplied values where pixels and their alpha are averaged (the gaussian,
motion, radial and zoom blurs), so a blur over a see-through edge leaves no dark fringe. What they give is kept between
0 and 1.

## Threads

Every piece takes a `concurrency::TaskPool&` first, as `concurrent_for` does, and shares the rows of its result among
that pool's workers and the calling thread; images under 64 rows are done on the calling thread alone. Each row
depends only on the image given, never on the order rows are done in, so the result is the same however many threads
there are: noise takes its numbers from the pixel's place and the seed, not from a sequence.

```cpp
auto& pool{olagarro::concurrency::TaskPool::shared()};
const auto soft{olagarro::effects::gaussian_blur(pool, frame, 4.0)};
const auto glow{olagarro::effects::mix(pool, frame, soft, olagarro::effects::BlendMode::screen, 0.5)};
```

A piece called from a task already on the pool is safe: the caller takes shares of the work itself, so it never waits
on work nobody can start (see [How the work is divided](concurrency.md#how-the-work-is-divided)).

## The pieces

Each takes an image and gives a new one; none changes what it is given.

| Piece | What it does |
|-------|--------------|
| `gaussian_blur(pool, image, radius)` | A gaussian whose radius is two sigmas, across then down. 0 changes nothing. Past a radius of 12, three box blurs of the same spread, as fast whatever the radius. The light in the picture is kept. |
| `box_blur(pool, image, radius)` | Each colour the average of the square `radius` pixels each side; alpha kept. |
| `motion_blur(pool, image, degrees, distance)` | Averaged along a line `distance` pixels long through each pixel, at `degrees`: 0 to the right, 90 up. |
| `radial_blur(pool, image, degrees, center_x, center_y)` | Averaged along an arc of `degrees` round a centre, in pixels: a spin. |
| `zoom_blur(pool, image, amount, center_x, center_y)` | Averaged along the line from each pixel towards a centre, `amount` from 0 to 1 of the way. |
| `convolve(pool, image, matrix, size, divisor, offset)` | Each colour the sum of the neighbours' times a square matrix, `size` a side, divided and offset: sharpening, edges, emboss. Alpha kept. |
| `add_noise(pool, image, intensity, saturation, coverage, seed)` | `coverage` of the pixels lighter or darker by up to `intensity`, the channels apart by `saturation` of that. The same seed gives the same noise. |
| `lookup(pool, image, tables)` | Each of red, green and blue through a table of its own, spread over 0 to 1 and interpolated: curves, levels, invert, posterize. An empty table leaves its channel. |
| `lookup_luminosity(pool, image, table)` | The luminance through a table, the colour scaled with it so its hue stays; past white goes towards white. Luminance is linear light's: 0.2126 red, 0.7152 green, 0.0722 blue. |
| `hue_saturation(pool, image, hue, saturation, lightness)` | Hue turned by degrees; saturation scaled by 1 plus `saturation`; towards white or black by `lightness`, from -1 to 1. |
| `mix(pool, below, above, mode, opacity)` | `above` over `below` by a [blend mode](#blend-modes), seen through by `opacity`. |
| `vignette(pool, image, amount, softness)` | The edges darkened by `amount` outside the ellipse touching the sides, fading in towards the middle over `softness` of the way (0 a hard edge, 1 from the middle). |
| `tint(pool, image, matrix)` | Each colour through three rows of four numbers, red, green, blue and a constant: sepia, black and white, channel mixing. |
| `channel_range(pool, image)` | Not an image: the darkest and lightest of each colour channel among the pixels not wholly see-through, for auto levels. |

Sepia and black and white as tints:

```cpp
const std::array<double, 12> black_and_white{0.2126, 0.7152, 0.0722, 0,  0.2126, 0.7152, 0.0722, 0,
                                             0.2126, 0.7152, 0.0722, 0};
const std::array<double, 12> sepia{0.393, 0.769, 0.189, 0,  0.349, 0.686, 0.168, 0,  0.272, 0.534, 0.131, 0};
```

## Blend modes

`mix` lays `above` over `below` as the W3C's compositing does: where `below` is see-through `above` shows as it is,
where it is solid each colour channel is mixed by the mode, and `opacity` scales `above`'s alpha. The result has
`below`'s size; where `above` does not reach, `below` stays. The modes are OUI Paint's, which are Paint.NET's:
`normal`, `multiply`, `additive`, `color_burn`, `color_dodge`, `reflect`, `glow`, `overlay`, `difference`, `negation`,
`lighten`, `darken`, `screen` and `xor_`, the last on the channels' bits at 16 bits.

## The sound

```cpp
#include <effects/sound.h>

olagarro::effects::Sound block{48000, std::size_t{1024}};    // 1024 frames of silence
olagarro::effects::Sound given{48000, {0.5f, -0.5f,  0.25f, 0.25f}};
```

`Sound` is `{int rate; std::vector<float> samples;}`: two channels, left and right, interleaved, `rate` frames a
second, full scale from -1 to 1. Louder is kept and not clipped: a mixer sums sounds and limits them at its end.
`frames()` is half the samples. The sound pieces take no task pool: a filter goes along its sound one frame after
another, and a mixer runs them for many clips at once on its own tasks.

## Tails

Each sound piece takes a stretch and gives a new one as long, and starts from silence: it does not see what came before
the stretch. A filter rings and an echo repeats, so what comes out at a frame depends on what came in a while before.
That while is the piece's **tail**: `<piece>_tail_seconds(rate, parameters...)`, the piece's own parameters with the
sound's rate in place of the sound. Given its tail of what comes before a stretch, and those first frames dropped from
what it gives, a piece gives the stretch as it gives it from the whole sound, to within a millionth. That is how a mixer
renders a block of a long sound exactly wherever it cuts it, without keeping a filter's state between blocks:

```cpp
using namespace olagarro::effects;

// The frames from `from` to `to` of a long sound, low passed, as the whole would have them.
const auto tail{static_cast<std::size_t>(std::ceil(low_pass_tail_seconds(rate, 1000.0, 0.707) * rate))};
const auto earlier{from - std::min(from, tail)};
const auto run{low_pass(frames_of(whole, earlier, to), 1000.0, 0.707)};
// The block is run's frames from (from - earlier) on.
```

Gain, mono and crackle have no tail (0). A filter's is how long its slowest pole takes to die down, a few periods of its
frequency: milliseconds at 1 kHz, a few tenths of a second at 20 Hz. An echo's is its delay times the repeats still
heard, which a feedback near 1 makes long. Pieces in a row have the sum of their tails.

Crackle and bitcrush depend on where they are in the whole sound: crackle's clicks come from each frame's place and the
seed, not from a sequence, and bitcrush's holds fall on whole multiples of its rate from the start. Both take `start`,
the frame the stretch starts at in the whole sound.

## The sound pieces

| Piece | What it does | Tail |
|-------|--------------|------|
| `gain(sound, decibels)` | Louder or quieter by `decibels`: 6 doubles, -6 halves. | None |
| `low_pass(sound, frequency, q)` | Frequencies above `frequency`, in Hz, taken away at 12 dB an octave; `q` 0.707 rings not at all at the corner (Butterworth), more rings. A biquad, as the Audio EQ Cookbook's. | Its ringing |
| `high_pass(sound, frequency, q)` | Frequencies below `frequency` taken away, as `low_pass` those above. | Its ringing |
| `band_pass(sound, frequency, q)` | The band round `frequency`, `frequency / q` wide, kept, its edges 3 dB down, 12 dB an octave each side past them: two biquads in a row. A band from `low` to `high` is `frequency = sqrt(low * high)`, `q = frequency / (high - low)`; 300 to 3000 Hz is a telephone or an old radio. | Both's ringing |
| `equalise(sound, low, mid, high)` | Three bands in decibels: a shelf below 250 Hz, a bell round 1 kHz (q 0.7) and a shelf above 4 kHz. 0, 0, 0 changes nothing. | Their ringing |
| `echo(sound, delay, feedback, mix)` | Repeats added `delay` seconds apart, the first `mix` as loud as the sound and each next `feedback` of the one before (kept under 1). | The delay times the repeats heard |
| `bitcrush(sound, bits, rate, start)` | Each sample rounded to `bits` bits (fractions allowed), and held so only `rate` samples a second are heard (none held at the sound's rate or more). | One hold |
| `mono(sound)` | Both channels their average. | None |
| `crackle(sound, amount, seed, start)` | An old record: sparse clicks alike on both channels and a little hiss, as many and as loud as `amount` from 0 to 1. The same seed gives the same crackle. | None |

An old radio, from a few pieces:

```cpp
using namespace olagarro::effects;

const auto radio{crackle(bitcrush(mono(band_pass(sound, 948.7, 0.351)), 8.0, 11025.0, start), 0.3, seed, start)};
```
