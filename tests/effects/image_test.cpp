#include <effects/image.h>

#include <gtest/gtest.h>

#include <cmath>
#include <future>
#include <initializer_list>

using namespace olagarro;
using namespace olagarro::effects;

namespace
{

// Black and solid, with one white pixel at (dot_x, dot_y).
Image dot(int width, int height, int dot_x, int dot_y)
{
    Image image{width, height};
    for(int y{}; y < height; ++ y)
    {
        for(int x{}; x < width; ++ x)
        {
            auto* const pixel{image.at(x, y)};
            const auto lit{x == dot_x && y == dot_y};
            pixel[0] = pixel[1] = pixel[2] = lit ? 1.0f : 0.0f;
            pixel[3] = 1.0f;
        }
    }
    return image;
}

Image dot(int size)
{
    return dot(size, size, size / 2, size / 2);
}

// Every pixel the same.
Image filled(int width, int height, float red, float green, float blue, float alpha)
{
    Image image{width, height};
    for(int y{}; y < height; ++ y)
    {
        for(int x{}; x < width; ++ x)
        {
            auto* const pixel{image.at(x, y)};
            pixel[0] = red;
            pixel[1] = green;
            pixel[2] = blue;
            pixel[3] = alpha;
        }
    }
    return image;
}

// Every colour and alpha a different number, from the pixel's place.
Image pattern(int width, int height)
{
    Image image{width, height};
    for(int y{}; y < height; ++ y)
    {
        for(int x{}; x < width; ++ x)
        {
            auto* const pixel{image.at(x, y)};
            pixel[3] = 0.5f + 0.5f * static_cast<float>((x * 7 + y * 3) % 11) / 10.0f;
            pixel[0] = pixel[3] * static_cast<float>((x * 13 + y * 5) % 17) / 16.0f;
            pixel[1] = pixel[3] * static_cast<float>((x * 3 + y * 11) % 7) / 6.0f;
            pixel[2] = pixel[3] * static_cast<float>((x + y * 7) % 5) / 4.0f;
        }
    }
    return image;
}

double red_sum(const Image& image)
{
    auto total{0.0};
    for(int y{}; y < image.height; ++ y)
    {
        for(int x{}; x < image.width; ++ x)
        {
            total += image.at(x, y)[0];
        }
    }
    return total;
}

void expect_pixel(const Image& image, int x, int y, std::initializer_list<double> expected)
{
    auto channel{0};
    for(const auto value : expected)
    {
        EXPECT_NEAR(image.at(x, y)[channel], value, 1e-5) << "pixel " << x << ", " << y << ", channel " << channel;
        ++ channel;
    }
}

} // namespace

TEST(EffectsImage, AGaussianBlurSpreadsADotAndKeepsItsLight)
{
    concurrency::TaskPool pool{2};
    const auto blurred{gaussian_blur(pool, dot(21), 4.0)};
    EXPECT_NEAR(red_sum(blurred), 1.0, 1e-4);
    EXPECT_LT(blurred.at(10, 10)[0], 0.5f);
    EXPECT_GT(blurred.at(12, 10)[0], 0.0f);
    EXPECT_FLOAT_EQ(blurred.at(10, 12)[0], blurred.at(12, 10)[0]) << "the same every way";
    EXPECT_FLOAT_EQ(blurred.at(3, 7)[3], 1.0f) << "solid stays solid";
}

TEST(EffectsImage, AWideGaussianBlurHasAGaussiansShapeAndKeepsItsLight)
{
    concurrency::TaskPool pool{2};
    const auto blurred{gaussian_blur(pool, dot(201), 30.0)};
    EXPECT_NEAR(red_sum(blurred), 1.0, 1e-3);
    // The radius is two sigmas, as the narrow one's.
    const auto middle{blurred.at(100, 100)[0]};
    for(const auto distance : {5, 10, 15, 20, 25})
    {
        const auto expected{std::exp(-distance * distance / (2.0 * 15.0 * 15.0))};
        EXPECT_NEAR(blurred.at(100 + distance, 100)[0] / middle, expected, 0.06) << distance;
        EXPECT_NEAR(blurred.at(100, 100 - distance)[0], blurred.at(100 + distance, 100)[0], 1e-6) << distance;
    }
}

