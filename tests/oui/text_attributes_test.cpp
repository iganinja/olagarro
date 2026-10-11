#include <oui/backend.h>
#include <oui/cell_buffer.h>
#include <oui/clipboard.h>
#include <oui/color.h>
#include <oui/label.h>

#include <gtest/gtest.h>

#include <array>
#include <unordered_set>

#include <cstdint>
#include <cstdlib>
#include <string>

#include "../../oui/src/terminal_backend_helpers.h"

using namespace olagarro;

namespace
{

using oui::Cell;
using oui::CellBuffer;
using oui::CellBufferPainter;
using oui::Color;
using oui::text_bold;
using oui::text_dim;
using oui::text_italic;
using oui::text_underline;

TEST(OuiTextAttributeTests, DrawTextPropagatesAttributesToGlyphCells)
{
    CellBuffer buffer{8, 1};
    CellBufferPainter painter{buffer};
    painter.draw_text({0, 0}, "Hi",
                      {Color{255, 255, 255}, Color{0, 0, 0}, static_cast<std::uint8_t>(text_bold | text_underline)});

    EXPECT_EQ(buffer.cell_at(0, 0).glyph, U'H');
    EXPECT_EQ(buffer.cell_at(0, 0).attributes, text_bold | text_underline);
    EXPECT_EQ(buffer.cell_at(1, 0).glyph, U'i');
    EXPECT_EQ(buffer.cell_at(1, 0).attributes, text_bold | text_underline);
}

TEST(OuiTextAttributeTests, DrawTextPropagatesAttributesToWideGlyphContinuation)
{
    CellBuffer buffer{8, 1};
    CellBufferPainter painter{buffer};
    painter.draw_text({0, 0}, "\U00004e16", {Color{255, 255, 255}, Color{0, 0, 0}, text_bold});

    ASSERT_EQ(buffer.cell_at(0, 0).attributes, text_bold);
    ASSERT_TRUE(buffer.cell_at(1, 0).continuation);
    EXPECT_EQ(buffer.cell_at(1, 0).attributes, text_bold);
}

TEST(OuiTextAttributeTests, PlainCellsCarryNoAttributes)
{
    CellBuffer buffer{4, 1};
    CellBufferPainter painter{buffer};
    painter.draw_text({0, 0}, "ok", {});

    EXPECT_EQ(buffer.cell_at(0, 0).attributes, 0);
}

// Every style starts with a reset: a run is only written when it changed, so a plain run drawn after a
// bold one would otherwise stay bold on the terminal.
TEST(OuiTextAttributeTests, AnsiStyleEmitsAttributeCodesBeforeTheColors)
{
    const Cell plain{};
    EXPECT_EQ(oui::terminal_helpers::style(plain), "\x1b[0;38;2;255;255;255;48;2;0;0;0m");

    Cell styled{};
    styled.attributes = text_bold | text_italic | text_underline;
    EXPECT_EQ(oui::terminal_helpers::style(styled), "\x1b[0;1;3;4;38;2;255;255;255;48;2;0;0;0m");
}

TEST(OuiTextAttributeTests, AnsiStyleEmitsDimCode)
{
    Cell dim{};
    dim.attributes = text_dim;
    EXPECT_EQ(oui::terminal_helpers::style(dim), "\x1b[0;2;38;2;255;255;255;48;2;0;0;0m");
}

TEST(OuiTextAttributeTests, ReducedStyleEmitsAttributeCodes)
{
    const Cell plain{};
    EXPECT_EQ(oui::terminal_helpers::reduced_style(plain), "\x1b[0;97;40m");

    Cell bold{};
    bold.attributes = text_bold;
    EXPECT_EQ(oui::terminal_helpers::reduced_style(bold), "\x1b[0;1;97;40m");
}

TEST(OuiTextAttributeTests, BoldLabelMarksItsCells)
{
    auto heading{oui::label("Editor")};
    EXPECT_FALSE(heading->is_bold());
    heading->set_bold(true);
    heading->arrange({0, 0, 20, 1});
    CellBuffer buffer{20, 1};
    CellBufferPainter painter{buffer};
    heading->paint(painter, {});

    ASSERT_TRUE(heading->is_bold());
    EXPECT_EQ(buffer.cell_at(0, 0).glyph, U'E');
    EXPECT_EQ(buffer.cell_at(0, 0).attributes & text_bold, text_bold);

    heading->set_bold(false);
    CellBuffer plain_buffer{20, 1};
    CellBufferPainter plain_painter{plain_buffer};
    heading->paint(plain_painter, {});

    EXPECT_FALSE(heading->is_bold());
    EXPECT_EQ(plain_buffer.cell_at(0, 0).attributes, 0);
}

TEST(OuiTextAttributeTests, WithVisibleForegroundSeparatesCollidingPlanes)
{
    EXPECT_EQ(oui::terminal_helpers::with_visible_foreground(0x12), 0x12)
        << "already-distinct attributes must pass through unchanged";
    EXPECT_EQ(oui::terminal_helpers::with_visible_foreground(0x11), 0x1f)
        << "dark background collision must get a bright white foreground";
    EXPECT_EQ(oui::terminal_helpers::with_visible_foreground(0xff), 0xf7)
        << "bright background collision must get a plain white foreground";

    for(int word{0}; word <= 0xff; ++ word)
    {
        const auto result{oui::terminal_helpers::with_visible_foreground(static_cast<std::uint16_t>(word))};
        const auto foreground{result & 0x000f};
        const auto background{(result & 0x00f0) >> 4};
        EXPECT_NE(foreground, background) << "attributes 0x" << std::hex << word << " still collapse planes";
    }
}

TEST(OuiTerminalHelperTests, Osc52CarriesTheTextInBase64)
{
    EXPECT_EQ(oui::terminal_helpers::osc52_clipboard("mundo"), "\x1b]52;c;bXVuZG8=\x07");
    EXPECT_EQ(oui::terminal_helpers::osc52_clipboard("ñ"), "\x1b]52;c;w7E=\x07");
}

namespace
{

// Remembers what it was asked to put on the terminal's clipboard.
class ClipboardProbe final : public oui::Backend
{
  public:
    [[nodiscard]] oui::Size size() const override { return {10, 2}; }
    std::optional<oui::Event> poll_event() override { return std::nullopt; }
    void present(const oui::CellBuffer&) override {}
    void set_clipboard_text(std::string_view text) override { offered = text; }
    std::string offered;
};

} // namespace

TEST(OuiClipboardTests, OffersCopiedTextToTheBackendAndKeepsItForPasting)
{
#ifdef _WIN32
    GTEST_SKIP() << "it would replace the Windows clipboard of whoever runs the tests";
#else
    // Without a desktop session no helper is found, so the user's real clipboard is left alone.
    unsetenv("DISPLAY");
    unsetenv("WAYLAND_DISPLAY");
#endif
    ClipboardProbe probe;
    oui::Clipboard clipboard{&probe};
    EXPECT_TRUE(clipboard.set_text("copied"));
    EXPECT_EQ(probe.offered, "copied");
    EXPECT_EQ(clipboard.text(), "copied");
}

} // namespace

