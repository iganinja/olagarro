#include <paint/stroke.h>

#include <gtest/gtest.h>

using namespace olagarro;
using namespace olagarro::paint;

namespace
{

GrayAlpha16 at(const Canvas& canvas, int x, int y)
{
    return canvas.read(x, y, 1, 1).pixel<GrayAlpha16>(0, 0);
}

} // namespace

TEST(Stroke, AHardBrushPaintsItsWidthAlongTheLineAndNothingBeyond)
{
    Canvas canvas{PixelFormat::gray_alpha16, 100, 100};
    BrushSettings settings;
    settings.width = 10.0;
    settings.hardness = 1.0;
    settings.antialias = false;
    const Selection everything{100, 100};
    Stroke stroke{canvas, everything, {1.0, 1.0, 1.0, 1.0}, settings, StrokeMode::paint};
    stroke.line_to({20.5, 50.5});
    stroke.line_to({80.5, 50.5});
    EXPECT_EQ(at(canvas, 50, 50).value[0], 65535);
    EXPECT_EQ(at(canvas, 50, 54).value[0], 65535);
    EXPECT_EQ(at(canvas, 50, 56).alpha(), 0) << "past the radius";
    EXPECT_EQ(at(canvas, 14, 50).alpha(), 0) << "before the round end";
    EXPECT_EQ(at(canvas, 16, 50).value[0], 65535);
}

TEST(Stroke, ASeeThroughColourDoesNotBuildUpWhereTheStrokeCrossesItself)
{
    Canvas canvas{PixelFormat::rgba8, 64, 64};
    BrushSettings settings;
    settings.width = 6.0;
    settings.hardness = 1.0;
    const Selection everything{64, 64};
    Stroke stroke{canvas, everything, {1.0, 0.0, 0.0, 0.5}, settings, StrokeMode::paint};
    stroke.line_to({10.5, 30.5});
    stroke.line_to({50.5, 30.5});
    stroke.line_to({10.5, 30.5});
    stroke.line_to({30.5, 10.5});
    stroke.line_to({30.5, 50.5});
    EXPECT_EQ(canvas.read(30, 30, 1, 1).pixel<Rgba8>(0, 0).alpha(), 128);
}

TEST(Stroke, TheEraserTakesAlphaAwayAndOnlyTheTilesWrittenAreTold)
{
    Canvas canvas{PixelFormat::gray_alpha8, 600, 100};
    const Selection everything{600, 100};
    Image solid{PixelFormat::gray_alpha8, 600, 100};
    for(int y{}; y < 100; ++ y)
    {
        for(int x{}; x < 600; ++ x)
        {
            solid.set_pixel(x, y, GrayAlpha8{{200, 255}});
        }
    }
    canvas.write(solid, 0, 0);
    BrushSettings settings;
    settings.width = 4.0;
    settings.hardness = 1.0;
    settings.antialias = false;
    Stroke stroke{canvas, everything, {0, 0, 0, 1}, settings, StrokeMode::erase};
    stroke.line_to({300.5, 20.5});
    stroke.line_to({310.5, 20.5});
    EXPECT_EQ(canvas.read(305, 20, 1, 1).pixel<GrayAlpha8>(0, 0).alpha(), 0);
    EXPECT_EQ(canvas.read(305, 30, 1, 1).pixel<GrayAlpha8>(0, 0).alpha(), 255);
    EXPECT_EQ(stroke.take_changed(), (std::vector<TileIndex>{{1, 0}}));
    EXPECT_TRUE(stroke.take_changed().empty());
}

TEST(Stroke, PaintGoesOnlyWhereTheSelectionSelects)
{
    Canvas canvas{PixelFormat::gray_alpha8, 100, 100};
    Selection selection{100, 100};
    selection.add_rectangle({0, 0, 50, 100}, SelectionMode::replace);
    BrushSettings settings;
    settings.width = 4.0;
    settings.hardness = 1.0;
    settings.antialias = false;
    Stroke stroke{canvas, selection, {1, 1, 1, 1}, settings, StrokeMode::paint};
    stroke.line_to({10.5, 50.5});
    stroke.line_to({90.5, 50.5});
    EXPECT_EQ(canvas.read(40, 50, 1, 1).pixel<GrayAlpha8>(0, 0).alpha(), 255);
    EXPECT_EQ(canvas.read(60, 50, 1, 1).pixel<GrayAlpha8>(0, 0).alpha(), 0);
}