TEST(EffectsImage, NothingToDoChangesNothing)
{
    concurrency::TaskPool pool{2};
    const auto image{dot(9, 7, 3, 2)};
    EXPECT_EQ(gaussian_blur(pool, image, 0.0).pixels, image.pixels);
    EXPECT_EQ(box_blur(pool, image, 0).pixels, image.pixels);
    EXPECT_EQ(motion_blur(pool, image, 30.0, 0.0).pixels, image.pixels);
    EXPECT_EQ(radial_blur(pool, image, 0.0, 4.5, 3.5).pixels, image.pixels);
    EXPECT_EQ(zoom_blur(pool, image, 0.0, 4.5, 3.5).pixels, image.pixels);
    EXPECT_EQ(add_noise(pool, image, 0.5, 1.0, 0.0, 3).pixels, image.pixels);
    EXPECT_EQ(lookup(pool, image, {}).pixels, image.pixels);
    EXPECT_EQ(hue_saturation(pool, image, 0.0, 0.0, 0.0).pixels, image.pixels);
    EXPECT_EQ(vignette(pool, image, 0.0, 0.5).pixels, image.pixels);
    EXPECT_EQ(tint(pool, image, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0}).pixels, image.pixels);
}

TEST(EffectsImage, ABoxBlurAveragesTheSquareRoundEachPixel)
{
    concurrency::TaskPool pool{2};
    const auto blurred{box_blur(pool, dot(9), 1)};
    EXPECT_NEAR(red_sum(blurred), 1.0, 1e-5);
    for(int y{3}; y <= 5; ++ y)
    {
        for(int x{3}; x <= 5; ++ x)
        {
            expect_pixel(blurred, x, y, {1.0 / 9.0, 1.0 / 9.0, 1.0 / 9.0, 1.0});
        }
    }
    expect_pixel(blurred, 6, 4, {0.0, 0.0, 0.0, 1.0});
}

TEST(EffectsImage, AMotionBlurSmearsAlongItsAngleOnly)
{
    concurrency::TaskPool pool{2};
    const auto across{motion_blur(pool, dot(21), 0.0, 8.0)};
    EXPECT_GT(across.at(13, 10)[0], 0.0f);
    EXPECT_FLOAT_EQ(across.at(10, 13)[0], 0.0f);
    const auto down{motion_blur(pool, dot(21), 90.0, 8.0)};
    EXPECT_GT(down.at(10, 13)[0], 0.0f);
    EXPECT_FLOAT_EQ(down.at(13, 10)[0], 0.0f);
}

TEST(EffectsImage, ARadialBlurSmearsRoundItsCentreAndLeavesTheCentre)
{
    concurrency::TaskPool pool{2};
    // The dot 8 pixels right of the centre, (10.5, 10.5); an arc of 40 degrees reaches about 2.8 pixels up and down.
    const auto turned{radial_blur(pool, dot(21, 21, 18, 10), 40.0, 10.5, 10.5)};
    EXPECT_GT(turned.at(18, 12)[0], 0.0f);
    EXPECT_GT(turned.at(18, 8)[0], 0.0f);
    EXPECT_FLOAT_EQ(turned.at(20, 10)[0], 0.0f) << "not outwards";
    EXPECT_FLOAT_EQ(turned.at(15, 10)[0], 0.0f) << "nor inwards";
    const auto middle{radial_blur(pool, dot(21), 90.0, 10.5, 10.5)};
    EXPECT_FLOAT_EQ(middle.at(10, 10)[0], 1.0f) << "the centre turns onto itself";
}

TEST(EffectsImage, AZoomBlurSmearsAlongTheLineFromItsCentre)
{
    concurrency::TaskPool pool{2};
    const auto zoomed{zoom_blur(pool, dot(21, 21, 15, 10), 0.5, 10.5, 10.5)};
    EXPECT_GT(zoomed.at(17, 10)[0], 0.0f) << "outwards from the dot";
    EXPECT_FLOAT_EQ(zoomed.at(13, 10)[0], 0.0f) << "a pixel nearer the centre looks inwards, away from the dot";
    EXPECT_FLOAT_EQ(zoomed.at(15, 13)[0], 0.0f);
    const auto flat{zoom_blur(pool, filled(9, 9, 0.25f, 0.5f, 0.125f, 1.0f), 1.0, 2.0, 3.0)};
    expect_pixel(flat, 7, 1, {0.25, 0.5, 0.125, 1.0});
}

