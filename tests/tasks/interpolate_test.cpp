#include <tasks/interpolate.h>
#include <tasks/repeat.h>
#include <tasks/taskexecutor.h>

#include <gtest/gtest.h>

#include <vector>

using namespace olagarro;
using namespace olagarro::tasks;

namespace
{

struct Point
{
    float x;
    float y;

    Point operator+(const Point& other) const { return {x + other.x, y + other.y}; }
    Point operator-(const Point& other) const { return {x - other.x, y - other.y}; }
    Point operator*(float factor) const { return {x * factor, y * factor}; }
};

TEST(InterpolateTests, TheFirstUpdateAlreadyMoves)
{
    std::vector<float> applied;

    TaskExecutor executor;
    executor.add_task(interpolate(1.0f, [&](float u) { applied.push_back(u); }));

    executor.update(0.25f);
    ASSERT_EQ(applied.size(), 1u);
    EXPECT_FLOAT_EQ(applied[0], 0.25f);
}

TEST(InterpolateTests, ItFinishesOnTheUpdateTheTimeRunsOutApplyingExactlyOne)
{
    std::vector<float> applied;

    TaskExecutor executor;
    executor.add_task(interpolate(1.0f, [&](float u) { applied.push_back(u); }));

    executor.update(0.5f);
    EXPECT_FALSE(executor.is_empty());

    executor.update(0.5f);
    EXPECT_TRUE(executor.is_empty());
    ASSERT_EQ(applied.size(), 2u);
    EXPECT_FLOAT_EQ(applied[0], 0.5f);
    EXPECT_EQ(applied[1], 1.0f);
}

TEST(InterpolateTests, AZeroDurationFinishesOnTheFirstUpdate)
{
    std::vector<float> applied;

    TaskExecutor executor;
    executor.add_task(interpolate(0.0f, [&](float u) { applied.push_back(u); }));

    executor.update(0.0f);
    EXPECT_TRUE(executor.is_empty());
    ASSERT_EQ(applied.size(), 1u);
    EXPECT_EQ(applied[0], 1.0f);
}

TEST(InterpolateTests, TheCurveIsAppliedButNotToTheLastUpdate)
{
    std::vector<float> applied;

    TaskExecutor executor;
    executor.add_task(interpolate(1.0f, [&](float u) { applied.push_back(u); }, [](float) { return 0.75f; }));

    executor.update(0.5f);
    executor.update(0.5f);
    ASSERT_EQ(applied.size(), 2u);
    EXPECT_EQ(applied[0], 0.75f);
    EXPECT_EQ(applied[1], 1.0f);
}

TEST(InterpolateTests, TheCurvesGoFromZeroToOne)
{
    for(const auto curve : {curves::linear, curves::ease_in, curves::ease_out, curves::ease_in_out, curves::smoothstep})
    {
        EXPECT_FLOAT_EQ(curve(0.0f), 0.0f);
        EXPECT_FLOAT_EQ(curve(1.0f), 1.0f);
    }

    EXPECT_FLOAT_EQ(curves::linear(0.25f), 0.25f);
    EXPECT_FLOAT_EQ(curves::ease_in(0.5f), 0.25f);
    EXPECT_FLOAT_EQ(curves::ease_out(0.5f), 0.75f);
    EXPECT_FLOAT_EQ(curves::ease_in_out(0.25f), 0.125f);
    EXPECT_FLOAT_EQ(curves::ease_in_out(0.75f), 0.875f);
    EXPECT_FLOAT_EQ(curves::smoothstep(0.5f), 0.5f);
}

// 0.3 + (0.1 - 0.3) * 1 is not 0.1 in floats.
TEST(InterpolateTests, InterpolateValueEndsExactlyOnTheTarget)
{
    float value{0.0f};

    TaskExecutor executor;
    executor.add_task(interpolate_value(0.3f, 0.1f, 1.0f, [&](float set) { value = set; }));

    executor.update(0.5f);
    EXPECT_FLOAT_EQ(value, 0.2f);

    executor.update(0.5f);
    EXPECT_EQ(value, 0.1f);
}

TEST(InterpolateTests, InterpolateValueTakesAnyTypeWithTheArithmetic)
{
    Point point{};

    TaskExecutor executor;
    executor.add_task(interpolate_value(
        Point{0.0f, 0.0f}, Point{2.0f, 4.0f}, 2.0f, [&](Point set) { point = set; }, curves::ease_in));

    executor.update(1.0f);
    EXPECT_FLOAT_EQ(point.x, 0.5f);
    EXPECT_FLOAT_EQ(point.y, 1.0f);

    executor.update(1.0f);
    EXPECT_EQ(point.x, 2.0f);
    EXPECT_EQ(point.y, 4.0f);
}

TEST(InterpolateTests, AProgressMonitorShowsHowFarItIsAndStaysValidAfterTheTask)
{
    auto [task, progress] = progress_monitor(2.0f);
    EXPECT_EQ(progress.value(), 0.0f);

    TaskExecutor executor;
    executor.add_task(std::move(task));

    executor.update(0.5f);
    EXPECT_FLOAT_EQ(progress.value(), 0.25f);

    executor.update(2.0f);
    EXPECT_TRUE(executor.is_empty());
    EXPECT_EQ(progress.value(), 1.0f);
}

TEST(InterpolateTests, AProgressMonitorAsksForItsDurationOnEveryStart)
{
    int asked{0};
    auto [task, progress] = progress_monitor(
        [&]
        {
            ++ asked;
            return static_cast<float>(asked);
        });

    TaskExecutor executor;
    executor.add_task(repeat_forever(std::move(task)));

    executor.update(0.5f);
    EXPECT_EQ(asked, 1);
    EXPECT_FLOAT_EQ(progress.value(), 0.5f);

    executor.update(0.5f);
    EXPECT_EQ(progress.value(), 1.0f);

    // Started again, with a duration of 2 now, and back from 0.
    executor.update(0.5f);
    EXPECT_EQ(asked, 2);
    EXPECT_FLOAT_EQ(progress.value(), 0.25f);
}

} // namespace
