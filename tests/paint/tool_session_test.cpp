#include <paint/tool_session.h>

#include <gtest/gtest.h>

using namespace olagarro;
using namespace olagarro::paint;

namespace
{

Rgba8 at(const Canvas& canvas, int x, int y)
{
    return canvas.read(x, y, 1, 1).pixel<Rgba8>(0, 0);
}

// Whether two canvases hold the same pixels, tile for tile.
bool same_tiles(const Canvas& a, const Canvas& b)
{
    if(a.tiles().size() != b.tiles().size())
    {
        return false;
    }
    for(const auto& [index, tile] : a.tiles())
    {
        const auto other{b.tile(index)};
        if(other == nullptr)
        {
            return false;
        }
        for(int y{}; y < tile_size; ++ y)
        {
            if(!std::equal(tile->row(y), tile->row(y) + tile->row_bytes(), other->row(y)))
            {
                return false;
            }
        }
    }
    return true;
}

ToolInput input(double x, double y)
{
    ToolInput result;
    result.at = {x, y};
    return result;
}

// A square of `color` with its top left corner at x, y.
void square(Canvas& canvas, int x, int y, int side, Rgba8 color)
{
    Image block{PixelFormat::rgba8, side, side};
    for(int row{}; row < side; ++ row)
    {
        for(int column{}; column < side; ++ column)
        {
            block.set_pixel(column, row, color);
        }
    }
    canvas.write(block, x, y);
}

} // namespace

TEST(ToolSession, ABrushStrokeFromPressToReleasePaintsAsTheStrokeDoesAndItsStepUndoesIt)
{
    Canvas canvas{PixelFormat::rgba8, 100, 100};
    Selection selection{100, 100};
    ToolSession session{canvas, selection};
    session.options().brush = {10.0, 1.0, false, false};
    session.options().primary = {1.0, 1.0, 1.0, 1.0};
    const auto before{canvas};
    session.press(input(20.5, 50.5));
    EXPECT_TRUE(session.busy());
    session.drag(input(50.5, 50.5));
    session.release(input(80.5, 50.5));
    EXPECT_FALSE(session.busy());
    // As Stroke's own test paints it.
    EXPECT_EQ(at(canvas, 50, 50).value, (std::array<std::uint8_t, 4>{255, 255, 255, 255}));
    EXPECT_EQ(at(canvas, 50, 54).alpha(), 255);
    EXPECT_EQ(at(canvas, 50, 56).alpha(), 0) << "past the radius";
    EXPECT_EQ(at(canvas, 14, 50).alpha(), 0) << "before the round end";
    EXPECT_EQ(at(canvas, 16, 50).alpha(), 255);

    const auto steps{session.take_steps()};
    ASSERT_EQ(steps.size(), 1u);
    EXPECT_EQ(steps[0].tool, Tool::brush);
    EXPECT_FALSE(steps[0].selection) << "a stroke leaves the selection alone";
    EXPECT_TRUE(session.take_steps().empty());
    const auto painted{canvas};
    steps[0].change.undo(canvas);
    EXPECT_TRUE(same_tiles(canvas, before));
    steps[0].change.redo(canvas);
    EXPECT_TRUE(same_tiles(canvas, painted));
}

TEST(ToolSession, AFastDragPaintsALineNotDots)
{
    Canvas canvas{PixelFormat::rgba8, 100, 100};
    Selection selection{100, 100};
    ToolSession session{canvas, selection};
    session.options().brush = {2.0, 1.0, false, false};
    session.press(input(10.5, 50.5));
    session.drag(input(90.5, 50.5));
    session.release(input(90.5, 50.5));
    for(int x{10}; x <= 90; ++ x)
    {
        ASSERT_EQ(at(canvas, x, 50).alpha(), 255) << x;
    }
    EXPECT_EQ(at(canvas, 50, 53).alpha(), 0);
    const auto changed{session.take_changed()};
    EXPECT_EQ(changed, (std::vector<TileIndex>{{0, 0}}));
    EXPECT_TRUE(session.take_changed().empty());
}

