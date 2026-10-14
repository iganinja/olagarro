#include <oui/application.h>
#include <oui/layout.h>
#include <oui/line_edit.h>
#include <oui/menu_bar.h>
#include <oui/scripted_backend.h>
#include <oui/spacer.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

namespace
{

using namespace olagarro;
using namespace olagarro::oui;

// The rows of the last frame, as text.
std::vector<std::u32string> rows_of(const CellBuffer& frame)
{
    std::vector<std::u32string> rows;
    for(int y{}; y < frame.height(); ++ y)
    {
        std::u32string row;
        for(int x{}; x < frame.width(); ++ x)
        {
            row += frame.cell_at(x, y).glyph;
        }
        rows.push_back(row);
    }
    return rows;
}

bool shows(const std::vector<std::u32string>& rows, std::u32string_view text)
{
    for(const auto& row : rows)
    {
        if(row.find(text) != std::u32string::npos)
        {
            return true;
        }
    }
    return false;
}

TEST(OuiMenuScrollTests, AMenuTallerThanTheScreenScrollsToWhatIsSelected)
{
    std::vector<Event> events;
    for(int step{}; step < 14; ++ step)
    {
        events.push_back(KeyEvent{Key::down});
    }
    auto backend{scripted_backend(events, {40, 10})};
    auto& screen{*backend};
    Application application{std::move(backend)};
    std::vector<MenuItem> items;
    for(int index{}; index < 20; ++ index)
    {
        const auto id{"item." + std::to_string(index)};
        application.commands().register_command(id, "Item " + std::to_string(index), [] {});
        items.emplace_back(id);
    }
    auto content{column()};
    auto& bar{menu_bar(application.commands(), *content)};
    bar.add_menu("&File", items);
    content->add(vspacer(9));
    application.set_content(std::move(content));
    bar.open_menu(0);
    for(int frame{}; frame < 20; ++ frame)
    {
        application.run_frame();
    }
    ASSERT_TRUE(screen.last_frame());
    const auto rows{rows_of(*screen.last_frame())};
    // The 15th entry is marked and shows; the first ones have scrolled away, and both arrows show.
    EXPECT_TRUE(shows(rows, U"Item 14"));
    EXPECT_FALSE(shows(rows, U"Item 0 "));
    EXPECT_TRUE(shows(rows, U"▲"));
    EXPECT_TRUE(shows(rows, U"▼"));
}

TEST(OuiMenuScrollTests, AContextMenuOpensWhereItIsAskedAndRunsWhatIsChosen)
{
    auto backend{scripted_backend({KeyEvent{Key::down}, KeyEvent{Key::enter}}, {40, 10})};
    auto& screen{*backend};
    screen.set_keep_frames(true);
    screen.set_one_event_per_frame(true);
    Application application{std::move(backend)};
    auto content{column()};
    auto& bar{menu_bar(application.commands(), *content)};
    bar.add_menu("&File", {menu_action("Open", [] {})});
    content->add(vspacer(9));
    application.set_content(std::move(content));
    std::string ran;
    bar.open_context_menu(
        {menu_action("Rename", [&ran] { ran = "rename"; }), menu_action("Delete", [&ran] { ran = "delete"; })},
        {12, 4});
    application.run_frame();
    ASSERT_TRUE(screen.last_frame());
    // Its first entry sits under the point asked, past the frame.
    const auto rows{rows_of(screen.frames().front())};
    const auto rename_row{std::find_if(rows.begin(), rows.end(), [](const std::u32string& row)
                                       { return row.find(U"Rename") != std::u32string::npos; })};
    ASSERT_NE(rename_row, rows.end());
    EXPECT_EQ(rename_row - rows.begin(), 5);
    EXPECT_GT(rename_row->find(U"Rename"), 12u);
    for(int frame{}; frame < 4; ++ frame)
    {
        application.run_frame();
    }
    EXPECT_EQ(ran, "delete");
    EXPECT_FALSE(bar.is_open());
}

TEST(OuiMenuScrollTests, AClickOnASubmenusEntryThatHoveringOpenedRunsNothing)
{
    // Mode is the menu's third entry, on row 4 past the bar and the popup's frame: the pointer rests on it, which opens
    // the submenu, then clicks it.
    MouseEvent hovering{3, 4, false};
    hovering.hover = true;
    auto backend{scripted_backend({hovering, MouseEvent{3, 4, true}, MouseEvent{3, 4, false}}, {60, 12})};
    auto& screen{*backend};
    screen.set_one_event_per_frame(true);
    Application application{std::move(backend)};
    std::vector<std::string> ran;
    for(const auto* const id : {"image.one", "image.two", "mode.a", "mode.b", "mode.c"})
    {
        application.commands().register_command(id, id, [&ran, id] { ran.emplace_back(id); });
    }
    auto content{column()};
    auto& bar{menu_bar(application.commands(), *content)};
    bar.add_menu("&Image", {"image.one", "image.two",
                            menu_submenu("Mode", std::vector<MenuItem>{"mode.a", "mode.b", "mode.c"})});
    content->add(vspacer(11));
    application.set_content(std::move(content));
    bar.open_menu(0);
    for(int frame{}; frame < 5; ++ frame)
    {
        application.run_frame();
    }
    EXPECT_TRUE(ran.empty()) << "the submenu's third entry ran: " << (ran.empty() ? "" : ran.front());
    EXPECT_TRUE(bar.is_open());
    ASSERT_TRUE(screen.last_frame());
    EXPECT_TRUE(shows(rows_of(*screen.last_frame()), U"mode.c")) << "the submenu stays open";
}

TEST(OuiMenuScrollTests, AClickOnAMenuEntryLeavesTheFocusWhereItWas)
{
    // The entry's row: the bar is row 0, the popup's frame row 1, its first entry row 2.
    auto backend{scripted_backend({MouseEvent{3, 2, true}, MouseEvent{3, 2, false}}, {40, 10})};
    auto& screen{*backend};
    screen.set_one_event_per_frame(true);
    Application application{std::move(backend)};
    auto content{column()};
    auto& bar{menu_bar(application.commands(), *content)};
    Widget* focused_when_run{};
    bar.add_menu("&File", {menu_action("Rename", [&application, &focused_when_run]
                                       { focused_when_run = application.focused(); })});
    auto& field{content->add(line_edit())};
    application.set_content(std::move(content));
    application.focus(field);
    bar.open_menu(0);
    for(int frame{}; frame < 4; ++ frame)
    {
        application.run_frame();
    }
    EXPECT_EQ(focused_when_run, &field);
    EXPECT_EQ(application.focused(), &field);
}

TEST(OuiMenuScrollTests, ASubmenuWithNoRoomOnTheRightOpensOnTheLeft)
{
    auto backend{scripted_backend({KeyEvent{Key::right}}, {40, 10})};
    auto& screen{*backend};
    screen.set_one_event_per_frame(true);
    Application application{std::move(backend)};
    auto content{column()};
    auto& bar{menu_bar(application.commands(), *content)};
    bar.add_menu("&File", {menu_action("Open", [] {})});
    content->add(vspacer(9));
    application.set_content(std::move(content));
    bar.open_context_menu({menu_submenu("Open As", std::vector<MenuItem>{menu_action("Text", [] {})})}, {30, 2});
    for(int frame{}; frame < 3; ++ frame)
    {
        application.run_frame();
    }
    ASSERT_TRUE(screen.last_frame());
    const auto rows{rows_of(*screen.last_frame())};
    const auto& row{rows.at(3)};
    ASSERT_NE(row.find(U"Text"), std::u32string::npos);
    ASSERT_NE(row.find(U"Open As"), std::u32string::npos);
    EXPECT_LT(row.find(U"Text"), row.find(U"Open As"));
}

} // namespace
