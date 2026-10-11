#include <oui/turbo_c_theme.h>

#include <gtest/gtest.h>

using namespace olagarro;

// The colours Turbo Vision's palette (cpAppColor) gives the parts of its dialogs, menus and windows, as Turbo C++,
// Borland C++ and Turbo Pascal showed them.
TEST(TurboCThemeTests, UsesTurboVisionsPalette)
{
    using oui::TurboCTheme;
    const auto theme{TurboCTheme::make()};

    // Gray dialog: black on light grey, white frame, light green icons.
    EXPECT_EQ(theme.dialog_background, TurboCTheme::light_gray);
    EXPECT_EQ(theme.dialog_foreground, TurboCTheme::black);
    EXPECT_EQ(theme.dialog_border, TurboCTheme::white);
    EXPECT_EQ(theme.dialog_icon_foreground, TurboCTheme::light_green);
    // Buttons black on green, the selected one white.
    EXPECT_EQ(theme.button_background, TurboCTheme::green);
    EXPECT_EQ(theme.button_foreground, TurboCTheme::black);
    EXPECT_EQ(theme.button_focused_background, TurboCTheme::green);
    EXPECT_EQ(theme.button_focused_foreground, TurboCTheme::white);
    // Input lines white on blue; lists black on cyan, the chosen item white on green; clusters on cyan.
    EXPECT_EQ(theme.input_background, TurboCTheme::blue);
    EXPECT_EQ(theme.input_foreground, TurboCTheme::white);
    EXPECT_EQ(theme.list_background, TurboCTheme::cyan);
    EXPECT_EQ(theme.list_foreground, TurboCTheme::black);
    EXPECT_EQ(theme.item_selected_background, TurboCTheme::green);
    EXPECT_EQ(theme.item_selected_foreground, TurboCTheme::white);
    EXPECT_EQ(theme.cluster_background, TurboCTheme::cyan);
    // Menus black on light grey with red shortcuts, the selected item black on green.
    EXPECT_EQ(theme.menu_background, TurboCTheme::light_gray);
    EXPECT_EQ(theme.menu_foreground, TurboCTheme::black);
    EXPECT_EQ(theme.hotkey_foreground, TurboCTheme::red);
    EXPECT_EQ(theme.menu_selected_background, TurboCTheme::green);
    EXPECT_EQ(theme.menu_selected_foreground, TurboCTheme::black);
    // The editor yellow on blue, its selection blue on light grey; shadows dark grey on black.
    EXPECT_EQ(theme.editor_background, TurboCTheme::blue);
    EXPECT_EQ(theme.editor_foreground, TurboCTheme::yellow);
    EXPECT_EQ(theme.selection_background, TurboCTheme::light_gray);
    EXPECT_EQ(theme.selection_foreground, TurboCTheme::blue);
    EXPECT_EQ(theme.shadow, TurboCTheme::black);
    EXPECT_EQ(theme.shadow_foreground, TurboCTheme::dark_gray);
    // A blue window's scroll bar: blue on cyan.
    EXPECT_EQ(theme.scrollbar_background, TurboCTheme::cyan);
    EXPECT_EQ(theme.scrollbar_foreground, TurboCTheme::blue);
}