TEST(ToolSession, ThePencilIsOnePixelAndTheEraserTakesAway)
{
    Canvas canvas{PixelFormat::rgba8, 40, 40};
    Selection selection{40, 40};
    square(canvas, 0, 0, 40, Rgba8{{0, 0, 255, 255}});
    ToolSession session{canvas, selection};
    session.set_tool(Tool::pencil);
    session.options().primary = {1.0, 0.0, 0.0, 1.0};
    session.press(input(5.5, 10.5));
    session.release(input(30.5, 10.5));
    EXPECT_EQ(at(canvas, 20, 10).value, (std::array<std::uint8_t, 4>{255, 0, 0, 255}));
    EXPECT_EQ(at(canvas, 20, 11).value, (std::array<std::uint8_t, 4>{0, 0, 255, 255}));
    session.set_tool(Tool::eraser);
    session.options().brush = {4.0, 1.0, false, false};
    session.press(input(5.5, 30.5));
    session.release(input(30.5, 30.5));
    EXPECT_EQ(at(canvas, 20, 30).alpha(), 0);
    EXPECT_EQ(session.take_steps().size(), 2u);
}

TEST(ToolSession, AFillStopsAtItsTolerance)
{
    // Three bands of grey, 0.5, 0.55 and 0.8, side by side.
    Canvas canvas{PixelFormat::rgba8, 30, 10};
    Selection selection{30, 10};
    square(canvas, 0, 0, 10, Rgba8{{128, 128, 128, 255}});
    square(canvas, 10, 0, 10, Rgba8{{140, 140, 140, 255}});
    square(canvas, 20, 0, 10, Rgba8{{204, 204, 204, 255}});
    ToolSession session{canvas, selection};
    session.set_tool(Tool::fill);
    session.options().primary = {1.0, 0.0, 0.0, 1.0};
    session.options().fill.tolerance = 0.05;
    session.press(input(2.5, 2.5));
    session.release(input(2.5, 2.5));
    EXPECT_EQ(at(canvas, 15, 5).value, (std::array<std::uint8_t, 4>{255, 0, 0, 255}))
        << "0.55 is within the tolerance of 0.5";
    EXPECT_EQ(at(canvas, 25, 5).value, (std::array<std::uint8_t, 4>{204, 204, 204, 255})) << "0.8 is not";
    EXPECT_EQ(session.take_steps().size(), 1u);
}

TEST(ToolSession, AShapeIsShownBeforeItIsLetGoAndDrawnOnRelease)
{
    Canvas canvas{PixelFormat::rgba8, 100, 100};
    Selection selection{100, 100};
    ToolSession session{canvas, selection};
    session.set_tool(Tool::shapes);
    session.options().shape = {ShapeKind::rectangle, ShapeStyle::filled};
    session.options().primary = {0.0, 1.0, 0.0, 1.0};
    session.press(input(10, 10));
    session.drag(input(50, 40));
    EXPECT_TRUE(canvas.tiles().empty()) << "nothing drawn yet";
    const auto shown{session.overlay()};
    ASSERT_TRUE(shown.preview);
    EXPECT_EQ(at(*shown.preview, 30, 25).value, (std::array<std::uint8_t, 4>{0, 255, 0, 255}));
    EXPECT_TRUE(session.take_steps().empty());
    session.drag(input(60, 70));
    session.release(input(60, 70));
    EXPECT_FALSE(session.overlay().preview);
    EXPECT_EQ(at(canvas, 55, 65).value, (std::array<std::uint8_t, 4>{0, 255, 0, 255})) << "where it was let go";
    EXPECT_EQ(at(canvas, 70, 80).alpha(), 0);
    EXPECT_EQ(session.take_steps().size(), 1u);
}

TEST(ToolSession, ALineWaitsToBeBentByItsHandlesAndIsOneStep)
{
    Canvas canvas{PixelFormat::rgba8, 100, 100};
    Selection selection{100, 100};
    ToolSession session{canvas, selection};
    session.set_tool(Tool::line);
    session.options().brush = {3.0, 1.0, false, false};
    session.press(input(10.5, 50.5));
    session.release(input(90.5, 50.5));
    EXPECT_EQ(at(canvas, 50, 50).alpha(), 255) << "drawn on release";
    EXPECT_TRUE(session.busy()) << "still to bend";
    EXPECT_EQ(session.overlay().handles.size(), 4u);
    EXPECT_TRUE(session.take_steps().empty());
    // The handle a third along, pulled up: the line bends through it.
    session.press(input(37.2, 50.5));
    session.drag(input(37.2, 20.5));
    session.release(input(37.2, 20.5));
    EXPECT_EQ(at(canvas, 37, 20).alpha(), 255);
    EXPECT_EQ(at(canvas, 50, 50).alpha(), 0) << "no longer straight";
    session.finish();
    EXPECT_FALSE(session.busy());
    EXPECT_EQ(session.take_steps().size(), 1u);
}

