#include <oui/diff_view.h>
#include <oui/event.h>
#include <oui/label.h>
#include <oui/layout.h>
#include <oui/list.h>
#include <oui/markdown_view.h>
#include <oui/scroll_view.h>
#include <oui/scrollbar.h>

#include <gtest/gtest.h>

#include <string>

using namespace olagarro;

namespace
{

// A press, a drag to, and a release at the cells given.
oui::MouseEvent press(int x, int y)
{
    return oui::MouseEvent{x, y, true};
}

oui::MouseEvent drag(int x, int y)
{
    oui::MouseEvent event{x, y, false};
    event.dragging = true;
    return event;
}

oui::MouseEvent release(int x, int y)
{
    oui::MouseEvent event{x, y, false};
    event.released = true;
    return event;
}

TEST(OuiScrollbarTests, TheThumbShowsHowMuchShowsAndWhere)
{
    // Ten lines of a hundred on a track of ten: a cell long, at the top, then at the bottom.
    EXPECT_EQ(oui::scrollbar_thumb(10, 10, 100, 0, 90).start, 0);
    EXPECT_EQ(oui::scrollbar_thumb(10, 10, 100, 0, 90).length, 1);
    EXPECT_EQ(oui::scrollbar_thumb(10, 10, 100, 90, 90).start, 9);
    // Half of it: half the track.
    EXPECT_EQ(oui::scrollbar_thumb(10, 10, 20, 0, 10).length, 5);
}

TEST(OuiScrollbarTests, APressBesideTheThumbMovesAPageAndTheThumbIsDragged)
{
    oui::ScrollbarDrag scrollbar;
    const oui::Rect track{9, 0, 1, 10};
    // Below the thumb, a page on; above it, a page back.
    EXPECT_EQ(scrollbar.handle(press(9, 6), track, 10, 100, 0, 90, 9), std::optional<int>{9});
    EXPECT_EQ(scrollbar.handle(press(9, 0), track, 10, 100, 40, 90, 9), std::optional<int>{31});
    // Off the track, nothing of the scrollbar's.
    EXPECT_EQ(scrollbar.handle(press(3, 5), track, 10, 100, 0, 90, 9), std::nullopt);
    // The thumb, held, follows the pointer even off the track, until let go.
    EXPECT_EQ(scrollbar.handle(press(9, 0), track, 10, 100, 0, 90, 9), std::optional<int>{0});
    EXPECT_EQ(scrollbar.handle(drag(4, 9), track, 10, 100, 0, 90, 9), std::optional<int>{90});
    EXPECT_EQ(scrollbar.handle(release(4, 9), track, 10, 100, 90, 90, 9), std::optional<int>{90});
    EXPECT_EQ(scrollbar.handle(drag(4, 0), track, 10, 100, 90, 90, 9), std::nullopt) << "let go";
}

TEST(OuiScrollbarTests, AListAndAMarkdownViewAnswerTheirScrollbars)
{
    auto list{oui::list()};
    for(int item{}; item < 100; ++ item)
    {
        list->add_item("item " + std::to_string(item));
    }
    list->arrange({0, 0, 20, 10});
    EXPECT_TRUE(list->handle_event(press(19, 0)));
    EXPECT_TRUE(list->handle_event(drag(19, 9)));
    EXPECT_EQ(list->scroll_offset(), 90);

    std::string markdown;
    for(int line{}; line < 50; ++ line)
    {
        markdown += "Line " + std::to_string(line) + "\n\n";
    }
    auto view{oui::markdown_view(markdown)};
    view->arrange({0, 0, 20, 10});
    EXPECT_TRUE(view->handle_event(press(19, 8)));
    EXPECT_EQ(view->scroll_offset(), 9) << "a page on";
}

TEST(OuiScrollbarTests, AComparisonAndAScrollViewAnswerTheirScrollbarsToo)
{
    // Two texts of fifty lines on six rows between the bars: the thumb's cell at the top, the rest a page on.
    std::string text;
    for(int line{}; line < 50; ++ line)
    {
        text += "line " + std::to_string(line) + "\n";
    }
    oui::DiffView comparison;
    comparison.set_texts(text, text);
    comparison.arrange({0, 0, 60, 8});
    EXPECT_TRUE(comparison.handle_event(press(59, 5))) << "the scrollbar's, below the thumb";

    auto scroll{oui::scroll_view()};
    auto lines{oui::column()};
    for(int line{}; line < 30; ++ line)
    {
        lines->add(oui::label("row " + std::to_string(line)));
    }
    scroll->add(std::move(lines));
    scroll->arrange({0, 0, 20, 10});
    EXPECT_TRUE(scroll->handle_event(press(19, 0)));
    EXPECT_TRUE(scroll->handle_event(drag(19, 9)));
    scroll->arrange({0, 0, 20, 10});
    // Thirty rows in ten: twenty down at most.
    EXPECT_EQ(scroll->scroll_offset(), 20);
}

} // namespace
