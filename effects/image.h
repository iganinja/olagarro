#pragma once

#include <concurrency/pool.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace olagarro::effects
{

// A picture to work on: four floats a pixel, red, green, blue and alpha, row after row from the top. Colour is linear
// light, not the screen's curve, and multiplied by alpha, so mixing see-through pixels leaves no fringe; each from 0 to
// 1.
struct Image
{
    int width{0};
    int height{0};
    std::vector<float> pixels;

    Image() = default;
    // See-through black.
    Image(int width_given, int height_given)
        : width{width_given}, height{height_given},
          pixels(static_cast<std::size_t>(width_given) * static_cast<std::size_t>(height_given) * 4u)
    {
    }
    // Pixels given, `width` times `height` times four of them.
    Image(int width_given, int height_given, std::vector<float> pixels_given)
        : width{width_given}, height{height_given}, pixels{std::move(pixels_given)}
    {
    }

    [[nodiscard]] float* at(int x, int y) { return &pixels[(static_cast<std::size_t>(y) * width + x) * 4u]; }
    [[nodiscard]] const float* at(int x, int y) const
    {
        return &pixels[(static_cast<std::size_t>(y) * width + x) * 4u];
    }
    // The pixel at a place, the nearest edge pixel past the edges.
    [[nodiscard]] const float* clamped(int x, int y) const;
};

// How mix lays one picture over another, as OUI Paint's layers and Paint.NET's: each colour channel of the picture
// above and the one below, from 0 to 1, give the channel shown. Xor works on the channels' bits at 16 bits.
enum class BlendMode
{
    normal,
    multiply,
    additive,
    color_burn,
    color_dodge,
    reflect,
    glow,
    overlay,
    difference,
    negation,
    lighten,
    darken,
    screen,
    xor_,
};

// The pieces effects are made of. Each takes an image and gives a new one, its rows shared out among `pool`'s workers
// and the calling thread, as concurrent_for does; the result is the same however many there are.

// Blurred by a gaussian whose radius is two sigmas; 0 changes nothing. Past a radius of 12, three box blurs of the same
// spread, as fast whatever the radius.
[[nodiscard]] Image gaussian_blur(concurrency::TaskPool& pool, const Image& image, double radius);
// The colour of each pixel the average of the square `radius` pixels round it; alpha kept.
[[nodiscard]] Image box_blur(concurrency::TaskPool& pool, const Image& image, int radius);
// Averaged along a line through each pixel, `distance` pixels long at `degrees` (0 to the right, 90 up), centred on it.
[[nodiscard]] Image motion_blur(concurrency::TaskPool& pool, const Image& image, double degrees, double distance);
// Averaged along an arc round (center_x, center_y), in pixels, `degrees` of it centred on each pixel.
[[nodiscard]] Image radial_blur(concurrency::TaskPool& pool, const Image& image, double degrees, double center_x,
                                double center_y);
// Averaged along the line from each pixel to (center_x, center_y), `amount` from 0 to 1 of the way.
[[nodiscard]] Image zoom_blur(concurrency::TaskPool& pool, const Image& image, double amount, double center_x,
                              double center_y);
// Each pixel's colour the sum of its neighbours' times the matrix's numbers, `size` a side, row after row, divided by
// `divisor` and `offset` added; alpha kept.
[[nodiscard]] Image convolve(concurrency::TaskPool& pool, const Image& image, const std::vector<double>& matrix,
                             int size, double divisor, double offset);
// Random noise over the image: `coverage` of its pixels, from 0 to 1, each lighter or darker by up to `intensity`, and
// each of its channels apart by `saturation` of that. The same `seed` gives the same noise.
[[nodiscard]] Image add_noise(concurrency::TaskPool& pool, const Image& image, double intensity, double saturation,
                              double coverage, std::uint32_t seed);
// Each colour channel through a table of values from 0 to 1, spread evenly over 0 to 1 and interpolated between them:
// curves, levels, invert, posterize. An empty table leaves its channel. Alpha kept.
[[nodiscard]] Image lookup(concurrency::TaskPool& pool, const Image& image,
                           const std::array<std::vector<double>, 3>& tables);
// Each pixel's luminance through a table, as lookup's, its colour scaled with it so its hue stays; a colour that would
// go past white goes towards it. Luminance is linear light's, 0.2126 red, 0.7152 green and 0.0722 blue.
[[nodiscard]] Image lookup_luminosity(concurrency::TaskPool& pool, const Image& image,
                                      const std::vector<double>& table);
// Hue turned by `hue` degrees, saturation scaled by 1 plus `saturation`, and the colour taken towards white by
// `lightness` above 0 or towards black below, each from -1 to 1.
[[nodiscard]] Image hue_saturation(concurrency::TaskPool& pool, const Image& image, double hue, double saturation,
                                   double lightness);
// `above` over `below` as `mode` mixes them, `above` seen through by `opacity`, as the W3C's compositing does: where
// `below` is see-through `above` shows as it is, where it is solid mixed. The result is `below`'s size; what of it
// `above` does not cover keeps `below`.
[[nodiscard]] Image mix(concurrency::TaskPool& pool, const Image& below, const Image& above, BlendMode mode,
                        double opacity);
// The edges darkened: by `amount`, from 0 to 1, outside the ellipse touching the image's sides, and from there in
// towards the middle over `softness` of the way, from 0 (a hard edge) to 1 (from the middle). Alpha kept.
[[nodiscard]] Image vignette(concurrency::TaskPool& pool, const Image& image, double amount, double softness);
// Each colour through a matrix of three rows of four, red, green, blue and a constant: red becomes
// `matrix[0] * red + matrix[1] * green + matrix[2] * blue + matrix[3]`, green the next row's, blue the last's. Sepia,
// black and white, channel mixing. Alpha kept.
[[nodiscard]] Image tint(concurrency::TaskPool& pool, const Image& image, const std::array<double, 12>& matrix);

// The darkest and the lightest of each of red, green and blue among the pixels not wholly see-through, as their colour
// is; 1 and 0 when there are none.
struct ChannelRange
{
    std::array<double, 3> low{1.0, 1.0, 1.0};
    std::array<double, 3> high{0.0, 0.0, 0.0};
};
[[nodiscard]] ChannelRange channel_range(concurrency::TaskPool& pool, const Image& image);

} // namespace olagarro::effects
