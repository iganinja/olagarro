#include <effects/image.h>

#include <concurrency/concurrency.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <numeric>

namespace olagarro::effects
{

namespace
{

constexpr auto pi{3.14159265358979323846};

// Calls `row` with each of the rows from 0 to `height`, the rows shared out among `pool`'s workers and the calling
// thread: `row` must touch only its own row of what it writes.
void for_each_row(concurrency::TaskPool& pool, int height, const std::function<void(int y)>& row)
{
    // Small images are not worth the threads.
    if(height < 64)
    {
        for(int y{}; y < height; ++ y)
        {
            row(y);
        }
        return;
    }
    std::vector<int> rows(static_cast<std::size_t>(height));
    std::iota(rows.begin(), rows.end(), 0);
    concurrency::concurrent_for(pool, rows.begin(), rows.end(), [&row](std::size_t, int y) { row(y); }).get();
}

// Straight colour of a pixel, alpha apart.
void unpremultiply(const float* pixel, double& red, double& green, double& blue, double& alpha)
{
    alpha = pixel[3];
    const auto divide{alpha > 0.0 ? 1.0 / alpha : 0.0};
    red = pixel[0] * divide;
    green = pixel[1] * divide;
    blue = pixel[2] * divide;
}

void premultiply(float* pixel, double red, double green, double blue, double alpha)
{
    alpha = std::clamp(alpha, 0.0, 1.0);
    pixel[0] = static_cast<float>(std::clamp(red, 0.0, 1.0) * alpha);
    pixel[1] = static_cast<float>(std::clamp(green, 0.0, 1.0) * alpha);
    pixel[2] = static_cast<float>(std::clamp(blue, 0.0, 1.0) * alpha);
    pixel[3] = static_cast<float>(alpha);
}

// Each pixel's straight colour through `change(red, green, blue)`, alpha kept.
template <typename Change> Image each_color(concurrency::TaskPool& pool, const Image& image, Change change)
{
    Image result{image.width, image.height};
    for_each_row(pool, image.height,
                 [&](int y)
                 {
                     for(int x{}; x < image.width; ++ x)
                     {
                         double red{};
                         double green{};
                         double blue{};
                         double alpha{};
                         unpremultiply(image.at(x, y), red, green, blue, alpha);
                         change(red, green, blue);
                         premultiply(result.at(x, y), red, green, blue, alpha);
                     }
                 });
    return result;
}

// The average of the image at points along a path through each pixel, as motion, radial and zoom blurs take it.
template <typename Path> Image along(concurrency::TaskPool& pool, const Image& image, int samples, Path path)
{
    Image result{image.width, image.height};
    for_each_row(pool, image.height,
                 [&](int y)
                 {
                     for(int x{}; x < image.width; ++ x)
                     {
                         double sum[4]{};
                         for(int sample{}; sample < samples; ++ sample)
                         {
                             const auto t{samples == 1 ? 0.0 : static_cast<double>(sample) / (samples - 1) - 0.5};
                             double sx{};
                             double sy{};
                             path(x + 0.5, y + 0.5, t, sx, sy);
                             const auto* const pixel{
                                 image.clamped(static_cast<int>(std::floor(sx)), static_cast<int>(std::floor(sy)))};
                             for(int channel{}; channel < 4; ++ channel)
                             {
                                 sum[channel] += pixel[channel];
                             }
                         }
                         auto* const out{result.at(x, y)};
                         for(int channel{}; channel < 4; ++ channel)
                         {
                             out[channel] = static_cast<float>(sum[channel] / samples);
                         }
                     }
                 });
    return result;
}

// Each row averaged over a box `reach` pixels each side, its ends repeating its edge pixels, by a running sum: as fast
// whatever the reach.
Image box_rows(concurrency::TaskPool& pool, const Image& source, int reach)
{
    Image result{source.width, source.height};
    const auto last{source.width - 1};
    const auto scale{1.0 / (2 * reach + 1)};
    for_each_row(pool, source.height,
                 [&](int y)
                 {
                     double sum[4]{};
                     for(int offset{-reach}; offset <= reach; ++ offset)
                     {
                         const auto* const pixel{source.at(std::clamp(offset, 0, last), y)};
                         for(int channel{}; channel < 4; ++ channel)
                         {
                             sum[channel] += pixel[channel];
                         }
                     }
                     for(int x{}; x < source.width; ++ x)
                     {
                         auto* const out{result.at(x, y)};
                         const auto* const entering{source.at(std::min(x + reach + 1, last), y)};
                         const auto* const leaving{source.at(std::max(x - reach, 0), y)};
                         for(int channel{}; channel < 4; ++ channel)
                         {
                             out[channel] = static_cast<float>(sum[channel] * scale);
                             sum[channel] += entering[channel] - leaving[channel];
                         }
                     }
                 });
    return result;
}

// Rows become columns.
Image transposed(concurrency::TaskPool& pool, const Image& source)
{
    Image result{source.height, source.width};
    for_each_row(pool, result.height,
                 [&](int y)
                 {
                     for(int x{}; x < result.width; ++ x)
                     {
                         std::copy_n(source.at(y, x), 4, result.at(x, y));
                     }
                 });
    return result;
}

// Hue from 0 to 360, saturation and value from 0 to 1.
struct Hsv
{
    double hue{0.0};
    double saturation{0.0};
    double value{0.0};
};

Hsv to_hsv(double red, double green, double blue)
{
    const auto high{std::max({red, green, blue})};
    const auto low{std::min({red, green, blue})};
    const auto span{high - low};
    Hsv hsv{0.0, high > 0.0 ? span / high : 0.0, high};
    if(span <= 0.0)
    {
        return hsv;
    }
    if(high == red)
    {
        hsv.hue = 60.0 * std::fmod((green - blue) / span + 6.0, 6.0);
    }
    else if(high == green)
    {
        hsv.hue = 60.0 * ((blue - red) / span + 2.0);
    }
    else
    {
        hsv.hue = 60.0 * ((red - green) / span + 4.0);
    }
    return hsv;
}

void from_hsv(const Hsv& hsv, double& red, double& green, double& blue)
{
    const auto hue{std::fmod(std::fmod(hsv.hue, 360.0) + 360.0, 360.0) / 60.0};
    const auto chroma{hsv.value * hsv.saturation};
    const auto second{chroma * (1.0 - std::abs(std::fmod(hue, 2.0) - 1.0))};
    const auto base{hsv.value - chroma};
    red = green = blue = 0.0;
    switch(static_cast<int>(hue))
    {
    case 0:
        red = chroma, green = second;
        break;
    case 1:
        red = second, green = chroma;
        break;
    case 2:
        green = chroma, blue = second;
        break;
    case 3:
        green = second, blue = chroma;
        break;
    case 4:
        red = second, blue = chroma;
        break;
    default:
        red = chroma, blue = second;
        break;
    }
    red += base;
    green += base;
    blue += base;
}

// A value from 0 to 1 through a table spread evenly over 0 to 1, interpolated.
double through(const std::vector<double>& table, double value)
{
    if(table.empty())
    {
        return value;
    }
    const auto place{std::clamp(value, 0.0, 1.0) * static_cast<double>(table.size() - 1)};
    const auto low{static_cast<std::size_t>(std::floor(place))};
    const auto high{std::min(low + 1, table.size() - 1)};
    return table[low] + (table[high] - table[low]) * (place - static_cast<double>(low));
}

// One channel of `above` over one of `below`, as `mode` mixes them, both from 0 to 1.
double mix_channel(double below, double above, BlendMode mode)
{
    switch(mode)
    {
    case BlendMode::normal:
        return above;
    case BlendMode::multiply:
        return below * above;
    case BlendMode::additive:
        return std::min(1.0, below + above);
    case BlendMode::color_burn:
        return above <= 0.0 ? 0.0 : std::max(0.0, 1.0 - (1.0 - below) / above);
    case BlendMode::color_dodge:
        return above >= 1.0 ? 1.0 : std::min(1.0, below / (1.0 - above));
    case BlendMode::reflect:
        return above >= 1.0 ? 1.0 : std::min(1.0, below * below / (1.0 - above));
    case BlendMode::glow:
        return below >= 1.0 ? 1.0 : std::min(1.0, above * above / (1.0 - below));
    case BlendMode::overlay:
        return below < 0.5 ? 2.0 * below * above : 1.0 - 2.0 * (1.0 - below) * (1.0 - above);
    case BlendMode::difference:
        return std::abs(below - above);
    case BlendMode::negation:
        return 1.0 - std::abs(1.0 - below - above);
    case BlendMode::lighten:
        return std::max(below, above);
    case BlendMode::darken:
        return std::min(below, above);
    case BlendMode::screen:
        return 1.0 - (1.0 - below) * (1.0 - above);
    case BlendMode::xor_:
    {
        const auto bits{[](double value)
                        { return static_cast<unsigned>(std::lround(std::clamp(value, 0.0, 1.0) * 65535.0)); }};
        return static_cast<double>(bits(below) ^ bits(above)) / 65535.0;
    }
    }
    return above;
}

} // namespace

const float* Image::clamped(int x, int y) const
{
    return at(std::clamp(x, 0, width - 1), std::clamp(y, 0, height - 1));
}

Image gaussian_blur(concurrency::TaskPool& pool, const Image& image, double radius)
{
    if(radius <= 0.0)
    {
        return image;
    }
    const auto sigma{std::max(0.3, radius / 2.0)};
    const auto reach{static_cast<int>(std::ceil(radius))};
    // Wide, three box blurs one after another, whose sizes are worked out to make the same spread: as near a
    // gaussian as the eye tells, and as fast whatever the radius.
    if(reach > 12)
    {
        constexpr int boxes{3};
        const auto ideal{std::sqrt(12.0 * sigma * sigma / boxes + 1.0)};
        auto lower{static_cast<int>(std::floor(ideal))};
        if(lower % 2 == 0)
        {
            -- lower;
        }
        const auto upper{lower + 2};
        const auto with_lower{static_cast<int>(
            std::lround((12.0 * sigma * sigma - boxes * lower * lower - 4.0 * boxes * lower - 3.0 * boxes) /
                        (-4.0 * lower - 4.0)))};
        auto rows{image};
        for(int box{}; box < boxes; ++ box)
        {
            rows = box_rows(pool, rows, ((box < with_lower ? lower : upper) - 1) / 2);
        }
        auto columns{transposed(pool, rows)};
        for(int box{}; box < boxes; ++ box)
        {
            columns = box_rows(pool, columns, ((box < with_lower ? lower : upper) - 1) / 2);
        }
        return transposed(pool, columns);
    }
    std::vector<double> kernel(static_cast<std::size_t>(reach) * 2 + 1);
    auto total{0.0};
    for(int offset{-reach}; offset <= reach; ++ offset)
    {
        const auto weight{std::exp(-(offset * offset) / (2.0 * sigma * sigma))};
        kernel[static_cast<std::size_t>(offset + reach)] = weight;
        total += weight;
    }
    for(auto& weight : kernel)
    {
        weight /= total;
    }
    // Across, then down: a gaussian of two dimensions is two of one.
    const auto pass{[&](const Image& source, bool horizontal)
                    {
                        Image result{source.width, source.height};
                        for_each_row(pool, source.height,
                                     [&](int y)
                                     {
                                         for(int x{}; x < source.width; ++ x)
                                         {
                                             double sum[4]{};
                                             for(int offset{-reach}; offset <= reach; ++ offset)
                                             {
                                                 const auto* const pixel{horizontal ? source.clamped(x + offset, y)
                                                                                    : source.clamped(x, y + offset)};
                                                 const auto weight{kernel[static_cast<std::size_t>(offset + reach)]};
                                                 for(int channel{}; channel < 4; ++ channel)
                                                 {
                                                     sum[channel] += pixel[channel] * weight;
                                                 }
                                             }
                                             auto* const out{result.at(x, y)};
                                             for(int channel{}; channel < 4; ++ channel)
                                             {
                                                 out[channel] = static_cast<float>(sum[channel]);
                                             }
                                         }
                                     });
                        return result;
                    }};
    return pass(pass(image, true), false);
}

Image box_blur(concurrency::TaskPool& pool, const Image& image, int radius)
{
    const auto size{std::max(0, radius) * 2 + 1};
    const std::vector<double> matrix(static_cast<std::size_t>(size) * static_cast<std::size_t>(size), 1.0);
    return convolve(pool, image, matrix, size, static_cast<double>(matrix.size()), 0.0);
}

Image motion_blur(concurrency::TaskPool& pool, const Image& image, double degrees, double distance)
{
    const auto dx{std::cos(degrees * pi / 180.0) * distance};
    const auto dy{-std::sin(degrees * pi / 180.0) * distance};
    const auto samples{std::max(1, static_cast<int>(std::ceil(distance)) + 1)};
    return along(pool, image, samples,
                 [&](double x, double y, double t, double& sx, double& sy)
                 {
                     sx = x + dx * t;
                     sy = y + dy * t;
                 });
}

Image radial_blur(concurrency::TaskPool& pool, const Image& image, double degrees, double center_x, double center_y)
{
    const auto span{degrees * pi / 180.0};
    const auto farthest{
        std::hypot(std::max(center_x, image.width - center_x), std::max(center_y, image.height - center_y))};
    const auto samples{std::min(64, std::max(1, static_cast<int>(std::ceil(span * farthest / 2.0)) + 1))};
    // The turn of each sample, the same for every pixel, worked out once.
    std::vector<std::pair<double, double>> turns;
    for(int sample{}; sample < samples; ++ sample)
    {
        const auto t{samples == 1 ? 0.0 : static_cast<double>(sample) / (samples - 1) - 0.5};
        turns.emplace_back(std::cos(span * t), std::sin(span * t));
    }
    return along(pool, image, samples,
                 [&](double x, double y, double t, double& sx, double& sy)
                 {
                     const auto sample{samples == 1 ? 0 : static_cast<int>(std::lround((t + 0.5) * (samples - 1)))};
                     const auto [cosine, sine]{turns[static_cast<std::size_t>(sample)]};
                     const auto rx{x - center_x};
                     const auto ry{y - center_y};
                     sx = center_x + rx * cosine - ry * sine;
                     sy = center_y + rx * sine + ry * cosine;
                 });
}

Image zoom_blur(concurrency::TaskPool& pool, const Image& image, double amount, double center_x, double center_y)
{
    const auto farthest{
        std::hypot(std::max(center_x, image.width - center_x), std::max(center_y, image.height - center_y))};
    const auto samples{std::max(1, std::min(64, static_cast<int>(std::ceil(amount * farthest / 2.0)) + 1))};
    return along(pool, image, samples,
                 [&](double x, double y, double t, double& sx, double& sy)
                 {
                     // From the pixel towards the middle.
                     const auto toward{(t + 0.5) * amount};
                     sx = x + (center_x - x) * toward;
                     sy = y + (center_y - y) * toward;
                 });
}

Image convolve(concurrency::TaskPool& pool, const Image& image, const std::vector<double>& matrix, int size,
               double divisor, double offset)
{
    Image result{image.width, image.height};
    const auto half{size / 2};
    const auto divide{divisor != 0.0 ? 1.0 / divisor : 1.0};
    for_each_row(pool, image.height,
                 [&](int y)
                 {
                     for(int x{}; x < image.width; ++ x)
                     {
                         double sum[3]{};
                         for(int j{}; j < size; ++ j)
                         {
                             for(int i{}; i < size; ++ i)
                             {
                                 const auto weight{matrix[static_cast<std::size_t>(j * size + i)]};
                                 if(weight == 0.0)
                                 {
                                     continue;
                                 }
                                 double red{};
                                 double green{};
                                 double blue{};
                                 double alpha{};
                                 unpremultiply(image.clamped(x + i - half, y + j - half), red, green, blue, alpha);
                                 sum[0] += red * weight;
                                 sum[1] += green * weight;
                                 sum[2] += blue * weight;
                             }
                         }
                         premultiply(result.at(x, y), sum[0] * divide + offset, sum[1] * divide + offset,
                                     sum[2] * divide + offset, image.at(x, y)[3]);
                     }
                 });
    return result;
}

Image add_noise(concurrency::TaskPool& pool, const Image& image, double intensity, double saturation, double coverage,
                std::uint32_t seed)
{
    Image result{image};
    for_each_row(pool, image.height,
                 [&](int y)
                 {
                     for(int x{}; x < image.width; ++ x)
                     {
                         // Numbers from 0 to 1 from the pixel's place and the seed alone, so every row is the same on
                         // any thread.
                         auto state{static_cast<std::uint32_t>(x) * 0x9e3779b1u ^
                                    static_cast<std::uint32_t>(y) * 0x85ebca77u ^ seed * 0xc2b2ae3du};
                         const auto next{[&state]
                                         {
                                             state ^= state >> 16u;
                                             state *= 0x7feb352du;
                                             state ^= state >> 15u;
                                             state *= 0x846ca68bu;
                                             state ^= state >> 16u;
                                             return state / 4294967296.0;
                                         }};
                         if(next() >= coverage)
                         {
                             continue;
                         }
                         auto* const pixel{result.at(x, y)};
                         const auto alpha{static_cast<double>(pixel[3])};
                         const auto gray{(next() * 2.0 - 1.0) * intensity};
                         for(int channel{}; channel < 3; ++ channel)
                         {
                             const auto noise{gray + (next() * 2.0 - 1.0) * intensity * saturation * 0.5};
                             pixel[channel] =
                                 static_cast<float>(std::clamp(pixel[channel] + noise * alpha, 0.0, alpha));
                         }
                     }
                 });
    return result;
}

Image lookup(concurrency::TaskPool& pool, const Image& image, const std::array<std::vector<double>, 3>& tables)
{
    return each_color(pool, image,
                      [&](double& red, double& green, double& blue)
                      {
                          red = through(tables[0], red);
                          green = through(tables[1], green);
                          blue = through(tables[2], blue);
                      });
}

Image lookup_luminosity(concurrency::TaskPool& pool, const Image& image, const std::vector<double>& table)
{
    return each_color(pool, image,
                      [&](double& red, double& green, double& blue)
                      {
                          const auto luminance{0.2126 * red + 0.7152 * green + 0.0722 * blue};
                          const auto wanted{through(table, luminance)};
                          if(luminance <= 0.0)
                          {
                              red = green = blue = wanted;
                              return;
                          }
                          // The colour scaled to the luminance wanted; what that pushes past white is spread towards
                          // white, so the luminance stays what the table says.
                          red *= wanted / luminance;
                          green *= wanted / luminance;
                          blue *= wanted / luminance;
                          const auto highest{std::max({red, green, blue})};
                          if(highest > 1.0)
                          {
                              const auto towards{(highest - 1.0) / std::max(highest - wanted, 1e-9)};
                              red += (wanted - red) * towards;
                              green += (wanted - green) * towards;
                              blue += (wanted - blue) * towards;
                          }
                      });
}

Image hue_saturation(concurrency::TaskPool& pool, const Image& image, double hue, double saturation, double lightness)
{
    return each_color(pool, image,
                      [&](double& red, double& green, double& blue)
                      {
                          auto hsv{to_hsv(red, green, blue)};
                          hsv.hue += hue;
                          hsv.saturation = std::clamp(hsv.saturation * (1.0 + saturation), 0.0, 1.0);
                          from_hsv(hsv, red, green, blue);
                          // Lightness towards white above 0, towards black below.
                          const auto target{lightness > 0.0 ? 1.0 : 0.0};
                          const auto amount{std::abs(lightness)};
                          red += (target - red) * amount;
                          green += (target - green) * amount;
                          blue += (target - blue) * amount;
                      });
}

Image mix(concurrency::TaskPool& pool, const Image& below, const Image& above, BlendMode mode, double opacity)
{
    Image result{below.width, below.height};
    for_each_row(pool, below.height,
                 [&](int y)
                 {
                     for(int x{}; x < below.width; ++ x)
                     {
                         double base[4]{};
                         double over[4]{};
                         unpremultiply(below.at(x, y), base[0], base[1], base[2], base[3]);
                         if(x < above.width && y < above.height)
                         {
                             unpremultiply(above.at(x, y), over[0], over[1], over[2], over[3]);
                         }
                         const auto above_alpha{over[3] * opacity};
                         const auto below_alpha{base[3]};
                         const auto alpha{above_alpha + below_alpha * (1.0 - above_alpha)};
                         auto* const out{result.at(x, y)};
                         if(alpha <= 0.0)
                         {
                             std::fill_n(out, 4, 0.0f);
                             continue;
                         }
                         double color[3]{};
                         for(int channel{}; channel < 3; ++ channel)
                         {
                             const auto mixed{mix_channel(base[channel], over[channel], mode)};
                             const auto shown{(1.0 - below_alpha) * over[channel] + below_alpha * mixed};
                             color[channel] =
                                 (shown * above_alpha + base[channel] * below_alpha * (1.0 - above_alpha)) / alpha;
                         }
                         premultiply(out, color[0], color[1], color[2], alpha);
                     }
                 });
    return result;
}

Image vignette(concurrency::TaskPool& pool, const Image& image, double amount, double softness)
{
    Image result{image.width, image.height};
    const auto half_width{image.width / 2.0};
    const auto half_height{image.height / 2.0};
    const auto inner{1.0 - std::clamp(softness, 0.0, 1.0)};
    for_each_row(pool, image.height,
                 [&](int y)
                 {
                     for(int x{}; x < image.width; ++ x)
                     {
                         // 1 on the ellipse touching the sides.
                         const auto distance{
                             std::hypot((x + 0.5 - half_width) / half_width, (y + 0.5 - half_height) / half_height)};
                         auto edge{distance >= 1.0 ? 1.0 : 0.0};
                         if(distance < 1.0 && inner < 1.0 && distance > inner)
                         {
                             const auto t{(distance - inner) / (1.0 - inner)};
                             edge = t * t * (3.0 - 2.0 * t);
                         }
                         const auto kept{static_cast<float>(1.0 - amount * edge)};
                         const auto* const pixel{image.at(x, y)};
                         auto* const out{result.at(x, y)};
                         for(int channel{}; channel < 3; ++ channel)
                         {
                             out[channel] = pixel[channel] * kept;
                         }
                         out[3] = pixel[3];
                     }
                 });
    return result;
}

Image tint(concurrency::TaskPool& pool, const Image& image, const std::array<double, 12>& matrix)
{
    return each_color(pool, image,
                      [&](double& red, double& green, double& blue)
                      {
                          const auto row{[&](std::size_t first) {
                              return matrix[first] * red + matrix[first + 1] * green + matrix[first + 2] * blue +
                                     matrix[first + 3];
                          }};
                          const auto new_red{row(0)};
                          const auto new_green{row(4)};
                          blue = row(8);
                          red = new_red;
                          green = new_green;
                      });
}

ChannelRange channel_range(concurrency::TaskPool& pool, const Image& image)
{
    // Each row's range, then the rows' together.
    std::vector<ChannelRange> rows(static_cast<std::size_t>(image.height));
    for_each_row(pool, image.height,
                 [&](int y)
                 {
                     auto& range{rows[static_cast<std::size_t>(y)]};
                     for(int x{}; x < image.width; ++ x)
                     {
                         const auto* const pixel{image.at(x, y)};
                         if(pixel[3] <= 0.0f)
                         {
                             continue;
                         }
                         for(std::size_t channel{}; channel < 3; ++ channel)
                         {
                             const auto value{static_cast<double>(pixel[channel]) / pixel[3]};
                             range.low[channel] = std::min(range.low[channel], value);
                             range.high[channel] = std::max(range.high[channel], value);
                         }
                     }
                 });
    ChannelRange whole;
    for(const auto& row : rows)
    {
        for(std::size_t channel{}; channel < 3; ++ channel)
        {
            whole.low[channel] = std::min(whole.low[channel], row.low[channel]);
            whole.high[channel] = std::max(whole.high[channel], row.high[channel]);
        }
    }
    return whole;
}

} // namespace olagarro::effects
