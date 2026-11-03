#include <paint/color.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace olagarro::paint
{

std::string to_hex(const Color& color)
{
    const auto byte{[](double value) { return static_cast<int>(std::lround(std::clamp(value, 0.0, 1.0) * 255.0)); }};
    char text[16];
    if(color.alpha >= 1.0)
    {
        std::snprintf(text, sizeof text, "#%02x%02x%02x", byte(color.red), byte(color.green), byte(color.blue));
    }
    else
    {
        std::snprintf(text, sizeof text, "#%02x%02x%02x%02x", byte(color.red), byte(color.green), byte(color.blue),
                      byte(color.alpha));
    }
    return text;
}

std::optional<Color> from_hex(std::string_view text)
{
    if(!text.empty() && text.front() == '#')
    {
        text.remove_prefix(1);
    }
    if(text.size() != 6 && text.size() != 8)
    {
        return std::nullopt;
    }
    unsigned values[4]{0, 0, 0, 255};
    for(std::size_t index{}; index < text.size() / 2; ++ index)
    {
        const std::string pair{text.substr(index * 2, 2)};
        char* end{};
        values[index] = static_cast<unsigned>(std::strtoul(pair.c_str(), &end, 16));
        if(*end != '\0')
        {
            return std::nullopt;
        }
    }
    return Color{values[0] / 255.0, values[1] / 255.0, values[2] / 255.0, values[3] / 255.0};
}

} // namespace olagarro::paint
