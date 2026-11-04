#include <paint/palette.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace olagarro::paint
{

namespace
{

[[nodiscard]] int byte(double value)
{
    return static_cast<int>(std::lround(std::clamp(value, 0.0, 1.0) * 255.0));
}

[[nodiscard]] bool is_space(char character)
{
    return character == ' ' || character == '\t' || character == '\r';
}

[[nodiscard]] std::string_view trimmed(std::string_view text)
{
    while(!text.empty() && is_space(text.front()))
    {
        text.remove_prefix(1);
    }
    while(!text.empty() && is_space(text.back()))
    {
        text.remove_suffix(1);
    }
    return text;
}

// A number from 0 to 255 at the start of `text`, after spaces, taken off it.
[[nodiscard]] std::optional<int> take_byte(std::string_view& text)
{
    while(!text.empty() && is_space(text.front()))
    {
        text.remove_prefix(1);
    }
    auto value{0};
    std::size_t digits{};
    for(; digits < text.size() && text[digits] >= '0' && text[digits] <= '9' && digits < 4; ++ digits)
    {
        value = value * 10 + (text[digits] - '0');
    }
    if(digits == 0 || value > 255)
    {
        return std::nullopt;
    }
    text.remove_prefix(digits);
    return value;
}

} // namespace

Palette Palette::standard()
{
    // Greys from black to white first, as heightmaps need them, then the colours.
    Palette palette{"Default", {}};
    for(int step{}; step <= 8; ++ step)
    {
        palette.colors.push_back({step / 8.0, step / 8.0, step / 8.0, 1.0});
    }
    palette.colors.push_back({0.0, 0.0, 0.0, 0.0});
    for(const auto& color : std::vector<Color>{{1, 0, 0, 1},
                                               {1, 0.5, 0, 1},
                                               {1, 1, 0, 1},
                                               {0.5, 1, 0, 1},
                                               {0, 1, 0, 1},
                                               {0, 1, 0.5, 1},
                                               {0, 1, 1, 1},
                                               {0, 0.5, 1, 1},
                                               {0, 0, 1, 1},
                                               {0.5, 0, 1, 1},
                                               {1, 0, 1, 1},
                                               {1, 0, 0.5, 1},
                                               {0.5, 0, 0, 1},
                                               {0.5, 0.25, 0, 1},
                                               {0, 0.5, 0, 1},
                                               {0, 0, 0.5, 1}})
    {
        palette.colors.push_back(color);
    }
    return palette;
}

std::string to_gpl(const Palette& palette)
{
    std::string text{"GIMP Palette\nName: " + palette.name + "\n#\n"};
    for(const auto& color : palette.colors)
    {
        const auto clear{byte(color.alpha) == 0};
        char line[64];
        std::snprintf(line, sizeof line, "%3d %3d %3d\t%s\n", clear ? 0 : byte(color.red),
                      clear ? 0 : byte(color.green), clear ? 0 : byte(color.blue),
                      clear ? "transparent" : to_hex(color).c_str());
        text += line;
    }
    return text;
}

std::optional<Palette> palette_from_gpl(std::string_view text)
{
    constexpr std::string_view header{"GIMP Palette"};
    if(text.substr(0, header.size()) != header)
    {
        return std::nullopt;
    }
    Palette palette;
    for(auto start{text.find('\n')}; start != std::string_view::npos && start < text.size();)
    {
        const auto end{std::min(text.find('\n', start + 1), text.size())};
        auto line{trimmed(text.substr(start + 1, end - start - 1))};
        start = end;
        constexpr std::string_view name_key{"Name:"};
        if(line.substr(0, name_key.size()) == name_key)
        {
            palette.name = std::string{trimmed(line.substr(name_key.size()))};
            continue;
        }
        const auto red{take_byte(line)};
        const auto green{red ? take_byte(line) : std::nullopt};
        const auto blue{green ? take_byte(line) : std::nullopt};
        if(!blue)
        {
            // A comment, "Columns:", or a line that is no colour.
            continue;
        }
        const auto name{trimmed(line)};
        Color color{*red / 255.0, *green / 255.0, *blue / 255.0, 1.0};
        if(name == "transparent" && *red == 0 && *green == 0 && *blue == 0)
        {
            color.alpha = 0.0;
        }
        else if(const auto named{from_hex(name)};
                named && byte(named->red) == *red && byte(named->green) == *green && byte(named->blue) == *blue)
        {
            color.alpha = named->alpha;
        }
        palette.colors.push_back(color);
    }
    return palette;
}

} // namespace olagarro::paint
