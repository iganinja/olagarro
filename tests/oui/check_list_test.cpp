#include <oui/check_list.h>
#include <oui/event.h>
#include <oui/label.h>

#include <gtest/gtest.h>

#include <utility>
#include <vector>

using namespace olagarro;

namespace
{

TEST(OuiCheckListTests, AClickOnABoxOrSpaceTicksItAndSaysSo)
{
    auto list{oui::check_list()};
    list->add_item("bread", false);
    list->add_item("call Ana", true);
    list->arrange({0, 0, 30, 5});
    EXPECT_FALSE(list->is_checked(0));
    EXPECT_TRUE(list->is_checked(1));
    std::vector<std::pair<std::size_t, bool>> told;
    list->on_toggled([&told](std::size_t row, bool checked) { told.emplace_back(row, checked); });

    // The box is the row's first columns: a click there ticks it, and chooses the row.
    EXPECT_TRUE(list->handle_event(oui::MouseEvent{1, 0, true}));
    EXPECT_TRUE(list->is_checked(0));
    EXPECT_EQ(list->selected_row(), std::optional<std::size_t>{0});
    // On the text, a click only chooses.
    EXPECT_TRUE(list->handle_event(oui::MouseEvent{10, 1, true}));
    EXPECT_TRUE(list->is_checked(1));
    // Space ticks the row chosen, here back off.
    EXPECT_TRUE(list->handle_event(oui::KeyEvent{oui::Key::space}));
    EXPECT_FALSE(list->is_checked(1));
    EXPECT_EQ(told, (std::vector<std::pair<std::size_t, bool>>{{0, true}, {1, false}}));

    // Set from the code: not something the person did.
    list->set_checked(0, false);
    EXPECT_FALSE(list->is_checked(0));
    EXPECT_EQ(told.size(), 2u);
}

} // namespace
