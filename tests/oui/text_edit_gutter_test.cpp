#include <oui/cell_buffer.h>
#include <oui/event.h>
#include <oui/graphics/canvas.h>
#include <oui/graphics/font.h>
#include <oui/graphics/graphical_painter.h>
#include <oui/painter.h>
#include <oui/text_edit.h>
#include <oui/theme.h>

#include <gtest/gtest.h>

#include <optional>
#include <string>

namespace
{

using namespace olagarro;
using namespace olagarro::oui;

TEST(OuiTextEditGutterTests, NumbersOnlyTheFirstRowOfAWrappedLine)
{
    TextEdit edit;
    edit.set_line_numbers_visible(true);
    edit.set_word_wrap(true);
    edit.set_text("one two three four five six seven eight nine ten\nshort");
    edit.arrange({0, 0, 20, 8});
    CellBuffer buffer{20, 8};
    CellBufferPainter painter{buffer};
    edit.paint(painter, Theme{});
    const auto gutter{[&buffer](int row)
                      {
                          std::u32string text;
                          for(int x{}; x < 3; ++ x)
                          {
                              text += buffer.cell_at(x, row).glyph;
                          }
                          return text;
                      }};
    EXPECT_NE(gutter(0).find(U'1'), std::u32string::npos);
    // The rows the first line wraps onto carry no number; the next line has its own.
    int row{1};
    while(row < 8 && gutter(row).find(U'2') == std::u32string::npos)
    {
        EXPECT_EQ(gutter(row).find(U'1'), std::u32string::npos) << "row " << row;
        ++ row;
    }
    EXPECT_GT(row, 1);
    EXPECT_LT(row, 8);
}

TEST(OuiTextEditGutterTests, WrapsAgainAtOnceWhenItsWidthChanges)
{
    TextEdit edit;
    edit.set_word_wrap(true);
    edit.set_text("alpha beta gamma delta epsilon zeta");
    const auto row_text{[&edit](int width, int row)
                        {
                            edit.arrange({0, 0, static_cast<double>(width), 6});
                            CellBuffer buffer{width, 6};
                            CellBufferPainter painter{buffer};
                            edit.paint(painter, Theme{});
                            // The text, without the scrollbar in the last column.
                            std::u32string text;
                            for(int x{}; x < width - 1; ++ x)
                            {
                                text += buffer.cell_at(x, row).glyph;
                            }
                            return text;
                        }};
    // Wide, the line fits on one row; narrowed, the very next frame shows it carried onto the next.
    EXPECT_EQ(row_text(60, 1).find_first_not_of(U' '), std::u32string::npos);
    EXPECT_NE(row_text(16, 1).find(U'g'), std::u32string::npos);
}

TEST(OuiTextEditGutterTests, WrappedTextNeverScrollsSideways)
{
    TextEdit edit;
    edit.set_word_wrap(true);
    edit.arrange({0, 0, 12, 6});
    // Typing to the very end of a row leaves its first letter in sight.
    for(const auto letter : std::string{"abcdefghijk"})
    {
        edit.handle_event(TextEvent{static_cast<char32_t>(letter)});
    }
    CellBuffer buffer{12, 6};
    CellBufferPainter painter{buffer};
    edit.paint(painter, Theme{});
    EXPECT_EQ(buffer.cell_at(0, 0).glyph, U'a');
}

} // namespace

