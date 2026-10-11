#include <oui/cell_buffer.h>
#include <oui/diff_view.h>
#include <oui/turbo_c_theme.h>

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <utility>

using namespace olagarro;

namespace
{

using oui::DiffSide;
using oui::Key;
using oui::KeyEvent;

// A comparison 60 columns by 8 rows, drawn in Turbo's colours: the bar of names on row 0, rows of text from 1 to 6,
// the bar of differences on row 7. Each side is 29 columns, the left one from 0 and the right one from 30.
struct Drawn
{
    oui::DiffView view;
    oui::Theme theme{oui::TurboCTheme::make()};
    oui::CellBuffer buffer{60, 8};

    Drawn(std::string left, std::string right)
    {
        view.set_titles("left.txt", "right.txt");
        view.set_texts(std::move(left), std::move(right));
        view.arrange({0, 0, 60, 8});
        draw();
    }

    void draw()
    {
        buffer = oui::CellBuffer{60, 8};
        oui::CellBufferPainter painter{buffer};
        view.paint(painter, theme);
    }

    [[nodiscard]] std::u32string row(int y) const
    {
        std::u32string text;
        for(int x{}; x < buffer.width(); ++ x)
        {
            text += buffer.cell_at(x, y).glyph;
        }
        return text;
    }

    // The cell where the text first shows on a row, from a column on.
    [[nodiscard]] const oui::Cell& at(int y, std::u32string_view text, int from = 0) const
    {
        const auto x{row(y).find(text, static_cast<std::size_t>(from))};
        EXPECT_NE(x, std::u32string::npos) << "no " << std::string(text.begin(), text.end()) << " on row " << y;
        return buffer.cell_at(x == std::u32string::npos ? 0 : static_cast<int>(x), y);
    }
};

} // namespace

TEST(OuiDiffViewTests, NamesBothSidesAndShowsTheLinesOfEachFacingTheOther)
{
    Drawn drawn{"one\ntwo\n", "one\ntwo\n"};
    EXPECT_NE(drawn.row(0).find(U"left.txt"), std::u32string::npos);
    EXPECT_NE(drawn.row(0).find(U"right.txt", 30), std::u32string::npos);
    EXPECT_NE(drawn.row(1).find(U"one"), std::u32string::npos);
    EXPECT_NE(drawn.row(1).find(U"one", 30), std::u32string::npos);
    EXPECT_EQ(drawn.at(2, U"two").background, drawn.theme.editor_background);
    EXPECT_NE(drawn.row(7).find(U"The two texts are the same."), std::u32string::npos);
}

TEST(OuiDiffViewTests, ALineRemovedIsRedOnTheLeftAndFacesNothingOnTheRight)
{
    Drawn drawn{"one\ngone\nthree\n", "one\nthree\n"};
    EXPECT_EQ(drawn.at(2, U"gone").background, drawn.theme.diff_removed_background);
    // Across its side, not only under its text.
    EXPECT_EQ(drawn.buffer.cell_at(25, 2).background, drawn.theme.diff_removed_background);
    EXPECT_EQ(drawn.buffer.cell_at(40, 2).background, drawn.theme.diff_filler_background);
    EXPECT_NE(drawn.row(3).find(U"three", 30), std::u32string::npos) << "the lines after it face each other again";
}

TEST(OuiDiffViewTests, ALineAddedIsGreenOnTheRight)
{
    Drawn drawn{"one\n", "one\nnew\n"};
    EXPECT_EQ(drawn.at(2, U"new", 30).background, drawn.theme.diff_added_background);
    EXPECT_EQ(drawn.buffer.cell_at(10, 2).background, drawn.theme.diff_filler_background);
}