TEST(OuiTextAttributeTests, ShadowHalfBlocksGoBlankInAFontWithoutThem)
{
    using oui::terminal_helpers::glyph_for_font;
    // Like the Linux console's Lat15 font: box drawing and whole blocks, no half blocks.
    const std::unordered_set<char32_t> console{U'─', U'█', U'é'};
    EXPECT_EQ(glyph_for_font(U'▀', console), U' ');
    EXPECT_EQ(glyph_for_font(U'▄', console), U' ');
    EXPECT_EQ(glyph_for_font(U'▄', {U'▄'}), U'▄');
    EXPECT_EQ(glyph_for_font(U'─', console), U'─');
    EXPECT_EQ(glyph_for_font(U'a', console), U'a');
    // A character OUI knows no stand-in for goes through, for the terminal to show as best it can.
    EXPECT_EQ(glyph_for_font(U'▶', console), U'▶');
}

TEST(OuiTextAttributeTests, TheLinuxConsoleShowsWhiteBackgroundsAsCyanRatherThanGray)
{
    using oui::terminal_helpers::console_style;
    const auto background_code{[](oui::Color background)
                               {
                                   const auto sequence{console_style({U'x', {0, 0, 0}, background})};
                                   return sequence.substr(sequence.rfind(';') + 1);
                               }};
    // White would otherwise fall to light gray, the dialog's own colour.
    EXPECT_EQ(background_code({255, 255, 255}), "46m");
    EXPECT_EQ(background_code({170, 170, 170}), "47m");
    EXPECT_EQ(background_code({85, 255, 85}), "42m");
    EXPECT_EQ(background_code({0, 0, 170}), "44m");
    // Foregrounds keep their bright colours, which the console does show.
    EXPECT_NE(console_style({U'x', {255, 255, 85}, {0, 0, 170}}).find(";93;"), std::string::npos);
}