TEST(Stroke, LiftedPixelsLaidElsewhereKeepTheirSelectedShape)
{
    Canvas canvas{PixelFormat::rgba8, 100, 100};
    Image solid{PixelFormat::rgba8, 20, 20};
    for(int y{}; y < 20; ++ y)
    {
        for(int x{}; x < 20; ++ x)
        {
            solid.set_pixel(x, y, Rgba8{{static_cast<std::uint8_t>(x * 10), 0, 0, 255}});
        }
    }
    canvas.write(solid, 0, 0);
    Selection selection{100, 100};
    selection.add_rectangle({2, 2, 3, 3}, SelectionMode::replace);
    const auto lifted{lift_selected(canvas, selection)};
    EXPECT_EQ(lifted.x, 2);
    EXPECT_EQ(lifted.pixels.width, 3);
    static_cast<void>(erase_selected(canvas, selection));
    static_cast<void>(lay_over(canvas, lifted.pixels, 50, 60));
    EXPECT_EQ(canvas.read(3, 3, 1, 1).pixel<Rgba8>(0, 0).alpha(), 0) << "a hole where it was";
    EXPECT_EQ(canvas.read(51, 61, 1, 1).pixel<Rgba8>(0, 0).value, (std::array<std::uint8_t, 4>{30, 0, 0, 255}));
}

TEST(Stroke, TheCloneStampCopiesFromTheSourceAndRecolorReplacesOneColourOnly)
{
    Canvas canvas{PixelFormat::rgba8, 40, 20};
    Image picture{PixelFormat::rgba8, 40, 20};
    for(int y{}; y < 20; ++ y)
    {
        for(int x{}; x < 40; ++ x)
        {
            const auto blue{x < 20};
            picture.set_pixel(x, y, blue ? Rgba8{{0, 0, 255, 255}} : Rgba8{{255, 255, 0, 255}});
        }
    }
    canvas.write(picture, 0, 0);
    const Selection everything{40, 20};
    BrushSettings settings;
    settings.width = 4.0;
    settings.hardness = 1.0;
    settings.antialias = false;
    Stroke clone{canvas, everything, {}, settings, StrokeMode::clone, {-20, 0, {}, 0.0}};
    clone.line_to({30.5, 10.5});
    EXPECT_EQ(canvas.read(30, 10, 1, 1).pixel<Rgba8>(0, 0).value, (std::array<std::uint8_t, 4>{0, 0, 255, 255}))
        << "the blue from 20 pixels to the left";
    EXPECT_EQ(canvas.read(36, 10, 1, 1).pixel<Rgba8>(0, 0).value[0], 255) << "outside the brush";

    StrokeSource yellow;
    yellow.match = {1, 1, 0, 1};
    yellow.tolerance = 0.1;
    Stroke recolor{canvas, everything, {0, 1, 0, 1}, settings, StrokeMode::recolor, yellow};
    recolor.line_to({18.5, 15.5});
    recolor.line_to({24.5, 15.5});
    EXPECT_EQ(canvas.read(23, 15, 1, 1).pixel<Rgba8>(0, 0).value, (std::array<std::uint8_t, 4>{0, 255, 0, 255}));
    EXPECT_EQ(canvas.read(18, 15, 1, 1).pixel<Rgba8>(0, 0).value, (std::array<std::uint8_t, 4>{0, 0, 255, 255}))
        << "blue is not yellow, so it stays";
}

TEST(Stroke, AnExactStrokeWritesWholePixelsOnly)
{
    Canvas canvas{PixelFormat::gray_alpha8, 40, 40};
    Selection selection{40, 40};
    selection.add_ellipse(0, 0, 40, 40, SelectionMode::replace);
    BrushSettings settings;
    settings.width = 7.0;
    settings.exact = true;
    Stroke stroke{canvas, selection, {5 / 255.0, 5 / 255.0, 5 / 255.0, 1.0}, settings, StrokeMode::paint};
    stroke.line_to({2.5, 20.5});
    stroke.line_to({37.5, 20.5});
    const auto image{canvas.read(0, 0, 40, 40)};
    for(int y{}; y < 40; ++ y)
    {
        for(int x{}; x < 40; ++ x)
        {
            const auto pixel{image.pixel<GrayAlpha8>(x, y)};
            ASSERT_TRUE(pixel.alpha() == 0 || (pixel.alpha() == 255 && pixel.value[0] == 5)) << x << ", " << y;
        }
    }
}

TEST(Stroke, ALighterPressPaintsNarrowerAndTheWidthFollowsItAlongTheLine)
{
    Canvas canvas{PixelFormat::rgba8, 100, 100};
    BrushSettings settings;
    settings.width = 20.0;
    settings.hardness = 1.0;
    settings.antialias = false;
    const Selection everything{100, 100};
    Stroke stroke{canvas, everything, {1, 1, 1, 1}, settings, StrokeMode::paint};
    stroke.line_to({10.5, 50.5}, 0.2);
    stroke.line_to({90.5, 50.5}, 1.0);
    const auto alpha{[&](int x, int y) { return canvas.read(x, y, 1, 1).pixel<Rgba8>(0, 0).alpha(); }};
    EXPECT_EQ(alpha(10, 52), 255) << "inside the light end's two pixels";
    EXPECT_EQ(alpha(10, 54), 0) << "past the light end's radius";
    EXPECT_EQ(alpha(90, 58), 255) << "inside the heavy end's ten pixels";
    EXPECT_EQ(alpha(50, 56), 255) << "halfway, six pixels across";
    EXPECT_EQ(alpha(50, 58), 0);
}
