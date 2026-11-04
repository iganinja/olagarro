#include <paint/fill.h>

#include <gtest/gtest.h>

using namespace olagarro;
using namespace olagarro::paint;

namespace
{

// A white image with a black ring around (5..14, 5..14), its inside light grey.
Canvas ringed()
{
    Canvas canvas{PixelFormat::gray_alpha16, 20, 20};
    Image image{PixelFormat::gray_alpha16, 20, 20};
    for(int y{}; y < 20; ++ y)
    {
        for(int x{}; x < 20; ++ x)
        {
            const auto ring{(x == 5 || x == 14 || y == 5 || y == 14) && x >= 5 && x <= 14 && y >= 5 && y <= 14};
            const auto inside{x > 5 && x < 14 && y > 5 && y < 14};
            const std::uint16_t value{static_cast<std::uint16_t>(ring ? 0 : inside ? 60000 : 65535)};
            image.set_pixel(x, y, GrayAlpha16{{value, 65535}});
        }
    }
    canvas.write(image, 0, 0);
    return canvas;
}

std::uint16_t at(const Canvas& canvas, int x, int y)
{
    return canvas.read(x, y, 1, 1).pixel<GrayAlpha16>(0, 0).value[0];
}

} // namespace

TEST(Fill, AContiguousFillStopsAtWhatIsTooDifferent)
{
    auto canvas{ringed()};
    const Selection everything{20, 20};
    FillSettings settings;
    settings.tolerance = 0.0;
    bucket_fill(canvas, everything, 0, 0, {0.5, 0.5, 0.5, 1.0}, settings);
    EXPECT_NEAR(at(canvas, 0, 0), 32768, 1);
    EXPECT_EQ(at(canvas, 5, 5), 0) << "the ring";
    EXPECT_EQ(at(canvas, 9, 9), 60000) << "inside the ring, not joined";
}

TEST(Fill, ToleranceTakesSimilarPixelsAndGlobalTakesThemAnywhere)
{
    auto canvas{ringed()};
    const Selection everything{20, 20};
    FillSettings settings;
    settings.tolerance = 0.1;
    settings.global = true;
    bucket_fill(canvas, everything, 0, 0, {0.0, 0.0, 0.0, 1.0}, settings);
    EXPECT_EQ(at(canvas, 9, 9), 0) << "light grey is within a tenth of white";
    EXPECT_EQ(at(canvas, 19, 19), 0);
}

TEST(Fill, AFillStaysInsideTheSelection)
{
    auto canvas{ringed()};
    Selection selection{20, 20};
    selection.add_rectangle({0, 0, 3, 20}, SelectionMode::replace);
    bucket_fill(canvas, selection, 0, 0, {0.0, 0.0, 0.0, 1.0}, {});
    EXPECT_EQ(at(canvas, 1, 10), 0);
    EXPECT_EQ(at(canvas, 4, 10), 65535);
}
