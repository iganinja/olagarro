# Effects

The pieces picture effects are made of: blurs, noise, tables, colour changes, vignettes and mixing one picture over
another with a blend mode. `effects/image.h` (namespace `olagarro::effects`), CMake target `olagarro::Effects`, in the
build tree and from an installed package alike. It depends on [Concurrency](concurrency.md) and nothing else: no OUI,
no files, no scripts. A program chains the pieces into its effects and binds them to its scripts itself.

- [The image](#the-image)
- [Threads](#threads)
- [The pieces](#the-pieces)
- [Blend modes](#blend-modes)

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