TEST(EffectsImage, EdgeDetectionFindsTheEdgeAndFlatPlacesStayGrey)
{
    concurrency::TaskPool pool{2};
    Image half{10, 10};
    for(int y{}; y < 10; ++ y)
    {
        for(int x{}; x < 10; ++ x)
        {
            auto* const pixel{half.at(x, y)};
            pixel[0] = pixel[1] = pixel[2] = x < 5 ? 0.0f : 1.0f;
            pixel[3] = 1.0f;
        }
    }
    const auto edges{convolve(pool, half, {-1, 0, 1, -2, 0, 2, -1, 0, 1}, 3, 1.0, 0.5)};
    EXPECT_NEAR(edges.at(2, 5)[0], 0.5, 1e-6);
    EXPECT_GT(edges.at(5, 5)[0], 0.9f);
}

TEST(EffectsImage, NoiseIsTheSameForTheSameSeedAndCoversWhatItIsAskedTo)
{
    concurrency::TaskPool pool{2};
    const auto gray{filled(64, 64, 0.5f, 0.5f, 0.5f, 1.0f)};
    const auto first{add_noise(pool, gray, 0.3, 1.0, 0.5, 7)};
    EXPECT_EQ(first.pixels, add_noise(pool, gray, 0.3, 1.0, 0.5, 7).pixels);
    EXPECT_NE(first.pixels, add_noise(pool, gray, 0.3, 1.0, 0.5, 8).pixels);
    auto changed{0};
    for(int y{}; y < 64; ++ y)
    {
        for(int x{}; x < 64; ++ x)
        {
            const auto* const pixel{first.at(x, y)};
            changed += pixel[0] != 0.5f ? 1 : 0;
            EXPECT_GE(pixel[0], 0.05f - 1e-6f) << "lighter or darker by 0.3, and the channel by half that again";
            EXPECT_LE(pixel[0], 0.95f + 1e-6f);
            EXPECT_FLOAT_EQ(pixel[3], 1.0f);
        }
    }
    EXPECT_NEAR(changed / 4096.0, 0.5, 0.05) << "half the pixels";
}

TEST(EffectsImage, ATableTakesEachChannelThroughItInterpolated)
{
    concurrency::TaskPool pool{2};
    // Straight colour 0.5, 0.25, 1 at half alpha.
    const auto image{filled(1, 1, 0.25f, 0.125f, 0.5f, 0.5f)};
    const std::vector<double> inverse{1.0, 0.0};
    const std::vector<double> bent{0.0, 0.8, 1.0};
    const auto result{lookup(pool, image, {inverse, bent, {}})};
    // Red 0.5 inverted is 0.5; green 0.25 is half way to 0.8; blue has no table.
    expect_pixel(result, 0, 0, {0.25, 0.2, 0.5, 0.5});
}

TEST(EffectsImage, ALuminosityTableChangesHowLightAColourIsAndKeepsItsHue)
{
    concurrency::TaskPool pool{2};
    Image image{2, 1};
    const float orange[4]{0.8f, 0.4f, 0.2f, 1.0f};
    const float half_blue[4]{0.0f, 0.0f, 0.25f, 0.5f};
    std::copy(orange, orange + 4, image.at(0, 0));
    std::copy(half_blue, half_blue + 4, image.at(1, 0));
    // Everything half as light.
    const auto result{lookup_luminosity(pool, image, {0.0, 0.5})};
    expect_pixel(result, 0, 0, {0.4, 0.2, 0.1, 1.0});
    expect_pixel(result, 1, 0, {0.0, 0.0, 0.125, 0.5});
    // Everything white: a colour past white goes to it.
    const auto white{lookup_luminosity(pool, image, {1.0, 1.0})};
    expect_pixel(white, 0, 0, {1.0, 1.0, 1.0, 1.0});
}

TEST(EffectsImage, HueTurnsSaturationGreysAndLightnessGoesTowardsWhiteOrBlack)
{
    concurrency::TaskPool pool{2};
    const auto red{filled(1, 1, 0.5f, 0.0f, 0.0f, 0.5f)};
    expect_pixel(hue_saturation(pool, red, 120.0, 0.0, 0.0), 0, 0, {0.0, 0.5, 0.0, 0.5});
    expect_pixel(hue_saturation(pool, red, -120.0, 0.0, 0.0), 0, 0, {0.0, 0.0, 0.5, 0.5});
    // No saturation leaves the value, 1, in every channel.
    expect_pixel(hue_saturation(pool, red, 0.0, -1.0, 0.0), 0, 0, {0.5, 0.5, 0.5, 0.5});
    expect_pixel(hue_saturation(pool, red, 0.0, 0.0, 0.5), 0, 0, {0.5, 0.25, 0.25, 0.5});
    expect_pixel(hue_saturation(pool, red, 0.0, 0.0, -0.5), 0, 0, {0.25, 0.0, 0.0, 0.5});
}

