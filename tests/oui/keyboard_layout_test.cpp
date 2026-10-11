#include "../../oui/src/graphics/keyboard_layout.h"

#include <gtest/gtest.h>

#include <linux/input-event-codes.h>

#include <string>
#include <vector>

using namespace olagarro;

namespace
{

using oui::KeyboardLayout;

// What pressing and letting go of each key in turn writes, as text.
[[nodiscard]] std::u32string typed(KeyboardLayout& layout, const std::vector<unsigned int>& keys)
{
    std::u32string text;
    for(const auto code : keys)
    {
        for(const auto& event : layout.key(code, 1))
        {
            if(const auto* const written{std::get_if<oui::TextEvent>(&event)})
            {
                text += written->codepoint;
            }
        }
        static_cast<void>(layout.key(code, 0));
    }
    return text;
}

TEST(OuiKeyboardLayoutTests, KnowsItsLayoutsByName)
{
    EXPECT_TRUE(KeyboardLayout::named("us"));
    EXPECT_TRUE(KeyboardLayout::named("es"));
    EXPECT_FALSE(KeyboardLayout::named("klingon"));
}

TEST(OuiKeyboardLayoutTests, WritesWhatASpanishKeyboardShows)
{
    auto layout{*KeyboardLayout::named("es")};
    EXPECT_EQ(typed(layout, {KEY_SEMICOLON, KEY_BACKSLASH, KEY_EQUAL}), U"ñç¡");
    static_cast<void>(layout.key(KEY_LEFTSHIFT, 1));
    EXPECT_EQ(typed(layout, {KEY_SEMICOLON, KEY_7, KEY_MINUS, KEY_EQUAL, KEY_2}), U"Ñ/?¿\"");
    static_cast<void>(layout.key(KEY_LEFTSHIFT, 0));
    static_cast<void>(layout.key(KEY_RIGHTALT, 1));
    EXPECT_EQ(typed(layout, {KEY_2, KEY_E, KEY_1, KEY_GRAVE}), U"@€|\\") << "AltGr";
    static_cast<void>(layout.key(KEY_RIGHTALT, 0));
}

TEST(OuiKeyboardLayoutTests, DeadKeysAccentTheNextLetter)
{
    auto layout{*KeyboardLayout::named("es")};
    EXPECT_EQ(typed(layout, {KEY_APOSTROPHE, KEY_A, KEY_APOSTROPHE, KEY_O}), U"áó");
    static_cast<void>(layout.key(KEY_LEFTSHIFT, 1));
    EXPECT_EQ(typed(layout, {KEY_APOSTROPHE}), U"") << "Shift and the acute key is ¨, waiting";
    static_cast<void>(layout.key(KEY_LEFTSHIFT, 0));
    EXPECT_EQ(typed(layout, {KEY_U}), U"ü");
    EXPECT_EQ(typed(layout, {KEY_LEFTBRACE, KEY_E}), U"è") << "the grave";
    EXPECT_EQ(typed(layout, {KEY_APOSTROPHE, KEY_SPACE}), U"´") << "an accent on its own, before a space";
    EXPECT_EQ(typed(layout, {KEY_APOSTROPHE, KEY_X}), U"´x") << "a letter the accent does not go on";
    EXPECT_EQ(typed(layout, {KEY_APOSTROPHE, KEY_APOSTROPHE, KEY_E}), U"´é") << "two dead keys";
    static_cast<void>(layout.key(KEY_LEFTSHIFT, 1));
    EXPECT_EQ(typed(layout, {KEY_APOSTROPHE}), U"");
    static_cast<void>(layout.key(KEY_LEFTSHIFT, 0));
    static_cast<void>(layout.key(KEY_LEFTSHIFT, 1));
    EXPECT_EQ(typed(layout, {KEY_A}), U"Ä");
    static_cast<void>(layout.key(KEY_LEFTSHIFT, 0));
}

TEST(OuiKeyboardLayoutTests, ShortcutsComeAsTheTerminalSendsThem)
{
    auto layout{*KeyboardLayout::named("es")};
    static_cast<void>(layout.key(KEY_LEFTCTRL, 1));
    const auto save{layout.key(KEY_S, 1)};
    ASSERT_EQ(save.size(), 1u);
    const auto& text{std::get<oui::TextEvent>(save.front())};
    EXPECT_EQ(text.codepoint, U's');
    EXPECT_TRUE(text.control);
    const auto next_tab{layout.key(KEY_PAGEDOWN, 1)};
    ASSERT_EQ(next_tab.size(), 1u);
    EXPECT_TRUE(std::get<oui::KeyEvent>(next_tab.front()).control);
    static_cast<void>(layout.key(KEY_LEFTCTRL, 0));
    EXPECT_FALSE(layout.control());
}

TEST(OuiKeyboardLayoutTests, TellsWhenTheModifiersChange)
{
    auto layout{*KeyboardLayout::named("us")};
    const auto pressed{layout.key(KEY_LEFTCTRL, 1)};
    ASSERT_EQ(pressed.size(), 1u);
    EXPECT_TRUE(std::get<oui::ModifiersEvent>(pressed.front()).control);
    EXPECT_TRUE(layout.key(KEY_LEFTCTRL, 2).empty()) << "a held key repeating changes nothing";
    EXPECT_FALSE(std::get<oui::ModifiersEvent>(layout.key(KEY_LEFTCTRL, 0).front()).control);
    static_cast<void>(layout.key(KEY_RIGHTALT, 1));
    EXPECT_TRUE(layout.alt()) << "with no AltGr on the layout, the right Alt is Alt";
}

} // namespace
