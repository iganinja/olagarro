#include <oui/event.h>

#include <gtest/gtest.h>

#include <string_view>
#include <vector>

#include "../../oui/src/terminal_input_decoder.h"

using namespace olagarro;

namespace
{

using oui::Event;
using oui::Key;
using oui::KeyEvent;
using oui::MouseEvent;
using oui::TerminalInputDecoder;
using oui::TextEvent;

std::vector<Event> decode(std::string_view bytes)
{
    TerminalInputDecoder decoder;
    decoder.feed(bytes);
    decoder.flush();
    std::vector<Event> events;
    while(const auto event{decoder.next()})
    {
        events.push_back(*event);
    }
    return events;
}

KeyEvent single_key(std::string_view bytes)
{
    const auto events{decode(bytes)};
    EXPECT_EQ(events.size(), 1u);
    const auto* const key{events.empty() ? nullptr : std::get_if<KeyEvent>(&events.front())};
    EXPECT_NE(key, nullptr);
    return key != nullptr ? *key : KeyEvent{Key::f12};
}

TextEvent single_text(std::string_view bytes)
{
    const auto events{decode(bytes)};
    EXPECT_EQ(events.size(), 1u);
    const auto* const text{events.empty() ? nullptr : std::get_if<TextEvent>(&events.front())};
    EXPECT_NE(text, nullptr);
    return text != nullptr ? *text : TextEvent{0};
}

MouseEvent single_mouse(std::string_view bytes)
{
    const auto events{decode(bytes)};
    EXPECT_EQ(events.size(), 1u);
    const auto* const mouse{events.empty() ? nullptr : std::get_if<MouseEvent>(&events.front())};
    EXPECT_NE(mouse, nullptr);
    return mouse != nullptr ? *mouse : MouseEvent{-1, -1, false};
}

TEST(TerminalInputDecoderTest, ControlLettersBecomeLowercaseShortcuts)
{
    const auto palette{single_text("\x10")};
    EXPECT_EQ(palette.codepoint, U'p');
    EXPECT_TRUE(palette.control);
    const auto quit{single_text("\x11")};
    EXPECT_EQ(quit.codepoint, U'q');
    EXPECT_TRUE(quit.control);
}

TEST(TerminalInputDecoderTest, BackspaceIsDeleteByteAndControlHIsReplace)
{
    EXPECT_EQ(single_key("\x7f").key, Key::backspace);
    const auto replace{single_text("\x08")};
    EXPECT_EQ(replace.codepoint, U'h');
    EXPECT_TRUE(replace.control);
}

TEST(TerminalInputDecoderTest, EnterTabAndSpaceAreKeys)
{
    EXPECT_EQ(single_key("\r").key, Key::enter);
    EXPECT_EQ(single_key("\n").key, Key::enter);
    EXPECT_EQ(single_key("\t").key, Key::tab);
    EXPECT_EQ(single_key(" ").key, Key::space);
    const auto shift_tab{single_key("\x1b[Z")};
    EXPECT_EQ(shift_tab.key, Key::shift_tab);
    EXPECT_TRUE(shift_tab.shift);
}

TEST(TerminalInputDecoderTest, ReadsEveryEventOfAChunk)
{
    const auto events{decode("Hola ñ\r")};
    ASSERT_EQ(events.size(), 7u);
    EXPECT_EQ(std::get<TextEvent>(events.at(0)).codepoint, U'H');
    EXPECT_TRUE(std::get<TextEvent>(events.at(0)).shift);
    EXPECT_EQ(std::get<TextEvent>(events.at(3)).codepoint, U'a');
    EXPECT_EQ(std::get<KeyEvent>(events.at(4)).key, Key::space);
    EXPECT_EQ(std::get<TextEvent>(events.at(5)).codepoint, U'ñ');
    EXPECT_EQ(std::get<KeyEvent>(events.at(6)).key, Key::enter);
}

TEST(TerminalInputDecoderTest, WaitsForSplitUtf8AndEscapeSequences)
{
    TerminalInputDecoder decoder;
    decoder.feed("\xc3");
    EXPECT_FALSE(decoder.next());
    EXPECT_TRUE(decoder.has_pending());
    decoder.feed("\xb1");
    const auto text{decoder.next()};
    ASSERT_TRUE(text);
    EXPECT_EQ(std::get<TextEvent>(*text).codepoint, U'ñ');

    decoder.feed("\x1b[1;");
    EXPECT_FALSE(decoder.next());
    decoder.feed("5C");
    const auto word_right{decoder.next()};
    ASSERT_TRUE(word_right);
    EXPECT_EQ(std::get<KeyEvent>(*word_right).key, Key::right);
    EXPECT_TRUE(std::get<KeyEvent>(*word_right).control);
    EXPECT_FALSE(decoder.has_pending());
}

TEST(TerminalInputDecoderTest, LoneEscapeWaitsUntilFlushed)
{
    TerminalInputDecoder decoder;
    decoder.feed("\x1b");
    EXPECT_FALSE(decoder.next());
    decoder.flush();
    const auto escape{decoder.next()};
    ASSERT_TRUE(escape);
    EXPECT_EQ(std::get<KeyEvent>(*escape).key, Key::escape);
    EXPECT_FALSE(decoder.has_pending());

    // A flush resolves only what was buffered; later sequences wait again.
    decoder.feed("\x1b");
    EXPECT_FALSE(decoder.next());
}

TEST(TerminalInputDecoderTest, EscapeBeforeAKeyIsAlt)
{
    const auto file_menu{single_text("\x1b"
                                     "f")};
    EXPECT_EQ(file_menu.codepoint, U'f');
    EXPECT_TRUE(file_menu.alt);
    EXPECT_FALSE(file_menu.control);
}

TEST(TerminalInputDecoderTest, NavigationKeysCarryModifiers)
{
    EXPECT_EQ(single_key("\x1b[A").key, Key::up);
    EXPECT_EQ(single_key("\x1bOB").key, Key::down);
    EXPECT_EQ(single_key("\x1b[H").key, Key::home);
    EXPECT_EQ(single_key("\x1b[4~").key, Key::end);
    const auto select_left{single_key("\x1b[1;2D")};
    EXPECT_EQ(select_left.key, Key::left);
    EXPECT_TRUE(select_left.shift);
    EXPECT_FALSE(select_left.control);
    const auto next_tab{single_key("\x1b[6;5~")};
    EXPECT_EQ(next_tab.key, Key::page_down);
    EXPECT_TRUE(next_tab.control);
    const auto copy{single_key("\x1b[2;5~")};
    EXPECT_EQ(copy.key, Key::insert_);
    EXPECT_TRUE(copy.control);
    EXPECT_EQ(single_key("\x1b[3~").key, Key::delete_);
}

TEST(TerminalInputDecoderTest, FunctionKeysUsedByTheEditor)
{
    EXPECT_EQ(single_key("\x1bOR").key, Key::f3);
    const auto find_previous{single_key("\x1b[1;2R")};
    EXPECT_EQ(find_previous.key, Key::f3);
    EXPECT_TRUE(find_previous.shift);
    EXPECT_EQ(single_key("\x1bOS").key, Key::f4);
    EXPECT_EQ(single_key("\x1b[24~").key, Key::f12);
}

TEST(TerminalInputDecoderTest, EveryFunctionKey)
{
    using oui::Key;
    EXPECT_EQ(single_key("\x1bOP").key, Key::f1);
    EXPECT_EQ(single_key("\x1bOQ").key, Key::f2);
    EXPECT_EQ(single_key("\x1b[11~").key, Key::f1);
    EXPECT_EQ(single_key("\x1b[15~").key, Key::f5);
    EXPECT_EQ(single_key("\x1b[17~").key, Key::f6);
    EXPECT_EQ(single_key("\x1b[18~").key, Key::f7);
    EXPECT_EQ(single_key("\x1b[19~").key, Key::f8);
    EXPECT_EQ(single_key("\x1b[20~").key, Key::f9);
    EXPECT_EQ(single_key("\x1b[21~").key, Key::f10);
    EXPECT_EQ(single_key("\x1b[23~").key, Key::f11);
    const auto control_f2{single_key("\x1b[1;5Q")};
    EXPECT_EQ(control_f2.key, Key::f2);
    EXPECT_TRUE(control_f2.control);
    const auto shift_f9{single_key("\x1b[20;2~")};
    EXPECT_EQ(shift_f9.key, Key::f9);
    EXPECT_TRUE(shift_f9.shift);
}

TEST(TerminalInputDecoderTest, ModifyOtherKeysDistinguishesControlShiftLetters)
{
    const auto save_as{single_text("\x1b[27;6;83~")};
    EXPECT_EQ(save_as.codepoint, U's');
    EXPECT_TRUE(save_as.control);
    EXPECT_TRUE(save_as.shift);
    const auto csi_u{single_text("\x1b[115;6u")};
    EXPECT_EQ(csi_u.codepoint, U's');
    EXPECT_TRUE(csi_u.control);
    EXPECT_TRUE(csi_u.shift);
}

TEST(TerminalInputDecoderTest, SgrMouseReports)
{
    const auto press{single_mouse("\x1b[<0;31;3M")};
    EXPECT_EQ(press.x, 30);
    EXPECT_EQ(press.y, 2);
    EXPECT_TRUE(press.pressed);
    EXPECT_TRUE(single_mouse("\x1b[<0;31;3m").released);
    EXPECT_TRUE(single_mouse("\x1b[<32;35;3M").dragging);
    EXPECT_TRUE(single_mouse("\x1b[<35;10;4M").hover);
    EXPECT_TRUE(single_mouse("\x1b[<2;5;5M").right);
    EXPECT_EQ(single_mouse("\x1b[<64;5;5M").wheel_delta, 1);
    EXPECT_EQ(single_mouse("\x1b[<65;5;5M").wheel_delta, -1);
}

TEST(TerminalInputDecoderTest, ReportsTheTerminalGainingAndLosingFocus)
{
    const auto events{decode("\x1b[I\x1b[O")};
    ASSERT_EQ(events.size(), 2u);
    EXPECT_TRUE(std::get<oui::FocusEvent>(events.at(0)).focused);
    EXPECT_FALSE(std::get<oui::FocusEvent>(events.at(1)).focused);
}

TEST(TerminalInputDecoderTest, MouseReportsCarryTheirModifiers)
{
    const auto shift_click{single_mouse("\x1b[<4;5;5M")};
    EXPECT_TRUE(shift_click.pressed);
    EXPECT_TRUE(shift_click.shift);
    EXPECT_FALSE(shift_click.control);
    const auto control_alt_click{single_mouse("\x1b[<24;5;5M")};
    EXPECT_TRUE(control_alt_click.pressed);
    EXPECT_TRUE(control_alt_click.control);
    EXPECT_TRUE(control_alt_click.alt);
    EXPECT_TRUE(single_mouse("\x1b[<36;5;5M").dragging);
}

TEST(TerminalInputDecoderTest, UnknownSequencesAreSwallowedWhole)
{
    const auto events{decode("\x1b[?1;2cx")};
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(std::get<TextEvent>(events.front()).codepoint, U'x');
}

} // namespace

