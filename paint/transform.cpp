#include <paint/transform.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace olagarro::paint
{

namespace
{

// A pixel as four numbers from 0 to 1, colour multiplied by alpha, so mixing see-through pixels leaves no fringe.
using Premultiplied = std::array<double, 4>;

template <typename PixelType> [[nodiscard]] Premultiplied premultiplied(const PixelType& pixel)
{
    constexpr double full{PixelType::full};
    const auto alpha{pixel.alpha() / full};
    if constexpr(PixelType::channels == 2)
    {
        const auto gray{pixel.value[0] / full * alpha};
        return {gray, gray, gray, alpha};
    }
    else
    {
        return {pixel.value[0] / full * alpha, pixel.value[1] / full * alpha, pixel.value[2] / full * alpha, alpha};
    }
}

template <typename PixelType> [[nodiscard]] PixelType straight(const Premultiplied& value)
{
    using Channel = typename PixelType::ChannelType;
    constexpr double full{PixelType::full};
    const auto alpha{std::clamp(value[3], 0.0, 1.0)};
    const auto channel{[](double unit)
                       { return static_cast<Channel>(std::lround(std::clamp(unit, 0.0, 1.0) * full)); }};
    PixelType pixel;
    if(alpha <= 0.0)
    {
        return pixel;
    }
    if constexpr(PixelType::channels == 2)
    {
        pixel.value = {channel(value[0] / alpha), channel(alpha)};
    }
    else
    {
        pixel.value = {channel(value[0] / alpha), channel(value[1] / alpha), channel(value[2] / alpha), channel(alpha)};
    }
    return pixel;
}

// Keys' cubic, a = -0.5, as most programs' bicubic.
[[nodiscard]] double cubic(double distance)
{
    const auto x{std::abs(distance)};
    if(x <= 1.0)
    {
        return (1.5 * x - 2.5) * x * x + 1.0;
    }
    if(x < 2.0)
    {
        return ((-0.5 * x + 2.5) * x - 4.0) * x + 2.0;
    }
    return 0.0;
}

template <typename PixelType> class Sampler
{
  public:
    explicit Sampler(const Image& image) : m_image{image} {}

    [[nodiscard]] Premultiplied at(int x, int y) const
    {
        if(x < 0 || y < 0 || x >= m_image.width || y >= m_image.height)
        {
            return {};
        }
        return premultiplied(m_image.pixel<PixelType>(x, y));
    }

    // The pixel at a place, the edge's own where the place is past it: what interpolation mixes, so an edge does
    // not fade into nothing.
    [[nodiscard]] Premultiplied clamped(int x, int y) const
    {
        return premultiplied(
            m_image.pixel<PixelType>(std::clamp(x, 0, m_image.width - 1), std::clamp(y, 0, m_image.height - 1)));
    }

    // The image at a place, pixels' middles at half pixels.
    [[nodiscard]] Premultiplied sample(double x, double y, Resampling resampling) const
    {
        if(resampling == Resampling::nearest || resampling == Resampling::supersampling)
        {
            return at(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)));
        }
        const auto fx{x - 0.5};
        const auto fy{y - 0.5};
        const auto x0{static_cast<int>(std::floor(fx))};
        const auto y0{static_cast<int>(std::floor(fy))};
        const auto tx{fx - x0};
        const auto ty{fy - y0};
        Premultiplied sum{};
        if(resampling == Resampling::bilinear)
        {
            for(int j{}; j < 2; ++ j)
            {
                for(int i{}; i < 2; ++ i)
                {
                    const auto weight{(i == 0 ? 1.0 - tx : tx) * (j == 0 ? 1.0 - ty : ty)};
                    const auto pixel{clamped(x0 + i, y0 + j)};
                    for(int k{}; k < 4; ++ k)
                    {
                        sum[static_cast<std::size_t>(k)] += pixel[static_cast<std::size_t>(k)] * weight;
                    }
                }
            }
            return sum;
        }
        for(int j{-1}; j <= 2; ++ j)
        {
            for(int i{-1}; i <= 2; ++ i)
            {
                const auto weight{cubic(i - tx) * cubic(j - ty)};
                const auto pixel{clamped(x0 + i, y0 + j)};
                for(int k{}; k < 4; ++ k)
                {
                    sum[static_cast<std::size_t>(k)] += pixel[static_cast<std::size_t>(k)] * weight;
                }
            }
        }
        // A cubic can overshoot past black and white, and past the alpha it has.
        sum[3] = std::clamp(sum[3], 0.0, 1.0);
        for(int k{}; k < 3; ++ k)
        {
            sum[static_cast<std::size_t>(k)] = std::clamp(sum[static_cast<std::size_t>(k)], 0.0, sum[3]);
        }
        return sum;
    }

  private:
    const Image& m_image;
};

} // namespace

