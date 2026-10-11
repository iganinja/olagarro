#include <oui/terminal_view.h>

#include <gtest/gtest.h>

#include <string>

using namespace olagarro;

namespace
{

using oui::Key;
using oui::KeyEvent;
using oui::terminal_keys;
using oui::TextEvent;

TEST(OuiTerminalKeysTests, TypesCharactersAsUtf8)
{
    EXPECT_EQ(terminal_keys(TextEvent{U'a'}, false), "a");
    EXPECT_EQ(terminal_keys(TextEvent{U'ñ'}, false), "\xc3\xb1");
    EXPECT_EQ(terminal_keys(TextEvent{U'€'}, false), "\xe2\x82\xac");
}

TEST(OuiTerminalKeysTests, ControlMakesControlCharacters)
{
    EXPECT_EQ(terminal_keys(TextEvent{U'c', true}, false), "\x03");
    EXPECT_EQ(terminal_keys(TextEvent{U'D', true}, false), "\x04");
    EXPECT_EQ(terminal_keys(TextEvent{U'[', true}, false), "\x1b");
    EXPECT_EQ(terminal_keys(TextEvent{U'@', true}, false), std::string(1, '\0'));
    EXPECT_EQ(terminal_keys(KeyEvent{Key::space, true}, false), std::string(1, '\0'));
}

TEST(OuiTerminalKeysTests, AltPutsEscapeBefore)
{
    EXPECT_EQ(terminal_keys(TextEvent{U'b', false, true}, false), "\x1b"
                                                                  "b");
    EXPECT_EQ(terminal_keys(KeyEvent{Key::backspace, false, true}, false), "\x1b\x7f");
}

TEST(OuiTerminalKeysTests, CursorKeysHaveTheirTwoForms)
{
    EXPECT_EQ(terminal_keys(KeyEvent{Key::up}, false), "\x1b[A");
    EXPECT_EQ(terminal_keys(KeyEvent{Key::up}, true), "\x1bOA");
    EXPECT_EQ(terminal_keys(KeyEvent{Key::left, true}, true), "\x1b[1;5D");
    EXPECT_EQ(terminal_keys(KeyEvent{Key::end, false, false, true}, false), "\x1b[1;2F");
}

TEST(OuiTerminalKeysTests, OtherKeysAsXtermSendsThem)
{
    EXPECT_EQ(terminal_keys(KeyEvent{Key::enter}, false), "\r");
    EXPECT_EQ(terminal_keys(KeyEvent{Key::backspace}, false), "\x7f");
    EXPECT_EQ(terminal_keys(KeyEvent{Key::shift_tab}, false), "\x1b[Z");
    EXPECT_EQ(terminal_keys(KeyEvent{Key::delete_}, false), "\x1b[3~");
    EXPECT_EQ(terminal_keys(KeyEvent{Key::page_up, true}, false), "\x1b[5;5~");
    EXPECT_EQ(terminal_keys(KeyEvent{Key::f1}, false), "\x1bOP");
    EXPECT_EQ(terminal_keys(KeyEvent{Key::f5}, false), "\x1b[15~");
    EXPECT_EQ(terminal_keys(KeyEvent{Key::f12}, false), "\x1b[24~");
}

} // namespace