namespace
{

// A terminal that said 256color got 24-bit colours, which some of those, as macOS's Terminal, cannot show.
TEST(OuiTerminalColourTests, ChoosesTheColoursATerminalSaysItHas)
{
    using oui::terminal_helpers::colour_mode;
    using oui::terminal_helpers::ColourMode;
    EXPECT_EQ(colour_mode("truecolor", "xterm-256color"), ColourMode::true_colour);
    EXPECT_EQ(colour_mode("24bit", "xterm"), ColourMode::true_colour);
    EXPECT_EQ(colour_mode(nullptr, "xterm-256color"), ColourMode::palette_256);
    EXPECT_EQ(colour_mode("", "screen-256color"), ColourMode::palette_256);
    EXPECT_EQ(colour_mode(nullptr, "xterm"), ColourMode::console_16);
    EXPECT_EQ(colour_mode(nullptr, nullptr), ColourMode::console_16);
}

TEST(OuiTerminalColourTests, WritesAColourAsTheNearestOfTheTwoHundredAndFiftySix)
{
    using oui::terminal_helpers::palette_index;
    EXPECT_EQ(palette_index({0, 0, 0}), 16);
    EXPECT_EQ(palette_index({255, 255, 255}), 231);
    EXPECT_EQ(palette_index({255, 0, 0}), 196);
    EXPECT_EQ(palette_index({0, 0, 170}), 19) << "the console's blue, as near as the cube has it";
    EXPECT_EQ(palette_index({128, 128, 128}), 244) << "a grey from the ramp, nearer than the cube's";
    Cell cell;
    cell.foreground = {255, 255, 255};
    cell.background = {0, 0, 170};
    cell.attributes = text_bold;
    EXPECT_EQ(oui::terminal_helpers::palette_style(cell), "\x1b[0;1;38;5;231;48;5;19m");
}

} // namespace

namespace
{

// Each channel was on above 127: the console's brown, 170, 85, 0, came out red, and Amber's surfaces with it.
TEST(OuiTerminalColourTests, WritesEachOfTheSixteenAsItself)
{
    using oui::terminal_helpers::ansi_color;
    const std::array<Color, 16> sixteen{{{0, 0, 0},
                                         {170, 0, 0},
                                         {0, 170, 0},
                                         {170, 85, 0},
                                         {0, 0, 170},
                                         {170, 0, 170},
                                         {0, 170, 170},
                                         {170, 170, 170},
                                         {85, 85, 85},
                                         {255, 85, 85},
                                         {85, 255, 85},
                                         {255, 255, 85},
                                         {85, 85, 255},
                                         {255, 85, 255},
                                         {85, 255, 255},
                                         {255, 255, 255}}};
    for(std::size_t index{}; index < sixteen.size(); ++ index)
    {
        EXPECT_EQ(ansi_color(sixteen[index]),
                  index < 8 ? 30 + static_cast<int>(index) : 90 + static_cast<int>(index) - 8)
            << index;
    }
    EXPECT_EQ(ansi_color({200, 90, 10}), 33) << "a colour between them takes the nearest";
}

} // namespace

namespace
{

// The legacy Windows console took a channel above 31 as on: dark grey came out light grey and light green white.
TEST(OuiTerminalColourTests, GivesTheLegacyWindowsConsoleTheSameSixteen)
{
    using oui::terminal_helpers::windows_console_attribute;
    // Its bits: blue 1, green 2, red 4, intensity 8; the background's four above.
    EXPECT_EQ(windows_console_attribute({0, 0, 170}, false), 0x1);
    EXPECT_EQ(windows_console_attribute({170, 85, 0}, false), 0x6) << "brown, the console's dark yellow";
    EXPECT_EQ(windows_console_attribute({85, 85, 85}, false), 0x8);
    EXPECT_EQ(windows_console_attribute({85, 255, 85}, false), 0xa);
    EXPECT_EQ(windows_console_attribute({255, 255, 255}, false), 0xf);
    EXPECT_EQ(windows_console_attribute({0, 170, 170}, true), 0x30);
    EXPECT_EQ(windows_console_attribute({255, 85, 85}, true), 0xc0);
}

} // namespace
