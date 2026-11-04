#include <paint/color_math.h>

#include <gtest/gtest.h>

using namespace olagarro;
using namespace olagarro::paint;

TEST(ColorMath, ColoursGoToHsvAndBack)
{
    for(const auto& color : {Color{1, 0, 0, 1}, Color{0.2, 0.6, 0.3, 1}, Color{0.5, 0.5, 0.5, 1},
                             Color{0.1, 0.2, 0.9, 1}, Color{0.9, 0.8, 0.1, 1}, Color{0.7, 0.1, 0.6, 1}})
    {
        const auto back{from_hsv(to_hsv(color))};
        EXPECT_NEAR(back.red, color.red, 1e-12);
        EXPECT_NEAR(back.green, color.green, 1e-12);
        EXPECT_NEAR(back.blue, color.blue, 1e-12);
    }
    EXPECT_DOUBLE_EQ(to_hsv({0, 1, 0, 1}).hue, 120.0);
    EXPECT_DOUBLE_EQ(to_hsv({0.5, 0.5, 0.5, 1}).saturation, 0.0);
}