TEST(OuiDiffViewTests, AChangedLineMarksTheWordsThatChangedOnEachSide)
{
    Drawn drawn{"int count = total + 1;\n", "int count = sum + 1;\n"};
    EXPECT_EQ(drawn.at(1, U"count").background, drawn.theme.diff_changed_background);
    EXPECT_EQ(drawn.at(1, U"total").background, drawn.theme.diff_removed_background);
    EXPECT_EQ(drawn.at(1, U"sum", 30).background, drawn.theme.diff_added_background);
    EXPECT_EQ(drawn.at(1, U"count", 30).background, drawn.theme.diff_changed_background);
}

TEST(OuiDiffViewTests, F7GoesFromOneDifferenceToTheNextAndShiftF7Back)
{
    Drawn drawn{"a\nb\nc\nd\ne\nf\n", "a\nB\nc\nd\nE\nf\n"};
    EXPECT_EQ(drawn.view.difference_count(), 2u);
    EXPECT_NE(drawn.row(7).find(U"2 differences"), std::u32string::npos);
    EXPECT_TRUE(drawn.view.handle_event(KeyEvent{Key::f7}));
    EXPECT_EQ(drawn.view.current_row(), 1u);
    EXPECT_TRUE(drawn.view.handle_event(KeyEvent{Key::f7}));
    EXPECT_EQ(drawn.view.current_row(), 4u);
    drawn.draw();
    EXPECT_NE(drawn.row(7).find(U"Difference 2 of 2"), std::u32string::npos);
    // There is no third: it stays.
    EXPECT_TRUE(drawn.view.handle_event(KeyEvent{Key::f7}));
    EXPECT_EQ(drawn.view.current_row(), 4u);
    EXPECT_TRUE(drawn.view.handle_event(KeyEvent{Key::f7, false, false, true}));
    EXPECT_EQ(drawn.view.current_row(), 1u);
}

TEST(OuiDiffViewTests, EnterSaysWhichLineOfWhichSideWasChosen)
{
    Drawn drawn{"a\nb\n", "a\nx\nb\n"};
    std::optional<std::pair<DiffSide, std::optional<std::size_t>>> chosen;
    auto connection{drawn.view.on_activated([&chosen](DiffSide side, std::optional<std::size_t> line)
                                            { chosen = std::pair{side, line}; })};
    drawn.view.next_difference();
    EXPECT_TRUE(drawn.view.handle_event(KeyEvent{Key::enter}));
    ASSERT_TRUE(chosen);
    EXPECT_EQ(chosen->first, DiffSide::right);
    EXPECT_EQ(chosen->second, std::optional<std::size_t>{1});
}

TEST(OuiDiffViewTests, LeftAndRightShowMoreOfLongLines)
{
    const std::string longer{"start " + std::string(60, '.') + " finish\n"};
    Drawn drawn{longer, longer};
    EXPECT_EQ(drawn.row(1).find(U"finish"), std::u32string::npos);
    for(int press{}; press < 20; ++ press)
    {
        drawn.view.handle_event(KeyEvent{Key::right});
    }
    drawn.draw();
    EXPECT_NE(drawn.row(1).find(U"finish"), std::u32string::npos);
    EXPECT_EQ(drawn.row(1).find(U"start"), std::u32string::npos);
}

TEST(OuiDiffViewTests, ADifferenceGoneToShowsNearTheTopWithWhatIsBeforeIt)
{
    // Far below what shows at first: F7 puts it a few rows from the top, so the rows after it show too.
    std::string left;
    std::string right;
    for(int line{}; line < 100; ++ line)
    {
        left += "line " + std::to_string(line) + "\n";
        right += (line == 60 ? "changed " : "line ") + std::to_string(line) + "\n";
    }
    Drawn drawn{left, right};
    drawn.view.next_difference();
    drawn.draw();
    // Rows of text are 1 to 6: the change is on the third, after two rows of what comes before it.
    EXPECT_NE(drawn.row(3).find(U"changed 60", 30), std::u32string::npos);
    EXPECT_NE(drawn.row(1).find(U"line 58"), std::u32string::npos);
}
