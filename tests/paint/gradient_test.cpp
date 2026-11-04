#include <paint/gradient.h>

#include <gtest/gtest.h>

#include <set>

using namespace olagarro;
using namespace olagarro::paint;

TEST(Gradient, EachKindRunsFromTheStartToTheEnd)
{
    const Position start{10, 10};
    const Position end{20, 10};
    EXPECT_DOUBLE_EQ(gradient_position(GradientKind::linear, start, end, {15, 30}), 0.5);
    EXPECT_DOUBLE_EQ(gradient_position(GradientKind::linear, start, end, {0, 10}), 0.0);
    EXPECT_DOUBLE_EQ(gradient_position(GradientKind::linear_reflected, start, end, {5, 10}), 0.5);
    EXPECT_DOUBLE_EQ(gradient_position(GradientKind::radial, start, end, {10, 15}), 0.5);
    EXPECT_DOUBLE_EQ(gradient_position(GradientKind::diamond, start, end, {13, 12}), 0.5);
    EXPECT_NEAR(gradient_position(GradientKind::conical, start, end, {10, 20}), 0.5, 1e-9);
    EXPECT_NEAR(gradient_position(GradientKind::conical, start, end, {0, 10}), 1.0, 1e-9);
}

TEST(Gradient, ASixteenBitGreyGradientTakesEveryStep)
{
    Canvas canvas{PixelFormat::gray_alpha16, 1000, 1};
    const Canvas base{canvas};
    const Selection everything{1000, 1};
    draw_gradient(canvas, base, everything, GradientKind::linear, {0.5, 0}, {999.5, 0}, {0, 0, 0, 1}, {1, 1, 1, 1},
                  false);
    const auto row{canvas.read(0, 0, 1000, 1)};
    EXPECT_EQ(row.pixel<GrayAlpha16>(0, 0).value[0], 0);
    EXPECT_EQ(row.pixel<GrayAlpha16>(999, 0).value[0], 65535);
    // A thousand distinct values: 16 bits keep what 8 would round to 256.
    std::set<std::uint16_t> values;
    for(int x{}; x < 1000; ++ x)
    {
        values.insert(row.pixel<GrayAlpha16>(x, 0).value[0]);
    }
    EXPECT_EQ(values.size(), 1000u);
}

TEST(Gradient, TransparencyModeFadesTheCanvasItself)
{
    Canvas canvas{PixelFormat::rgba8, 10, 1};
    Image red{PixelFormat::rgba8, 10, 1};
    for(int x{}; x < 10; ++ x)
    {
        red.set_pixel(x, 0, Rgba8{{255, 0, 0, 255}});
    }
    canvas.write(red, 0, 0);
    const Canvas base{canvas};
    const Selection everything{10, 1};
    draw_gradient(canvas, base, everything, GradientKind::linear, {0, 0}, {10, 0}, {}, {}, true);
    const auto row{canvas.read(0, 0, 10, 1)};
    EXPECT_GT(row.pixel<Rgba8>(0, 0).alpha(), 230);
    EXPECT_LT(row.pixel<Rgba8>(9, 0).alpha(), 25);
    EXPECT_EQ(row.pixel<Rgba8>(9, 0).value[0], 255) << "its colour stays";
}

TEST(Gradient, ADitheredGradientUsesOnlyItsTwoIndicesMoreOfTheSecondFurtherOn)
{
    Canvas canvas{PixelFormat::gray_alpha8, 64, 4};
    const Canvas base{canvas};
    const Selection everything{64, 4};
    draw_dithered_gradient(canvas, base, everything, GradientKind::linear, {0, 0}, {64, 0}, GrayAlpha8{{2, 255}},
                           GrayAlpha8{{9, 255}});
    const auto image{canvas.read(0, 0, 64, 4)};
    auto seconds_left{0};
    auto seconds_right{0};
    for(int y{}; y < 4; ++ y)
    {
        for(int x{}; x < 64; ++ x)
        {
            const auto value{image.pixel<GrayAlpha8>(x, y).value[0]};
            ASSERT_TRUE(value == 2 || value == 9);
            (x < 16 ? seconds_left : seconds_right) += (x < 16 || x >= 48) && value == 9 ? 1 : 0;
        }
    }
    EXPECT_LT(seconds_left, seconds_right);
}
