#include <paint/selection.h>

#include <gtest/gtest.h>

using namespace olagarro;
using namespace olagarro::paint;

TEST(Selection, NothingSelectedIsTheWholeImage)
{
    Selection selection{100, 50};
    EXPECT_FALSE(selection.active());
    EXPECT_EQ(selection.at(10, 10), 255);
    EXPECT_EQ(selection.bounds().width, 100);
}

TEST(Selection, RectanglesCombineByMode)
{
    Selection selection{100, 100};
    selection.add_rectangle({10, 10, 20, 20}, SelectionMode::replace);
    EXPECT_EQ(selection.at(15, 15), 255);
    EXPECT_EQ(selection.at(35, 15), 0);
    selection.add_rectangle({25, 10, 20, 20}, SelectionMode::add);
    EXPECT_EQ(selection.at(35, 15), 255);
    selection.add_rectangle({20, 0, 10, 100}, SelectionMode::subtract);
    EXPECT_EQ(selection.at(25, 15), 0);
    EXPECT_EQ(selection.at(15, 15), 255);
    selection.add_rectangle({0, 0, 18, 100}, SelectionMode::intersect);
    EXPECT_EQ(selection.at(15, 15), 255);
    EXPECT_EQ(selection.at(35, 15), 0) << "outside the intersecting rectangle";
    const auto area{selection.bounds()};
    EXPECT_EQ(area.x, 10);
    EXPECT_EQ(area.width, 8);
    selection.add_rectangle({12, 12, 2, 2}, SelectionMode::invert);
    EXPECT_EQ(selection.at(12, 12), 0);
    EXPECT_EQ(selection.at(11, 11), 255);
}

TEST(Selection, AnEllipseHasASoftEdgeAndFillsItsMiddle)
{
    Selection selection{100, 100};
    selection.add_ellipse(10, 10, 90, 50, SelectionMode::replace);
    EXPECT_EQ(selection.at(50, 30), 255);
    EXPECT_EQ(selection.at(11, 11), 0) << "the corner of its rectangle is outside it";
    auto partial{false};
    for(int x{5}; x < 15; ++ x)
    {
        const auto value{selection.at(x, 30)};
        partial = partial || (value > 0 && value < 255);
    }
    EXPECT_TRUE(partial) << "the edge is antialiased";
}

TEST(Selection, InvertingAllGivesNothingAndMovingKeepsTheShape)
{
    Selection selection{64, 64};
    selection.select_all();
    selection.invert();
    EXPECT_TRUE(selection.active());
    EXPECT_EQ(selection.at(5, 5), 0);
    selection.add_rectangle({0, 0, 4, 4}, SelectionMode::replace);
    selection.offset(10, 20);
    EXPECT_EQ(selection.at(1, 1), 0);
    EXPECT_EQ(selection.at(11, 21), 255);
    EXPECT_EQ(selection.bounds().x, 10);
}

TEST(Selection, ALassoSelectsInsideItsOutlineWithASoftEdge)
{
    Selection selection{100, 100};
    // A triangle with a slanting side.
    selection.add_polygon({{10, 10}, {60, 10}, {10, 60}}, SelectionMode::replace);
    EXPECT_EQ(selection.at(15, 15), 255);
    EXPECT_EQ(selection.at(55, 55), 0);
    auto partial{false};
    for(int x{20}; x < 50; ++ x)
    {
        const auto value{selection.at(x, 69 - x)};
        partial = partial || (value > 0 && value < 255);
    }
    EXPECT_TRUE(partial) << "the slanting side is antialiased";
}

TEST(Selection, AMagicWandsRegionCombinesLikeAnyShape)
{
    Selection selection{4, 1};
    selection.add_rectangle({0, 0, 2, 1}, SelectionMode::replace);
    selection.add_coverage({0, 255, 255, 0}, SelectionMode::add);
    EXPECT_EQ(selection.at(0, 0), 255);
    EXPECT_EQ(selection.at(2, 0), 255);
    EXPECT_EQ(selection.at(3, 0), 0);
}

TEST(Selection, APixelHalfInsideTheEdgeIsHalfSelected)
{
    Selection lasso{300, 100};
    // Its left side runs down the middle of column 10, its right down that of column 270, in the second tile.
    lasso.add_polygon({{10.5, 10}, {270.5, 10}, {270.5, 60}, {10.5, 60}}, SelectionMode::replace);
    EXPECT_EQ(lasso.at(9, 30), 0);
    EXPECT_EQ(lasso.at(10, 30), 128);
    EXPECT_EQ(lasso.at(11, 30), 255);
    EXPECT_EQ(lasso.at(270, 30), 128);
    EXPECT_EQ(lasso.at(271, 30), 0);
    Selection ellipse{100, 100};
    // Its leftmost point is the middle of pixel 10, 30, where the edge passes through.
    ellipse.add_ellipse(10.5, 11, 89.5, 50, SelectionMode::replace);
    EXPECT_EQ(ellipse.at(10, 30), 128);
    EXPECT_EQ(ellipse.at(9, 30), 0);
    EXPECT_EQ(ellipse.at(12, 30), 255);
}
