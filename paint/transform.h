#pragma once

#include <paint/image.h>

#include <string_view>
#include <vector>

namespace olagarro::paint
{

// How pixels are taken between whole ones when an image is resized or turned.
enum class Resampling
{
    nearest,
    bilinear,
    bicubic,
    // Each pixel the average of all those it covers, the best for making an image smaller.
    supersampling,
};

[[nodiscard]] const std::vector<Resampling>& all_resamplings();
[[nodiscard]] std::string_view resampling_name(Resampling resampling);

// A place in an image as an affine function of a place in another: x' = a x + b y + c, y' = d x + e y + f.
struct Affine
{
    double a{1.0};
    double b{0.0};
    double c{0.0};
    double d{0.0};
    double e{1.0};
    double f{0.0};

    [[nodiscard]] static Affine translation(double x, double y) { return {1, 0, x, 0, 1, y}; }
    [[nodiscard]] static Affine scale(double x, double y) { return {x, 0, 0, 0, y, 0}; }
    [[nodiscard]] static Affine rotation(double radians);
    // This after `first`.
    [[nodiscard]] Affine after(const Affine& first) const;
    [[nodiscard]] Affine inverse() const;
    void apply(double x, double y, double& out_x, double& out_y) const
    {
        out_x = a * x + b * y + c;
        out_y = d * x + e * y + f;
    }
};

// `image` mirrored left to right, or top to bottom.
[[nodiscard]] Image flipped(const Image& image, bool horizontal);
// `image` turned by quarter turns clockwise: 1, 2 or 3.
[[nodiscard]] Image rotated(const Image& image, int quarter_turns);
// `image` at a new size.
[[nodiscard]] Image resized(const Image& image, int width, int height, Resampling resampling);
// A `width` by `height` image whose each pixel's middle is taken from `source` at where `to_source` puts it;
// see-through where that falls outside it.
[[nodiscard]] Image transformed(const Image& source, const Affine& to_source, int width, int height,
                                Resampling resampling);

} // namespace olagarro::paint
