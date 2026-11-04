#include <paint/panels/icons.h>

#include <algorithm>
#include <cctype>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace olagarro::paint
{

// Defined in the source CMake generates from the icons it builds in.
std::vector<std::string_view> embedded_paint_icons();
std::vector<std::string_view> embedded_paint_icons_names();

namespace
{

// The Godot editor's own table for its light themes, from editor/themes/editor_color_map.cpp: the colours its icons
// use, and the ones they take on a light interface. Colours not here stay as they are.
const std::unordered_map<std::string, std::string>& light_colors()
{
    static const std::unordered_map<std::string, std::string> colors{
        {"#ffffff", "#414141"}, {"#000000", "#bfbfbf"}, {"#e0e0e0", "#5a5a5a"}, {"#b3b3b3", "#363636"},
        {"#f9f9f9", "#606060"}, {"#5fb2ff", "#0079f0"}, {"#003e7a", "#2b74bb"}, {"#f7f5cf", "#615f3a"},
        {"#c38ef1", "#a85de9"}, {"#8da5f3", "#3d64dd"}, {"#7582a8", "#6d83c8"}, {"#fc7f7f", "#cd3838"},
        {"#b56d6d", "#be6a6a"}, {"#99c4ff", "#4589e6"}, {"#869ebf", "#7097cd"}, {"#ffa6bd", "#e65c7f"},
        {"#bf909c", "#cd8b9c"}, {"#8eef97", "#2fa139"}, {"#76ad7b", "#64a66a"}, {"#f0caa0", "#844b0e"},
        {"#ff4545", "#ff2929"}, {"#ffe345", "#ffe337"}, {"#80ff45", "#74ff34"}, {"#45ffa2", "#2cff98"},
        {"#45d7ff", "#22ccff"}, {"#8045ff", "#702aff"}, {"#ff4596", "#ff2781"}, {"#e1da5b", "#d6cf4b"},
        {"#ffca5f", "#fea900"}, {"#2998ff", "#68b6ff"}, {"#a2d2ff", "#4998e3"}, {"#69c4d4", "#29a3cc"},
        {"#d6d6d6", "#474747"}, {"#474747", "#d6d6d6"}, {"#919191", "#6e6e6e"},
    };
    return colors;
}

[[nodiscard]] bool is_hex(char character)
{
    return std::isxdigit(static_cast<unsigned char>(character)) != 0;
}

// "#fff" as "#ffffff", in lower case.
[[nodiscard]] std::string full_color(std::string_view color)
{
    std::string full{"#"};
    for(std::size_t index{1}; index < color.size(); ++ index)
    {
        const auto digit{static_cast<char>(std::tolower(static_cast<unsigned char>(color[index])))};
        full += digit;
        if(color.size() == 4)
        {
            full += digit;
        }
    }
    return full;
}

[[nodiscard]] std::optional<std::string_view> built_in(std::string_view name)
{
    static const auto sources{embedded_paint_icons()};
    static const auto names{embedded_paint_icons_names()};
    for(std::size_t index{}; index < names.size(); ++ index)
    {
        const auto file{names[index]};
        if(file.size() == name.size() + 4 && file.substr(0, name.size()) == name && file.substr(name.size()) == ".svg")
        {
            return sources[index];
        }
    }
    return std::nullopt;
}

} // namespace

std::string for_light_interface(std::string_view svg)
{
    std::string result;
    result.reserve(svg.size());
    std::size_t index{};
    while(index < svg.size())
    {
        if(svg[index] != '#')
        {
            result += svg[index];
            ++ index;
            continue;
        }
        auto end{index + 1};
        while(end < svg.size() && is_hex(svg[end]))
        {
            ++ end;
        }
        const auto color{svg.substr(index, end - index)};
        if(color.size() == 4 || color.size() == 7)
        {
            const auto& colors{light_colors()};
            const auto found{colors.find(full_color(color))};
            result += found != colors.end() ? found->second : std::string{color};
        }
        else
        {
            result += color;
        }
        index = end;
    }
    return result;
}

void Icons::add(std::string name, std::string svg)
{
    m_added.insert_or_assign(std::move(name), std::move(svg));
    m_drawn.clear();
}

std::optional<std::string_view> Icons::source_of(std::string_view name) const
{
    if(const auto found{m_added.find(name)}; found != m_added.end())
    {
        return found->second;
    }
    return built_in(name);
}

void Icons::set_dark_interface(bool dark)
{
    if(dark != m_dark)
    {
        m_dark = dark;
        m_drawn.clear();
    }
}

const oui::Picture* Icons::picture(std::string_view name, int width, int height, std::optional<oui::Color> background)
{
    // Light, as Godot draws them, over a dark interface or a dark background.
    auto dark{m_dark};
    if(background)
    {
        dark = 0.299 * background->red + 0.587 * background->green + 0.114 * background->blue < 128.0;
    }
    const auto key{std::make_tuple(std::string{name}, width, height, dark)};
    if(const auto found{m_drawn.find(key)}; found != m_drawn.end())
    {
        return &found->second;
    }
    const auto source{source_of(name)};
    if(!source || width <= 0 || height <= 0)
    {
        return nullptr;
    }
    // Godot's icons are 16 pixels a side: a whole multiple of that keeps their edges on pixels.
    constexpr int native{16};
    const auto room{std::min(width, height)};
    const auto side{room >= native ? room / native * native : room};
    auto drawn{oui::draw_svg(dark ? std::string{*source} : for_light_interface(*source), side, side)};
    if(!drawn)
    {
        return nullptr;
    }
    oui::Picture placed{width, height, std::vector<std::uint32_t>(static_cast<std::size_t>(width) * height)};
    const auto left{(width - side) / 2};
    const auto top{(height - side) / 2};
    for(int y{}; y < side; ++ y)
    {
        std::copy_n(&drawn->pixels[static_cast<std::size_t>(y * side)], side,
                    &placed.pixels[static_cast<std::size_t>((top + y) * width + left)]);
    }
    return &m_drawn.emplace(key, std::move(placed)).first->second;
}

} // namespace olagarro::paint