namespace
{

// Where a place of the text is on screen, past the gutter, as a popup at the cursor wants it.
TEST(OuiTextEditGutterTests, TellsTheCellAPlaceOfTheTextIsIn)
{
    oui::TextEdit edit;
    edit.set_line_numbers_visible(true);
    edit.set_text("ab\ncdef");
    edit.arrange({5, 2, 30, 10});
    const auto cell{edit.cell_of(4)};
    ASSERT_TRUE(cell.has_value());
    const auto gutter{cell->x - 5 - 1};
    EXPECT_GT(gutter, 0) << "the line numbers come first";
    EXPECT_EQ(cell->y, 3);
    EXPECT_EQ(edit.cell_of(0)->x, 5 + gutter);
    edit.arrange({5, 2, 30, 1});
    EXPECT_FALSE(edit.cell_of(4).has_value()) << "the second line is out of sight";
}

[[nodiscard]] oui::Theme marking_theme()
{
    oui::Theme theme;
    theme.editor_background = {0, 0, 0};
    theme.line_number_foreground = {100, 100, 100};
    theme.error_foreground = {250, 20, 20};
    return theme;
}

// The gutter is two columns for three lines: the number and a space.
TEST(OuiTextEditGutterTests, MarksALineInTheGutterAndUnderItsText)
{
    oui::TextEdit edit;
    edit.set_line_numbers_visible(true);
    edit.set_text("one\n  two\nthree");
    EXPECT_FALSE(edit.marked_line().has_value());
    edit.set_marked_line(2);
    EXPECT_EQ(edit.marked_line(), 2u);
    edit.arrange({0, 0, 20, 4});
    oui::CellBuffer buffer{20, 4};
    oui::CellBufferPainter painter{buffer};
    const auto theme{marking_theme()};
    edit.paint(painter, theme);
    EXPECT_EQ(buffer.cell_at(0, 1).glyph, U'2');
    EXPECT_EQ(buffer.cell_at(0, 1).foreground, theme.error_foreground);
    EXPECT_EQ(buffer.cell_at(0, 0).foreground, theme.line_number_foreground);
    EXPECT_EQ(buffer.cell_at(0, 2).foreground, theme.line_number_foreground);
    // Its text underlined from its first letter, the indentation not.
    EXPECT_EQ(buffer.cell_at(4, 1).glyph, U't');
    for(int x{4}; x < 7; ++ x)
    {
        EXPECT_NE(buffer.cell_at(x, 1).attributes & oui::text_underline, 0) << "column " << x;
    }
    EXPECT_EQ(buffer.cell_at(3, 1).attributes & oui::text_underline, 0);
    EXPECT_EQ(buffer.cell_at(2, 0).attributes & oui::text_underline, 0);
    EXPECT_EQ(buffer.cell_at(2, 2).attributes & oui::text_underline, 0);
}

TEST(OuiTextEditGutterTests, TheMarkFollowsItsLineAsLinesAreAddedAndRemovedAbove)
{
    oui::TextEdit edit;
    edit.set_text("one\ntwo\nthree\nfour");
    edit.set_marked_line(3);
    edit.paste("zero\n");
    EXPECT_EQ(edit.marked_line(), 4u) << "a line added at the top";
    edit.select_range(edit.text().size(), edit.text().size());
    edit.paste("\nfive");
    EXPECT_EQ(edit.marked_line(), 4u) << "a line added after it";
    edit.select_range(0, 5);
    std::string clipboard;
    edit.cut_selection(clipboard);
    EXPECT_EQ(edit.marked_line(), 3u) << "the line removed";
    ASSERT_TRUE(edit.undo());
    EXPECT_EQ(edit.marked_line(), 4u) << "undone";
    // Enter at the start of the marked line itself takes the line down.
    ASSERT_TRUE(edit.go_to_line(4));
    edit.handle_event(oui::KeyEvent{oui::Key::enter});
    EXPECT_EQ(edit.marked_line(), 5u);
    // A change within the line leaves it.
    edit.paste("x");
    EXPECT_EQ(edit.marked_line(), 5u);
}

TEST(OuiTextEditGutterTests, TheMarkGoesWhenSetToNone)
{
    oui::TextEdit edit;
    edit.set_line_numbers_visible(true);
    edit.set_text("one\ntwo");
    edit.set_marked_line(2);
    edit.set_marked_line(std::nullopt);
    EXPECT_FALSE(edit.marked_line().has_value());
    edit.arrange({0, 0, 20, 3});
    oui::CellBuffer buffer{20, 3};
    oui::CellBufferPainter painter{buffer};
    const auto theme{marking_theme()};
    edit.paint(painter, theme);
    EXPECT_EQ(buffer.cell_at(0, 1).foreground, theme.line_number_foreground);
    EXPECT_EQ(buffer.cell_at(2, 1).attributes & oui::text_underline, 0);
}

// In pixels the line under the text is the error colour's, two pixels above the row's bottom, the text keeping its own.
TEST(OuiTextEditGutterTests, MarksALineInPixelsWithALineInTheErrorColour)
{
    auto cell{oui::default_cell_font()};
    auto interface{oui::default_interface_font()};
    auto symbols{oui::symbol_font(12, 24, cell->ascent())};
    oui::Canvas canvas{240, 96};
    const auto style{oui::classic_style()};
    oui::GraphicalPainter painter{canvas, style, *cell, *interface, *symbols};
    oui::TextEdit edit;
    edit.set_line_numbers_visible(true);
    edit.set_text("one\ntwo\nthree");
    edit.set_marked_line(2);
    edit.arrange({0, 0, 20, 4});
    const auto theme{marking_theme()};
    edit.paint(painter, theme);
    // The text starts at column 2, 24 pixels in; "two" ends at column 5, pixel 60.
    for(int x{24}; x < 60; ++ x)
    {
        EXPECT_EQ(canvas.pixel_at(x, 46), theme.error_foreground) << "pixel " << x << " of the marked row";
        EXPECT_NE(canvas.pixel_at(x, 22), theme.error_foreground) << "pixel " << x << " of the first row";
        EXPECT_NE(canvas.pixel_at(x, 70), theme.error_foreground) << "pixel " << x << " of the third row";
    }
    EXPECT_NE(canvas.pixel_at(64, 46), theme.error_foreground) << "past the text";
    // The number is drawn in the error colour: some of its pixels have it.
    auto marked_number{false};
    for(int y{24}; y < 48; ++ y)
    {
        for(int x{}; x < 12; ++ x)
        {
            marked_number = marked_number || canvas.pixel_at(x, y) == theme.error_foreground;
        }
    }
    EXPECT_TRUE(marked_number);
}

} // namespace