TEST(ToolSession, AnArcTakesThreeClicksAndEscLeavesItOut)
{
    Canvas canvas{PixelFormat::rgba8, 100, 100};
    Selection selection{100, 100};
    ToolSession session{canvas, selection};
    session.set_tool(Tool::arc);
    session.options().brush = {3.0, 1.0, false, false};
    session.press(input(50.5, 50.5));
    session.release(input(50.5, 50.5));
    session.hover(input(70.5, 50.5));
    EXPECT_EQ(session.overlay().lines.size(), 1u) << "the radius";
    session.press(input(70.5, 50.5));
    session.hover(input(50.5, 70.5));
    EXPECT_TRUE(canvas.tiles().empty()) << "shown, not drawn";
    ASSERT_TRUE(session.overlay().preview);
    session.cancel();
    EXPECT_TRUE(canvas.tiles().empty());
    EXPECT_TRUE(session.take_steps().empty());

    session.press(input(50.5, 50.5));
    session.press(input(70.5, 50.5));
    session.hover(input(50.5, 70.5));
    session.press(input(50.5, 70.5));
    EXPECT_FALSE(session.busy());
    EXPECT_EQ(at(canvas, 64, 64).alpha(), 255) << "a quarter of the circle, 20 from the centre";
    EXPECT_EQ(at(canvas, 30, 50).alpha(), 0);
    EXPECT_EQ(session.take_steps().size(), 1u);
}

TEST(ToolSession, TheSelectionToolsSelectAndTheirOutlineIsShown)
{
    Canvas canvas{PixelFormat::rgba8, 100, 100};
    Selection selection{100, 100};
    ToolSession session{canvas, selection};
    session.set_tool(Tool::rectangle_select);
    session.press(input(10.5, 10.5));
    session.drag(input(29.5, 19.5));
    session.release(input(29.5, 19.5));
    EXPECT_EQ(selection.at(20, 15), 255);
    EXPECT_EQ(selection.at(35, 15), 0);
    // A rectangle's outline is its four sides.
    EXPECT_EQ(session.overlay().lines.size(), 4u);
    const auto steps{session.take_steps()};
    ASSERT_EQ(steps.size(), 1u);
    ASSERT_TRUE(steps[0].selection);
    EXPECT_FALSE(steps[0].selection->before) << "nothing was selected";
    EXPECT_TRUE(steps[0].selection->after);

    // Ctrl adds.
    session.set_tool(Tool::ellipse_select);
    auto add{input(50, 50)};
    add.control = true;
    session.press(add);
    add.at = {90, 90};
    session.release(add);
    EXPECT_EQ(selection.at(70, 70), 255);
    EXPECT_EQ(selection.at(20, 15), 255);

    // A click without dragging selects nothing.
    session.set_tool(Tool::rectangle_select);
    session.press(input(5.5, 5.5));
    session.release(input(5.5, 5.5));
    EXPECT_FALSE(selection.active());
}

TEST(ToolSession, AMagicWandSelectsWhatAFillWouldReach)
{
    Canvas canvas{PixelFormat::rgba8, 40, 20};
    Selection selection{40, 20};
    square(canvas, 0, 0, 20, Rgba8{{255, 0, 0, 255}});
    ToolSession session{canvas, selection};
    session.set_tool(Tool::magic_wand);
    session.options().fill.tolerance = 0.1;
    session.press(input(5.5, 5.5));
    session.release(input(5.5, 5.5));
    EXPECT_EQ(selection.at(10, 10), 255);
    EXPECT_EQ(selection.at(30, 10), 0);
    EXPECT_TRUE(session.take_steps().at(0).selection);
}

