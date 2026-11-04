#include <paint/arc.h>
#include <paint/shapes.h>

#include <gtest/gtest.h>

#include <cmath>
#include <utility>

using namespace olagarro;
using namespace olagarro::paint;

namespace
{

constexpr auto pi{3.14159265358979323846};

std::uint8_t alpha_at(const Canvas& canvas, int x, int y)
{
    return canvas.read(x, y, 1, 1).pixel<Rgba8>(0, 0).alpha();
}

// The pointer `angle` round from (50, 50), 20 away: right is 0, down a quarter turn on, as the image's y goes.
Position round_at(double angle)
{
    return {50.0 + std::cos(angle) * 20.0, 50.0 + std::sin(angle) * 20.0};
}

} // namespace

TEST(Arc, AClickPutsTheCentreTheNextTheRadiusAndWhereItStartsAndTheThirdLeavesIt)
{
    ArcGesture arc;
    EXPECT_EQ(arc.step(), ArcGesture::Step::center);
    EXPECT_FALSE(arc.click({50, 50}, false));
    EXPECT_EQ(arc.step(), ArcGesture::Step::radius);
    arc.move({60, 50}, false);
    EXPECT_EQ(arc.pointer().x, 60) << "the radius's line goes to the pointer";
    EXPECT_FALSE(arc.click({50.2, 50}, false)) << "on the centre, no radius";
    EXPECT_EQ(arc.step(), ArcGesture::Step::radius);
    EXPECT_FALSE(arc.click(round_at(0.0), false));
    EXPECT_EQ(arc.step(), ArcGesture::Step::sweep);
    EXPECT_NEAR(arc.arc().radius, 20.0, 1e-9);
    EXPECT_NEAR(arc.arc().start, 0.0, 1e-9);
    EXPECT_TRUE(arc.click(round_at(pi / 2.0), false));
    EXPECT_NEAR(arc.arc().sweep, pi / 2.0, 1e-9);
}

TEST(Arc, ItTurnsTheWayThePointerGoesAndNoFurtherThanTheWholeCircle)
{
    ArcGesture arc;
    static_cast<void>(arc.click({50, 50}, false));
    static_cast<void>(arc.click(round_at(0.0), false));
    // Back the other way, past where atan2 jumps from a half turn to minus one.
    for(auto angle{0.0}; angle >= -3.0 * pi / 2.0; angle -= 0.1)
    {
        arc.move(round_at(angle), false);
    }
    EXPECT_NEAR(arc.arc().sweep, -3.0 * pi / 2.0 + std::fmod(3.0 * pi / 2.0, 0.1), 0.11);
    EXPECT_LT(arc.arc().sweep, -pi) << "past the half turn, still the same way";
    // Round and round: the whole circle at most, and it comes back as soon as the pointer does.
    for(auto angle{-3.0 * pi / 2.0}; angle >= -5.0 * pi; angle -= 0.1)
    {
        arc.move(round_at(angle), false);
    }
    EXPECT_NEAR(arc.arc().sweep, -2.0 * pi, 1e-9);
    arc.move(round_at(-5.0 * pi + 0.5), false);
    EXPECT_GT(arc.arc().sweep, -2.0 * pi + 0.3);
}

TEST(Arc, SnappingKeepsWhereItStartsAndHowFarItTurnsToStepsOf15Degrees)
{
    ArcGesture arc;
    static_cast<void>(arc.click({50, 50}, false));
    static_cast<void>(arc.click(round_at(0.2), true));
    EXPECT_NEAR(arc.arc().start, pi / 12.0, 1e-9) << "0.2 is nearest 15 degrees";
    arc.move(round_at(1.0), true);
    EXPECT_NEAR(arc.arc().sweep, pi / 4.0, 1e-9) << "turned 0.8, nearest 45 degrees";
}

TEST(Arc, AnArcIsDrawnOnlyWhereItTurnsWithRoundEnds)
{
    const Selection everything{100, 100};
    Canvas canvas{PixelFormat::rgba8, 100, 100};
    // A quarter from the right down, round (50.5, 50.5): pixels' middles are on the half.
    draw_arc(canvas, everything, {50.5, 50.5}, 20.0, 0.0, pi / 2.0, {1, 0, 0, 1}, 3.0, false);
    EXPECT_EQ(alpha_at(canvas, 70, 50), 255) << "its start";
    EXPECT_EQ(alpha_at(canvas, 64, 64), 255) << "between, 20 from the centre";
    EXPECT_EQ(alpha_at(canvas, 50, 70), 255) << "its end";
    EXPECT_EQ(alpha_at(canvas, 30, 50), 0) << "the other side";
    EXPECT_EQ(alpha_at(canvas, 64, 36), 0) << "turned the other way";
    EXPECT_EQ(alpha_at(canvas, 70, 49), 255)
        << "the round end, half the width round the start, reaches a little past it";
    EXPECT_EQ(alpha_at(canvas, 70, 47), 0);
    EXPECT_EQ(alpha_at(canvas, 50, 50), 0) << "nothing at the centre";

    Canvas whole{PixelFormat::rgba8, 100, 100};
    draw_arc(whole, everything, {50.5, 50.5}, 20.0, 1.0, -2.0 * pi, {1, 0, 0, 1}, 3.0, false);
    for(const auto& [x, y] : {std::pair{70, 50}, {50, 70}, {30, 50}, {50, 30}})
    {
        EXPECT_EQ(alpha_at(whole, x, y), 255) << x << ", " << y << ": a whole turn is the circle";
    }
}

TEST(Arc, SnappingKeepsTheRadiusLineToStepsOf15DegreesToo)
{
    ArcGesture arc;
    static_cast<void>(arc.click({50, 50}, false));
    arc.move(round_at(0.2), true);
    const auto pointer{arc.pointer()};
    EXPECT_NEAR(std::atan2(pointer.y - 50.0, pointer.x - 50.0), pi / 12.0, 1e-9) << "the line at 15 degrees";
    EXPECT_NEAR(std::hypot(pointer.x - 50.0, pointer.y - 50.0), 20.0, 1e-9) << "as long as the pointer is far";
    arc.move(round_at(0.2), false);
    EXPECT_NEAR(std::atan2(arc.pointer().y - 50.0, arc.pointer().x - 50.0), 0.2, 1e-9) << "and free without";
}
