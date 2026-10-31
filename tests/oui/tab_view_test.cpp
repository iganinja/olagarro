#include <oui/label.h>
#include <oui/application.h>
#include <oui/cell_buffer.h>
#include <oui/painter.h>
#include <oui/line_edit.h>
#include <oui/list.h>
#include <oui/scripted_backend.h>
#include <oui/split_view.h>
#include <oui/text_edit.h>
#include <oui/tab_view.h>

#include <gtest/gtest.h>

#include <optional>
#include <string>

namespace
{

using namespace olagarro;
using namespace olagarro::oui;

std::unique_ptr<TabView> three_tabs()
{
    auto tabs{tab_view()};
    tabs->add_tab("one", label("1"));
    tabs->add_tab("two", label("2"));
    tabs->add_tab("three", label("3"));
    tabs->arrange({0, 0, 60, 10});
    return tabs;
}

TEST(OuiTabViewTests, SaysWhereADraggedTabIsLetGo)
{
    auto tabs{three_tabs()};
    std::optional<std::pair<std::size_t, Point>> dropped;
    tabs->on_tab_dropped([&dropped](std::size_t index, Point where) { dropped = {index, where}; });
    // Pressed and let go on its own title, a tab is only clicked.
    EXPECT_TRUE(tabs->handle_event(MouseEvent{1, 0, true}));
    EXPECT_TRUE(tabs->handle_event(MouseEvent{1, 0, false, 0, false, true}));
    EXPECT_FALSE(dropped);
    // Dragged off it, letting go says which tab and where.
    const auto second{*tabs->tab_at(10)};
    EXPECT_TRUE(tabs->handle_event(MouseEvent{10, 0, true}));
    EXPECT_TRUE(tabs->handle_event(MouseEvent{30, 5, false, 0, true}));
    EXPECT_TRUE(tabs->handle_event(MouseEvent{30, 5, false, 0, false, true}));
    ASSERT_TRUE(dropped);
    EXPECT_EQ(dropped->first, second);
    EXPECT_EQ(dropped->second.x, 30);
    EXPECT_EQ(dropped->second.y, 5);
}

TEST(OuiTabViewTests, MovesATabAmongTheOthersKeepingTheSelection)
{
    auto tabs{three_tabs()};
    tabs->select(1);
    tabs->move_tab(0, 2);
    EXPECT_EQ(tabs->title(0), "two");
    EXPECT_EQ(tabs->title(1), "three");
    EXPECT_EQ(tabs->title(2), "one");
    EXPECT_EQ(tabs->selected_index(), 0u);
    tabs->move_tab(2, 0);
    EXPECT_EQ(tabs->title(0), "one");
    EXPECT_EQ(tabs->selected_index(), 1u);
    EXPECT_EQ(tabs->tab_at(0), std::optional<std::size_t>{0});
    EXPECT_FALSE(tabs->tab_at(59));
}

TEST(OuiTabViewTests, WithoutItsRowOfTabsTheSelectedOneHasAllTheRoom)
{
    // As an editor's distraction-free mode shows a document alone.
    auto tabs{three_tabs()};
    EXPECT_TRUE(tabs->header_visible());
    EXPECT_EQ(tabs->child_at(0).bounds().y, 1);
    tabs->set_header_visible(false);
    tabs->arrange({0, 0, 60, 10});
    EXPECT_EQ(tabs->child_at(0).bounds().y, 0);
    EXPECT_EQ(tabs->child_at(0).bounds().height, 10);
    EXPECT_EQ(tabs->measure({60, 10}).height, tabs->child_at(0).measure({60, 10}).height);
    // Nothing there to click: the press goes to what the tab shows.
    EXPECT_FALSE(tabs->handle_event(MouseEvent{10, 0, true}));
    EXPECT_EQ(tabs->selected_index(), 0u);
    CellBuffer buffer{60, 10};
    CellBufferPainter painter{buffer};
    tabs->paint(painter, Theme{});
    EXPECT_EQ(buffer.cell_at(0, 0).glyph, U'1');

    tabs->set_header_visible(true);
    tabs->arrange({0, 0, 60, 10});
    EXPECT_EQ(tabs->child_at(0).bounds().y, 1);
}

TEST(OuiTabViewTests, StepsThroughTheTabsFromTheMostRecentlyUsed)
{
    auto tabs{three_tabs()};
    tabs->select(2);
    tabs->select(0);
    // Used last: one, three, two.
    tabs->step_recent(1);
    EXPECT_EQ(tabs->title(tabs->selected_index()), "three") << "the tab used before";
    tabs->end_recent_steps();
    tabs->step_recent(1);
    EXPECT_EQ(tabs->title(tabs->selected_index()), "one") << "and back, from a new start";

    // Going on while stepping keeps the order it began with, and the one it ends on comes first in it.
    tabs->step_recent(1);
    EXPECT_EQ(tabs->title(tabs->selected_index()), "two");
    tabs->end_recent_steps();
    EXPECT_EQ(tabs->recent_order(), (std::vector<std::size_t>{1, 2, 0}));

    // Backwards it starts at the one used longest ago; selecting any other way starts afresh.
    tabs->step_recent(-1);
    EXPECT_EQ(tabs->title(tabs->selected_index()), "one");
    tabs->select(1);
    tabs->step_recent(1);
    EXPECT_EQ(tabs->title(tabs->selected_index()), "one");
}

} // namespace

namespace
{

TEST(OuiTableRowTests, SaysWhichRowIsAtAPlaceAndNoneBelowTheLast)
{
    auto rows{oui::list()};
    rows->add_item("one");
    rows->add_item("two");
    rows->arrange({0, 0, 20, 10});
    EXPECT_EQ(rows->row_at(0), std::optional<std::size_t>{0});
    EXPECT_EQ(rows->row_at(1), std::optional<std::size_t>{1});
    EXPECT_FALSE(rows->row_at(5));
}

} // namespace

namespace
{

TEST(OuiTableRowTests, AClickOnARowFocusesItsList)
{
    auto backend{
        oui::scripted_backend({oui::MouseEvent{5, 1, true}, oui::MouseEvent{5, 1, false, 0, false, true}}, {40, 10})};
    // The click comes once the screen is drawn, as a person's does.
    backend->set_one_event_per_frame(true);
    oui::Application application{std::move(backend)};
    auto split{oui::split_view()};
    auto rows{oui::list()};
    auto& rows_reference{*rows};
    rows->add_item("one");
    rows->add_item("two");
    split->add(std::move(rows));
    auto& note{split->add(oui::text_edit())};
    application.set_content(std::move(split));
    application.focus(note);
    for(int frame{}; frame < 4; ++ frame)
    {
        application.run_frame();
    }
    EXPECT_EQ(application.focused(), &rows_reference);
    EXPECT_EQ(rows_reference.selected_row(), std::optional<std::size_t>{1});
}

} // namespace

namespace
{

TEST(OuiTextFieldKeysTests, CtrlAInAFieldNobodyElseTookSelectsItsText)
{
    auto backend{oui::scripted_backend({oui::TextEvent{U'a', true}, oui::TextEvent{U'x'}}, {40, 5})};
    oui::Application application{std::move(backend)};
    auto field{oui::line_edit()};
    auto& field_reference{*field};
    field->set_text("tasks.md");
    application.set_content(std::move(field));
    application.focus(field_reference);
    for(int frame{}; frame < 4; ++ frame)
    {
        application.run_frame();
    }
    EXPECT_EQ(field_reference.text(), "x");
}

} // namespace
