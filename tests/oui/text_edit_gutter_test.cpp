#include <oui/cell_buffer.h>
#include <oui/text_edit.h>
#include <oui/theme.h>

#include <gtest/gtest.h>

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

} // namespace
