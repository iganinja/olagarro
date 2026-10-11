#include <oui/event.h>
#include <oui/tree_view.h>

#include <gtest/gtest.h>

using namespace olagarro;

namespace
{

TEST(OuiTreeViewTests, SaysWhichNodeIsChosenAndChoosesOneItOpensTheWayTo)
{
    auto tree{oui::tree_view()};
    auto& fruit{tree->add("fruit")};
    auto& apples{fruit.add("apples")};
    tree->add("vegetables");
    tree->arrange({0, 0, 20, 6});
    auto changes{0};
    tree->on_selection_changed([&changes] { ++ changes; });
    ASSERT_NE(tree->selected(), nullptr);
    EXPECT_EQ(tree->selected()->text, "fruit");
    EXPECT_TRUE(tree->handle_event(oui::KeyEvent{oui::Key::down}));
    EXPECT_EQ(tree->selected()->text, "vegetables");
    EXPECT_EQ(changes, 1);
    // A node under a closed one: the way to it opens.
    tree->select(apples);
    EXPECT_TRUE(fruit.expanded);
    EXPECT_EQ(tree->selected(), &apples);
    EXPECT_EQ(tree->nodes().size(), 2u);
}

} // namespace
