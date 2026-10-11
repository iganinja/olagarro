#include <oui/cell_buffer.h>
#include <oui/line_edit.h>
#include <oui/text_edit.h>
#include <oui/theme.h>

#include <gtest/gtest.h>

#include "../../oui/src/unicode_composition.h"

using namespace olagarro;

namespace
{

TEST(OuiUnicodeCompositionTests, ComposesLettersWithTheRightAccent)
{
    EXPECT_EQ(oui::compose(U'o', U'̂'), U'ô');
    EXPECT_EQ(oui::compose(U'n', U'̃'), U'ñ');
    EXPECT_EQ(oui::compose(U'A', U'̀'), U'À');
    EXPECT_EQ(oui::compose(U'y', U'̈'), U'ÿ');
    // These used to map to the wrong letter.
    EXPECT_EQ(oui::compose(U'a', U'̌'), U'ǎ');
    EXPECT_EQ(oui::compose(U'u', U'̌'), U'ǔ');
    EXPECT_EQ(oui::compose(U'y', U'̄'), U'ȳ');
    EXPECT_FALSE(oui::compose(U'q', U'̂'));
}

TEST(OuiUnicodeCompositionTests, TextEditMergesATypedCombiningAccentIntoItsLetter)
{
    oui::TextEdit editor;
    editor.handle_event(oui::TextEvent{U'o'});
    editor.handle_event(oui::TextEvent{U'̂'});
    editor.handle_event(oui::TextEvent{U'q'});
    editor.handle_event(oui::TextEvent{U'̂'});
    EXPECT_EQ(editor.text(), "ôq̂");
}

TEST(OuiUnicodeCompositionTests, LineEditMergesATypedCombiningAccentIntoItsLetter)
{
    oui::LineEdit edit;
    edit.handle_event(oui::TextEvent{U'n'});
    edit.handle_event(oui::TextEvent{U'̃'});
    EXPECT_EQ(edit.text(), "ñ");
}

TEST(OuiUnicodeCompositionTests, PainterDrawsDecomposedTextPrecomposed)
{
    oui::CellBuffer buffer{4, 1};
    oui::CellBufferPainter painter{buffer};
    painter.draw_text({0, 0}, "ôñ!", {});
    EXPECT_EQ(buffer.cell_at(0, 0).glyph, U'ô');
    EXPECT_EQ(buffer.cell_at(1, 0).glyph, U'ñ');
    EXPECT_EQ(buffer.cell_at(2, 0).glyph, U'!');
}

TEST(OuiUnicodeCompositionTests, TextEditPaintsDecomposedTextPrecomposed)
{
    const oui::Theme theme;
    oui::TextEdit editor;
    editor.set_text("áb");
    editor.arrange({0, 0, 10, 1});
    oui::CellBuffer buffer{10, 1};
    oui::CellBufferPainter painter{buffer};
    editor.paint(painter, theme);
    EXPECT_EQ(buffer.cell_at(0, 0).glyph, U'á');
    EXPECT_EQ(buffer.cell_at(1, 0).glyph, U'b');
}

} // namespace