TEST(ToolSession, ThePickerTakesTheColourUnderItForTheButtonUsed)
{
    Canvas canvas{PixelFormat::rgba8, 20, 20};
    Selection selection{20, 20};
    square(canvas, 0, 0, 10, Rgba8{{255, 0, 0, 255}});
    ToolSession session{canvas, selection};
    session.set_tool(Tool::color_picker);
    session.press(input(5.5, 5.5));
    session.release(input(5.5, 5.5));
    EXPECT_EQ(session.options().primary, (Color{1.0, 0.0, 0.0, 1.0}));
    auto right{input(15.5, 15.5)};
    right.secondary = true;
    session.press(right);
    session.release(right);
    EXPECT_EQ(session.options().secondary, (Color{0.0, 0.0, 0.0, 0.0}));
    EXPECT_TRUE(session.take_steps().empty());
}

TEST(ToolSession, ALassoThenAMoveMovesOnlyWhatWasSelected)
{
    Canvas canvas{PixelFormat::rgba8, 100, 100};
    Selection selection{100, 100};
    square(canvas, 10, 10, 10, Rgba8{{255, 0, 0, 255}});
    square(canvas, 10, 60, 10, Rgba8{{0, 0, 255, 255}});
    ToolSession session{canvas, selection};
    session.set_tool(Tool::lasso_select);
    session.press(input(5, 5));
    session.drag(input(25, 5));
    session.drag(input(25, 25));
    session.release(input(5, 25));
    EXPECT_EQ(selection.at(15, 15), 255);
    EXPECT_EQ(selection.at(15, 65), 0);

    session.set_tool(Tool::move_pixels);
    EXPECT_EQ(session.overlay().handles.size(), 8u) << "the selection's corners and edges";
    session.press(input(15, 15));
    session.drag(input(30, 15));
    session.release(input(45, 15));
    EXPECT_EQ(at(canvas, 45, 15).value, (std::array<std::uint8_t, 4>{255, 0, 0, 255})) << "moved 30 to the right";
    EXPECT_EQ(at(canvas, 15, 15).alpha(), 0) << "a hole where it was";
    EXPECT_EQ(at(canvas, 15, 65).value, (std::array<std::uint8_t, 4>{0, 0, 255, 255})) << "not selected, not moved";
    EXPECT_EQ(at(canvas, 45, 65).alpha(), 0);
    EXPECT_EQ(selection.at(45, 15), 255) << "the selection went with it";
    EXPECT_EQ(selection.at(15, 15), 0);
    EXPECT_TRUE(session.busy()) << "floating until put down";

    const auto steps{session.take_steps()};
    ASSERT_EQ(steps.size(), 2u);
    EXPECT_EQ(steps[0].tool, Tool::lasso_select);
    EXPECT_TRUE(steps[0].change.empty());
    EXPECT_EQ(steps[1].tool, Tool::move_pixels);
    EXPECT_FALSE(steps[1].change.empty());
    session.set_tool(Tool::brush);
    EXPECT_FALSE(session.busy());
}

TEST(ToolSession, MoveSelectionMovesTheOutlineOnly)
{
    Canvas canvas{PixelFormat::rgba8, 100, 100};
    Selection selection{100, 100};
    square(canvas, 10, 10, 10, Rgba8{{255, 0, 0, 255}});
    selection.add_rectangle({10, 10, 10, 10}, SelectionMode::replace);
    ToolSession session{canvas, selection};
    session.set_tool(Tool::move_selection);
    session.press(input(15, 15));
    session.release(input(35, 15));
    EXPECT_EQ(selection.at(35, 15), 255);
    EXPECT_EQ(selection.at(15, 15), 0);
    EXPECT_EQ(at(canvas, 15, 15).alpha(), 255) << "the pixels stay";
}

TEST(ToolSession, TheOutlineOfNothingSelectedIsNothing)
{
    Selection selection{10, 10};
    EXPECT_TRUE(selection_outline(selection).empty());
    selection.add_rectangle({2, 3, 4, 5}, SelectionMode::replace);
    const auto lines{selection_outline(selection)};
    ASSERT_EQ(lines.size(), 4u);
    EXPECT_EQ(lines[0].first.x, 2.0);
    EXPECT_EQ(lines[0].first.y, 3.0);
    EXPECT_EQ(lines[0].second.x, 6.0);
}

TEST(ToolSession, ToolsHaveTheirIds)
{
    for(const auto tool : all_tools())
    {
        EXPECT_EQ(tool_from_id(tool_id(tool)), tool);
    }
    EXPECT_EQ(tool_id(Tool::fill), "bucket");
    EXPECT_FALSE(tool_from_id("text"));
}
