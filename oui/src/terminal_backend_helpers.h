#pragma once

#include <oui/cell_buffer.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_set>

namespace olagarro::oui
{

namespace terminal_helpers
{

[[nodiscard]] inline std::string cursor_to(int x, int y)
{
    return "\x1b[" + std::to_string(y + 1) + ";" + std::to_string(x + 1) + "H";
}

// The glyph to draw with a font that has only `font`'s characters, such as the Linux console's: a stand-in
// for the few OUI draws itself, or the glyph unchanged, which the terminal then shows as best it can.
[[nodiscard]] inline char32_t glyph_for_font(char32_t glyph, const std::unordered_set<char32_t>& font)
{
    if(glyph < 0x80 || font.count(glyph) != 0)
    {
        return glyph;
    }
    switch(glyph)
    {
    // The half blocks of a button's shadow. A whole block in their place reads as a stain rather than a
    // shadow, so the button goes without one; Latin console fonts of 256 characters have no room for them.
    case 0x2580:
    case 0x2584:
        return U' ';
    default:
        return glyph;
    }
}

[[nodiscard]] inline std::string utf8(char32_t glyph)
{
    if(glyph < U' ' || (glyph >= 0x7f && glyph <= 0x9f))
    {
        return " ";
    }
    if(glyph < 0x80)
    {
        return {static_cast<char>(glyph)};
    }
    if(glyph < 0x800)
    {
        return {static_cast<char>(0xc0 | (glyph >> 6)), static_cast<char>(0x80 | (glyph & 0x3f))};
    }
    if(glyph < 0x10000)
    {
        return {static_cast<char>(0xe0 | (glyph >> 12)), static_cast<char>(0x80 | ((glyph >> 6) & 0x3f)),
                static_cast<char>(0x80 | (glyph & 0x3f))};
    }
    return {static_cast<char>(0xf0 | (glyph >> 18)), static_cast<char>(0x80 | ((glyph >> 12) & 0x3f)),
            static_cast<char>(0x80 | ((glyph >> 6) & 0x3f)), static_cast<char>(0x80 | (glyph & 0x3f))};
}

[[nodiscard]] inline std::string attribute_codes(std::uint8_t attributes)
{
    std::string result;
    if(attributes & text_bold)
    {
        result += "1;";
    }
    if(attributes & text_dim)
    {
        result += "2;";
    }
    if(attributes & text_italic)
    {
        result += "3;";
    }
    if(attributes & text_underline)
    {
        result += "4;";
    }
    return result;
}

[[nodiscard]] inline std::string style(const Cell& cell)
{
    return "\x1b[0;" + attribute_codes(cell.attributes) + "38;2;" + std::to_string(cell.foreground.red) + ";" +
           std::to_string(cell.foreground.green) + ";" + std::to_string(cell.foreground.blue) + ";48;2;" +
           std::to_string(cell.background.red) + ";" + std::to_string(cell.background.green) + ";" +
           std::to_string(cell.background.blue) + "m";
}

// What colours a terminal takes, as its environment says: 24-bit ones where COLORTERM says truecolor or 24bit, the
// way terminals tell they have them; the 256 of xterm's palette where TERM names a 256-colour terminal, some of which,
// as macOS's Terminal, show 24-bit colours wrong; and the console's 16 otherwise.
enum class ColourMode
{
    true_colour,
    palette_256,
    console_16
};

[[nodiscard]] inline ColourMode colour_mode(const char* colour_term, const char* terminal)
{
    if(colour_term != nullptr &&
       (std::string_view{colour_term} == "truecolor" || std::string_view{colour_term} == "24bit"))
    {
        return ColourMode::true_colour;
    }
    if(terminal != nullptr && std::string_view{terminal}.find("256color") != std::string_view::npos)
    {
        return ColourMode::palette_256;
    }
    return ColourMode::console_16;
}

// The nearest colour of xterm's 256: of its 6 by 6 by 6 cube, from 16, or of its 24 greys, from 232.
[[nodiscard]] inline int palette_index(Color color)
{
    constexpr std::array<int, 6> levels{0, 95, 135, 175, 215, 255};
    const auto nearest_level{[&levels](int value)
                             {
                                 auto best{0};
                                 for(int level{1}; level < 6; ++ level)
                                 {
                                     if(std::abs(levels[static_cast<std::size_t>(level)] - value) <
                                        std::abs(levels[static_cast<std::size_t>(best)] - value))
                                     {
                                         best = level;
                                     }
                                 }
                                 return best;
                             }};
    const auto distance{[](int red, int green, int blue, Color to)
                        {
                            return (red - to.red) * (red - to.red) + (green - to.green) * (green - to.green) +
                                   (blue - to.blue) * (blue - to.blue);
                        }};
    const auto red{nearest_level(color.red)};
    const auto green{nearest_level(color.green)};
    const auto blue{nearest_level(color.blue)};
    const auto cube_distance{distance(levels[static_cast<std::size_t>(red)], levels[static_cast<std::size_t>(green)],
                                      levels[static_cast<std::size_t>(blue)], color)};
    const auto average{(color.red + color.green + color.blue) / 3};
    const auto grey{std::clamp((average - 8 + 5) / 10, 0, 23)};
    const auto grey_value{8 + grey * 10};
    if(distance(grey_value, grey_value, grey_value, color) < cube_distance)
    {
        return 232 + grey;
    }
    return 16 + red * 36 + green * 6 + blue;
}

[[nodiscard]] inline std::string palette_style(const Cell& cell)
{
    return "\x1b[0;" + attribute_codes(cell.attributes) + "38;5;" + std::to_string(palette_index(cell.foreground)) +
           ";48;5;" + std::to_string(palette_index(cell.background)) + "m";
}

[[nodiscard]] inline std::uint16_t with_visible_foreground(std::uint16_t attributes)
{
    constexpr std::uint16_t foreground_mask{0x000f};
    constexpr std::uint16_t background_bright{0x0080};
    if((attributes & foreground_mask) == ((attributes & 0x00f0) >> 4))
    {
        auto result{static_cast<std::uint16_t>(attributes & static_cast<std::uint16_t>(~foreground_mask))};
        result |= (attributes & background_bright) != 0 ? std::uint16_t{0x0007} : foreground_mask;
        return result;
    }
    return attributes;
}

// Which of the sixteen colours of a console is nearest, as EGA and VGA showed them and terminals keep them: 0 to 7
// black, red, green, brown, blue, magenta, cyan and light grey, then their bright ones. The themes for characters are
// made of exactly these, so each comes out as itself, the brown and the dark grey included, which no rule by halves of
// each channel tells apart.
[[nodiscard]] inline std::size_t console_index(Color color)
{
    constexpr std::array<Color, 16> sixteen{{{0, 0, 0},
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
    std::size_t nearest{};
    auto nearest_distance{std::numeric_limits<int>::max()};
    for(std::size_t index{}; index < sixteen.size(); ++ index)
    {
        const auto red{color.red - sixteen[index].red};
        const auto green{color.green - sixteen[index].green};
        const auto blue{color.blue - sixteen[index].blue};
        if(const auto distance{red * red + green * green + blue * blue}; distance < nearest_distance)
        {
            nearest = index;
            nearest_distance = distance;
        }
    }
    return nearest;
}

// The SGR foreground code of the nearest of the sixteen: 30 to 37, and 90 to 97 for the bright ones.
[[nodiscard]] inline int ansi_color(Color color)
{
    const auto index{console_index(color)};
    return index < 8 ? 30 + static_cast<int>(index) : 90 + static_cast<int>(index) - 8;
}

// The legacy Windows console's attribute bits for the nearest of the sixteen: blue 1, green 2, red 4 and intensity 8,
// the console's order rather than the terminals', moved to the background's four bits above when `background`.
[[nodiscard]] inline std::uint16_t windows_console_attribute(Color color, bool background)
{
    const auto index{console_index(color)};
    const auto bits{
        static_cast<std::uint16_t>(((index & 1U) << 2U) | (index & 2U) | ((index & 4U) >> 2U) | (index & 8U))};
    return static_cast<std::uint16_t>(background ? bits << 4U : bits);
}

[[nodiscard]] inline std::string reduced_style(const Cell& cell)
{
    return "\x1b[0;" + attribute_codes(cell.attributes) + std::to_string(ansi_color(cell.foreground)) + ";" +
           std::to_string(ansi_color(cell.background) + 10) + "m";
}

// The style on the Linux console, which has 16 foreground colours but only the 8 dark backgrounds: it shows
// a bright background as its dark version. That turns white into light gray, the colour dialogs and menus
// are made of, and anything marked with white, such as the focused control, would vanish into them; white
// becomes cyan instead, the highlight of DOS-era text interfaces.
[[nodiscard]] inline std::string console_style(const Cell& cell)
{
    constexpr int bright_white{97};
    constexpr int cyan{36};
    const auto background{ansi_color(cell.background)};
    const auto dark_background{background == bright_white ? cyan : background >= 90 ? background - 60 : background};
    return "\x1b[0;" + attribute_codes(cell.attributes) + std::to_string(ansi_color(cell.foreground)) + ";" +
           std::to_string(dark_background + 10) + "m";
}

// The OSC 52 sequence that asks a terminal to put `text` on the clipboard.
[[nodiscard]] inline std::string osc52_clipboard(std::string_view text)
{
    static constexpr char alphabet[]{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};
    std::string sequence{"\x1b]52;c;"};
    for(std::size_t index{}; index < text.size(); index += 3)
    {
        const auto remaining{text.size() - index};
        auto chunk{static_cast<std::uint32_t>(static_cast<unsigned char>(text.at(index))) << 16};
        if(remaining > 1)
        {
            chunk |= static_cast<std::uint32_t>(static_cast<unsigned char>(text.at(index + 1))) << 8;
        }
        if(remaining > 2)
        {
            chunk |= static_cast<unsigned char>(text.at(index + 2));
        }
        sequence += alphabet[(chunk >> 18) & 0x3f];
        sequence += alphabet[(chunk >> 12) & 0x3f];
        sequence += remaining > 1 ? alphabet[(chunk >> 6) & 0x3f] : '=';
        sequence += remaining > 2 ? alphabet[chunk & 0x3f] : '=';
    }
    return sequence + '\x07';
}

} // namespace terminal_helpers

} // namespace olagarro::oui