const std::vector<Resampling>& all_resamplings()
{
    static const std::vector<Resampling> resamplings{Resampling::supersampling, Resampling::bicubic,
                                                     Resampling::bilinear, Resampling::nearest};
    return resamplings;
}

std::string_view resampling_name(Resampling resampling)
{
    switch(resampling)
    {
    case Resampling::nearest:
        return "Nearest Neighbor";
    case Resampling::bilinear:
        return "Bilinear";
    case Resampling::bicubic:
        return "Bicubic";
    case Resampling::supersampling:
        break;
    }
    return "Supersampling";
}

Affine Affine::rotation(double radians)
{
    const auto cosine{std::cos(radians)};
    const auto sine{std::sin(radians)};
    return {cosine, -sine, 0, sine, cosine, 0};
}

Affine Affine::after(const Affine& first) const
{
    return {a * first.a + b * first.d, a * first.b + b * first.e, a * first.c + b * first.f + c,
            d * first.a + e * first.d, d * first.b + e * first.e, d * first.c + e * first.f + f};
}

Affine Affine::inverse() const
{
    const auto determinant{a * e - b * d};
    const auto inverse_determinant{determinant != 0.0 ? 1.0 / determinant : 0.0};
    const Affine result{e * inverse_determinant,  -b * inverse_determinant, 0,
                        -d * inverse_determinant, a * inverse_determinant,  0};
    return {result.a, result.b, -(result.a * c + result.b * f), result.d, result.e, -(result.d * c + result.e * f)};
}

Image flipped(const Image& image, bool horizontal)
{
    Image result{image.format, image.width, image.height};
    const auto pixel_bytes{bytes_per_pixel(image.format)};
    for(int y{}; y < image.height; ++ y)
    {
        for(int x{}; x < image.width; ++ x)
        {
            const auto from_x{horizontal ? image.width - 1 - x : x};
            const auto from_y{horizontal ? y : image.height - 1 - y};
            std::memcpy(&result.bytes[static_cast<std::size_t>(y) * result.row_bytes() + x * pixel_bytes],
                        &image.bytes[static_cast<std::size_t>(from_y) * image.row_bytes() + from_x * pixel_bytes],
                        pixel_bytes);
        }
    }
    return result;
}

Image rotated(const Image& image, int quarter_turns)
{
    const auto turns{((quarter_turns % 4) + 4) % 4};
    if(turns == 0)
    {
        return image;
    }
    const auto sideways{turns % 2 == 1};
    Image result{image.format, sideways ? image.height : image.width, sideways ? image.width : image.height};
    const auto pixel_bytes{bytes_per_pixel(image.format)};
    for(int y{}; y < result.height; ++ y)
    {
        for(int x{}; x < result.width; ++ x)
        {
            // Where the pixel came from, turning back.
            int from_x{};
            int from_y{};
            switch(turns)
            {
            case 1:
                from_x = y;
                from_y = image.height - 1 - x;
                break;
            case 2:
                from_x = image.width - 1 - x;
                from_y = image.height - 1 - y;
                break;
            default:
                from_x = image.width - 1 - y;
                from_y = x;
                break;
            }
            std::memcpy(&result.bytes[static_cast<std::size_t>(y) * result.row_bytes() + x * pixel_bytes],
                        &image.bytes[static_cast<std::size_t>(from_y) * image.row_bytes() + from_x * pixel_bytes],
                        pixel_bytes);
        }
    }
    return result;
}

