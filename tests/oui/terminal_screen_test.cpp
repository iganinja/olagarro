#include <oui/terminal_screen.h>

#include <gtest/gtest.h>

#include <string>

using namespace olagarro;

namespace
{

using oui::TerminalScreen;

// What a row of the screen shows, without the blanks at its end.
[[nodiscard]] std::u32string row_text(const TerminalScreen& screen, int row, int scrolled_back = 0)
{
    std::u32string text;
    for(int column{}; column < screen.columns(); ++ column)
    {
        const auto& cell{screen.cell(column, row, scrolled_back)};
        if(!cell.continuation)
        {
            text += cell.glyph;
        }
    }
    return text.substr(0, text.find_last_not_of(U' ') + 1);
}

TEST(OuiTerminalScreenTests, WritesLinesAndMovesOnThem)
{
    TerminalScreen screen{20, 4};
    screen.feed("hello\r\nworld");
    EXPECT_EQ(row_text(screen, 0), U"hello");
    EXPECT_EQ(row_text(screen, 1), U"world");
    EXPECT_EQ(screen.cursor_row(), 1);
    EXPECT_EQ(screen.cursor_column(), 5);
    screen.feed("\b\bLD\ttab");
    EXPECT_EQ(row_text(screen, 1), U"worLD   tab");
}

TEST(OuiTerminalScreenTests, WrapsAtTheEdgeAndScrollsIntoTheHistory)
{
    TerminalScreen screen{5, 2};
    screen.feed("abcdefgh");
    EXPECT_EQ(row_text(screen, 0), U"abcde");
    EXPECT_EQ(row_text(screen, 1), U"fgh");
    screen.feed("\r\nnext");
    EXPECT_EQ(row_text(screen, 0), U"fgh");
    EXPECT_EQ(row_text(screen, 1), U"next");
    ASSERT_EQ(screen.history_size(), 1);
    EXPECT_EQ(row_text(screen, 0, 1), U"abcde") << "scrolled back a line";
    EXPECT_EQ(row_text(screen, 1, 1), U"fgh");
}

TEST(OuiTerminalScreenTests, ReadsUtf8CutBetweenWritesAndWideCharacters)
{
    TerminalScreen screen{10, 2};
    const std::string text{"ñ日"};
    screen.feed(text.substr(0, 1));
    screen.feed(text.substr(1));
    EXPECT_EQ(row_text(screen, 0), U"ñ日");
    EXPECT_TRUE(screen.cell(2, 0).continuation) << "the ideograph takes two cells";
    EXPECT_EQ(screen.cursor_column(), 3);
    screen.feed("e\xcc\x81");
    EXPECT_EQ(screen.cell(3, 0).glyph, U'é') << "a combining accent joins its letter";
}

TEST(OuiTerminalScreenTests, MovesTheCursorAndErasesAsAskedTo)
{
    TerminalScreen screen{10, 3};
    screen.feed("0123456789\r\nabcdefghij\r\nABCDEFGHIJ");
    screen.feed("\x1b[2;3H");
    EXPECT_EQ(screen.cursor_row(), 1);
    EXPECT_EQ(screen.cursor_column(), 2);
    screen.feed("\x1b[K");
    EXPECT_EQ(row_text(screen, 1), U"ab");
    screen.feed("\x1b[1;5H\x1b[1K");
    EXPECT_EQ(row_text(screen, 0), U"     56789");
    screen.feed("\x1b[3;1H\x1b[2P");
    EXPECT_EQ(row_text(screen, 2), U"CDEFGHIJ") << "two characters deleted";
    screen.feed("\x1b[2J");
    EXPECT_EQ(row_text(screen, 0), U"");
    EXPECT_EQ(row_text(screen, 2), U"");
}

TEST(OuiTerminalScreenTests, ColoursAndStylesTheText)
{
    TerminalScreen screen{20, 2};
    screen.feed(
        "\x1b[1;31mred\x1b[0m \x1b[38;5;21mblue\x1b[48;2;10;20;30mbg\x1b[7mI\r\n\x1b[0;31mr\x1b[1;38;2;1;2;3mt");
    EXPECT_TRUE(screen.cell(0, 0).bold);
    EXPECT_FALSE(screen.cell(0, 0).foreground.is_default);
    EXPECT_EQ(screen.cell(0, 0).foreground.rgb, (oui::Color{255, 85, 85})) << "bold makes the first eight bright";
    EXPECT_EQ(screen.cell(0, 1).foreground.rgb, (oui::Color{170, 0, 0}));
    EXPECT_EQ(screen.cell(1, 1).foreground.rgb, (oui::Color{1, 2, 3})) << "but no other colour";
    EXPECT_TRUE(screen.cell(3, 0).foreground.is_default) << "reset";
    EXPECT_EQ(screen.cell(4, 0).foreground.rgb, (oui::Color{0, 0, 255})) << "colour 21 of the 256";
    EXPECT_EQ(screen.cell(8, 0).background.rgb, (oui::Color{10, 20, 30}));
    EXPECT_TRUE(screen.cell(10, 0).inverse);
}

TEST(OuiTerminalScreenTests, GivesTheWholeScreenToAProgramAndBack)
{
    TerminalScreen screen{10, 3};
    screen.feed("shell$ ");
    screen.feed("\x1b[?1049h\x1b[Heditor");
    EXPECT_EQ(row_text(screen, 0), U"editor");
    EXPECT_EQ(screen.history_size(), 0);
    screen.feed("\x1b[?1049l");
    EXPECT_EQ(row_text(screen, 0), U"shell$") << "the shell's screen as it was";
    EXPECT_EQ(screen.cursor_column(), 7) << "and its cursor";
}

TEST(OuiTerminalScreenTests, ScrollsWithinTheRegionAProgramSets)
{
    TerminalScreen screen{10, 4};
    screen.feed("top\r\none\r\ntwo\r\nbottom");
    screen.feed("\x1b[2;3r\x1b[3;1H\n");
    EXPECT_EQ(row_text(screen, 0), U"top");
    EXPECT_EQ(row_text(screen, 1), U"two");
    EXPECT_EQ(row_text(screen, 2), U"");
    EXPECT_EQ(row_text(screen, 3), U"bottom") << "outside the region, it stays";
}

TEST(OuiTerminalScreenTests, AnswersWhatAProgramAsks)
{
    TerminalScreen screen{10, 3};
    screen.feed("ab\x1b[6n");
    EXPECT_EQ(screen.take_reply(), "\x1b[1;3R") << "where the cursor is";
    EXPECT_EQ(screen.take_reply(), "") << "once";
    screen.feed("\x1b[c");
    EXPECT_EQ(screen.take_reply(), "\x1b[?1;2c");
    screen.feed("\x1b]0;my title\x07\x1b[?1h\x1b[?2004h\x1b[?25l");
    EXPECT_EQ(screen.title(), "my title");
    EXPECT_TRUE(screen.application_cursor_keys());
    EXPECT_TRUE(screen.bracketed_paste());
    EXPECT_FALSE(screen.cursor_visible());
}

TEST(OuiTerminalScreenTests, KeepsTheTextWhenResized)
{
    TerminalScreen screen{10, 3};
    screen.feed("one\r\ntwo\r\nthree");
    screen.resize(6, 2);
    EXPECT_EQ(row_text(screen, 0), U"two");
    EXPECT_EQ(row_text(screen, 1), U"three");
    EXPECT_EQ(row_text(screen, 0, 1), U"one") << "the top line went to the history";
    screen.resize(12, 4);
    EXPECT_EQ(screen.columns(), 12);
    EXPECT_EQ(row_text(screen, 1), U"three");
}

} // namespace