TEST(EffectsImage, MixLaysOneImageOverAnotherAsItsModeSays)
{
    concurrency::TaskPool pool{2};
    const auto grey{filled(2, 2, 0.5f, 0.5f, 0.5f, 1.0f)};
    const auto orange{filled(2, 2, 1.0f, 0.5f, 0.0f, 1.0f)};
    expect_pixel(mix(pool, grey, orange, BlendMode::normal, 1.0), 0, 0, {1.0, 0.5, 0.0, 1.0});
    expect_pixel(mix(pool, grey, orange, BlendMode::normal, 0.5), 1, 1, {0.75, 0.5, 0.25, 1.0});
    expect_pixel(mix(pool, grey, orange, BlendMode::multiply, 1.0), 0, 1, {0.5, 0.25, 0.0, 1.0});
    expect_pixel(mix(pool, grey, orange, BlendMode::screen, 1.0), 1, 0, {1.0, 0.75, 0.5, 1.0});
    expect_pixel(mix(pool, grey, orange, BlendMode::difference, 1.0), 0, 0, {0.5, 0.0, 0.5, 1.0});
    // Over nothing, what is above shows as it is, whatever the mode; nothing above leaves what is below.
    const Image nothing{2, 2};
    expect_pixel(mix(pool, nothing, orange, BlendMode::multiply, 0.5), 0, 0, {0.5, 0.25, 0.0, 0.5});
    expect_pixel(mix(pool, grey, nothing, BlendMode::screen, 1.0), 0, 0, {0.5, 0.5, 0.5, 1.0});
    // Where the image above does not reach.
    const auto small{filled(1, 1, 1.0f, 0.5f, 0.0f, 1.0f)};
    expect_pixel(mix(pool, grey, small, BlendMode::normal, 1.0), 1, 1, {0.5, 0.5, 0.5, 1.0});
}

TEST(EffectsImage, AVignetteDarkensTheCornersAndLeavesTheMiddle)
{
    concurrency::TaskPool pool{2};
    const auto white{filled(21, 11, 1.0f, 1.0f, 1.0f, 1.0f)};
    const auto hard{vignette(pool, white, 0.75, 0.0)};
    expect_pixel(hard, 10, 5, {1.0, 1.0, 1.0, 1.0});
    expect_pixel(hard, 0, 0, {0.25, 0.25, 0.25, 1.0});
    // Inside the ellipse.
    expect_pixel(hard, 18, 5, {1.0, 1.0, 1.0, 1.0});
    const auto soft{vignette(pool, white, 0.75, 1.0)};
    expect_pixel(soft, 10, 5, {1.0, 1.0, 1.0, 1.0});
    expect_pixel(soft, 0, 0, {0.25, 0.25, 0.25, 1.0});
    EXPECT_LT(soft.at(18, 5)[0], 0.9f) << "darker from the middle out";
    EXPECT_GT(soft.at(18, 5)[0], 0.25f);
    EXPECT_GT(soft.at(16, 5)[0], soft.at(18, 5)[0]);
    const auto half_seen{vignette(pool, filled(21, 11, 0.5f, 0.5f, 0.5f, 0.5f), 0.5, 0.0)};
    expect_pixel(half_seen, 0, 0, {0.25, 0.25, 0.25, 0.5});
}

TEST(EffectsImage, ATintTakesEachColourThroughItsMatrix)
{
    concurrency::TaskPool pool{2};
    const auto image{filled(1, 1, 0.5f, 0.25f, 0.0f, 0.5f)};
    // Straight colour 1, 0.5, 0: grey of a third of each, then red from blue and a constant.
    const auto grey{
        tint(pool, image, {1.0 / 3, 1.0 / 3, 1.0 / 3, 0, 1.0 / 3, 1.0 / 3, 1.0 / 3, 0, 1.0 / 3, 1.0 / 3, 1.0 / 3, 0})};
    expect_pixel(grey, 0, 0, {0.25, 0.25, 0.25, 0.5});
    const auto swapped{tint(pool, image, {0, 0, 1, 0.25, 0, 1, 0, 0, 1, 0, 0, 0})};
    expect_pixel(swapped, 0, 0, {0.125, 0.25, 0.5, 0.5});
    const auto past_white{tint(pool, image, {2, 0, 0, 0, 0, 0, 0, -1, 1, 0, 0, 0})};
    // Colours kept from 0 to 1.
    expect_pixel(past_white, 0, 0, {0.5, 0.0, 0.5, 0.5});
}