TEST(TerminalInputDecoderTest, ControlSlashArrivesAsItsControlByteOrAsAModifiedKey)
{
    const auto legacy{single_text("\x1f")};
    EXPECT_EQ(legacy.codepoint, U'/');
    EXPECT_TRUE(legacy.control);
    // modifyOtherKeys, which OUI turns on: CSI 27 ; 5 (Ctrl) ; 47 ('/') ~.
    const auto modified{single_text("\x1b[27;5;47~")};
    EXPECT_EQ(modified.codepoint, U'/');
    EXPECT_TRUE(modified.control);
}

TEST(TerminalInputDecoderTest, ControlBackslashAndBracketArriveAsTheirControlBytes)
{
    const auto backslash{single_text("\x1c")};
    EXPECT_EQ(backslash.codepoint, U'\\');
    EXPECT_TRUE(backslash.control);
    const auto bracket{single_text("\x1d")};
    EXPECT_EQ(bracket.codepoint, U']');
    EXPECT_TRUE(bracket.control);
}

TEST(TerminalInputDecoderTest, AltArrowsCarryTheirModifier)
{
    const auto up{single_key("\x1b[1;3A")};
    EXPECT_EQ(up.key, Key::up);
    EXPECT_TRUE(up.alt);
    const auto shifted_down{single_key("\x1b[1;4B")};
    EXPECT_EQ(shifted_down.key, Key::down);
    EXPECT_TRUE(shifted_down.alt);
    EXPECT_TRUE(shifted_down.shift);
}

TEST(TerminalInputDecoderTest, FunctionKeysWithModifiersInKonsolesFormCarryThem)
{
    // Konsole sends F1 to F4 with a modifier as SS3, the modifier before the letter: ESC O 2 S for Shift+F4. It came
    // out as a capital S.
    const auto shift_f4{single_key("\x1bO2S")};
    EXPECT_EQ(shift_f4.key, Key::f4);
    EXPECT_TRUE(shift_f4.shift);
    EXPECT_FALSE(shift_f4.control);
    const auto control_f1{single_key("\x1bO5P")};
    EXPECT_EQ(control_f1.key, Key::f1);
    EXPECT_TRUE(control_f1.control);
    const auto plain{single_key("\x1bOS")};
    EXPECT_EQ(plain.key, Key::f4);
    EXPECT_FALSE(plain.shift);
}