Image transformed(const Image& source, const Affine& to_source, int width, int height, Resampling resampling)
{
    Image result{source.format, width, height};
    visit_format(
        source.format,
        [&](auto tag)
        {
            using PixelType = typename decltype(tag)::Type;
            const Sampler<PixelType> sampler{source};
            // How many of the result's pixels one of the source's is across: what an edge fades over.
            const auto source_per_pixel{std::sqrt(std::abs(to_source.a * to_source.e - to_source.b * to_source.d))};
            const auto pixels_per_source{source_per_pixel > 0.0 ? 1.0 / source_per_pixel : 1.0};
            for(int y{}; y < height; ++ y)
            {
                for(int x{}; x < width; ++ x)
                {
                    double source_x{};
                    double source_y{};
                    to_source.apply(x + 0.5, y + 0.5, source_x, source_y);
                    if(resampling == Resampling::nearest)
                    {
                        result.set_pixel(x, y, straight<PixelType>(sampler.sample(source_x, source_y, resampling)));
                        continue;
                    }
                    // How far inside the source's edge, in the result's pixels: half a pixel either side of
                    // it fades.
                    const auto inside{
                        std::min({source_x, source.width - source_x, source_y, source.height - source_y}) *
                        pixels_per_source};
                    const auto coverage{std::clamp(inside + 0.5, 0.0, 1.0)};
                    if(coverage <= 0.0)
                    {
                        continue;
                    }
                    auto value{sampler.sample(source_x, source_y, resampling)};
                    for(auto& channel : value)
                    {
                        channel *= coverage;
                    }
                    result.set_pixel(x, y, straight<PixelType>(value));
                }
            }
        });
    return result;
}

Image resized(const Image& image, int width, int height, Resampling resampling)
{
    const auto scale_x{static_cast<double>(image.width) / width};
    const auto scale_y{static_cast<double>(image.height) / height};
    if(resampling != Resampling::supersampling || (scale_x <= 1.0 && scale_y <= 1.0))
    {
        // Growing, supersampling has nothing to average: bicubic does it.
        return transformed(image, Affine::scale(scale_x, scale_y), width, height,
                           resampling == Resampling::supersampling ? Resampling::bicubic : resampling);
    }
    // Each new pixel the average of the old ones it covers, a part of a pixel counting as that part.
    Image result{image.format, width, height};
    visit_format(
        image.format,
        [&](auto tag)
        {
            using PixelType = typename decltype(tag)::Type;
            const Sampler<PixelType> sampler{image};
            for(int y{}; y < height; ++ y)
            {
                const auto top{y * scale_y};
                const auto bottom{(y + 1) * scale_y};
                for(int x{}; x < width; ++ x)
                {
                    const auto left{x * scale_x};
                    const auto right{(x + 1) * scale_x};
                    Premultiplied sum{};
                    auto total{0.0};
                    for(auto row{static_cast<int>(std::floor(top))}; row < bottom; ++ row)
                    {
                        const auto height_part{std::min<double>(row + 1, bottom) - std::max<double>(row, top)};
                        for(auto column{static_cast<int>(std::floor(left))}; column < right; ++ column)
                        {
                            const auto weight{(std::min<double>(column + 1, right) - std::max<double>(column, left)) *
                                              height_part};
                            const auto pixel{sampler.at(column, row)};
                            for(int k{}; k < 4; ++ k)
                            {
                                sum[static_cast<std::size_t>(k)] += pixel[static_cast<std::size_t>(k)] * weight;
                            }
                            total += weight;
                        }
                    }
                    for(auto& value : sum)
                    {
                        value /= total;
                    }
                    result.set_pixel(x, y, straight<PixelType>(sum));
                }
            }
        });
    return result;
}

} // namespace olagarro::paint