TEST(EffectsImage, TheRangeOfEachChannelLeavesOutWhatIsSeeThrough)
{
    concurrency::TaskPool pool{2};
    Image image{3, 1};
    const float first[4]{0.1f, 0.2f, 0.3f, 0.5f};
    const float second[4]{0.4f, 0.1f, 0.2f, 1.0f};
    std::copy(first, first + 4, image.at(0, 0));
    std::copy(second, second + 4, image.at(1, 0));
    const auto range{channel_range(pool, image)};
    EXPECT_NEAR(range.low[0], 0.2, 1e-6);
    EXPECT_NEAR(range.high[0], 0.4, 1e-6);
    EXPECT_NEAR(range.low[1], 0.1, 1e-6);
    EXPECT_NEAR(range.high[1], 0.4, 1e-6);
    EXPECT_NEAR(range.low[2], 0.2, 1e-6);
    EXPECT_NEAR(range.high[2], 0.6, 1e-6);
    const auto none{channel_range(pool, Image{2, 2})};
    EXPECT_EQ(none.low[0], 1.0);
    EXPECT_EQ(none.high[2], 0.0);
}

TEST(EffectsImage, EveryPieceGivesTheSameOnManyThreadsAsOnOne)
{
    const auto image{pattern(97, 83)};
    const auto above{pattern(83, 97)};
    concurrency::TaskPool many{8};
    concurrency::TaskPool one{1};
    // The pool of one kept busy, so the thread asking does every row alone.
    std::promise<void> release;
    std::promise<void> started;
    auto blocker{one.submit(
        [&release, &started](const concurrency::CancelToken&)
        {
            started.set_value();
            release.get_future().wait();
        })};
    started.get_future().wait();
    const auto both{[&](auto piece)
                    {
                        const Image by_many{piece(many)};
                        const Image by_one{piece(one)};
                        return by_many.pixels == by_one.pixels;
                    }};
    EXPECT_TRUE(both([&](concurrency::TaskPool& pool) { return gaussian_blur(pool, image, 3.0); }));
    EXPECT_TRUE(both([&](concurrency::TaskPool& pool) { return gaussian_blur(pool, image, 20.0); }));
    EXPECT_TRUE(both([&](concurrency::TaskPool& pool) { return box_blur(pool, image, 2); }));
    EXPECT_TRUE(both([&](concurrency::TaskPool& pool) { return motion_blur(pool, image, 30.0, 9.0); }));
    EXPECT_TRUE(both([&](concurrency::TaskPool& pool) { return radial_blur(pool, image, 20.0, 40.0, 30.0); }));
    EXPECT_TRUE(both([&](concurrency::TaskPool& pool) { return zoom_blur(pool, image, 0.3, 40.0, 30.0); }));
    EXPECT_TRUE(both([&](concurrency::TaskPool& pool) { return add_noise(pool, image, 0.2, 0.5, 0.7, 11); }));
    EXPECT_TRUE(both([&](concurrency::TaskPool& pool) { return lookup_luminosity(pool, image, {0.0, 0.7, 1.0}); }));
    EXPECT_TRUE(both([&](concurrency::TaskPool& pool) { return hue_saturation(pool, image, 40.0, 0.3, -0.2); }));
    EXPECT_TRUE(both([&](concurrency::TaskPool& pool) { return mix(pool, image, above, BlendMode::overlay, 0.7); }));
    EXPECT_TRUE(both([&](concurrency::TaskPool& pool) { return vignette(pool, image, 0.6, 0.5); }));
    const auto range_many{channel_range(many, image)};
    const auto range_one{channel_range(one, image)};
    EXPECT_EQ(range_many.low, range_one.low);
    EXPECT_EQ(range_many.high, range_one.high);
    release.set_value();
    blocker.get();
}
