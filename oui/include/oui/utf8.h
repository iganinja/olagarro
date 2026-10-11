#pragma once

#include <string>
#include <string_view>

namespace olagarro::oui
{

[[nodiscard]] inline std::string encode_utf8(char32_t codepoint)
{
    if(codepoint < 0x80)
    {
        return {static_cast<char>(codepoint)};
    }
    if(codepoint < 0x800)
    {
        return {static_cast<char>(0xc0 | (codepoint >> 6)), static_cast<char>(0x80 | (codepoint & 0x3f))};
    }
    if(codepoint < 0x10000)
    {
        return {static_cast<char>(0xe0 | (codepoint >> 12)), static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)),
                static_cast<char>(0x80 | (codepoint & 0x3f))};
    }
    return {static_cast<char>(0xf0 | (codepoint >> 18)), static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)),
            static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)), static_cast<char>(0x80 | (codepoint & 0x3f))};
}

[[nodiscard]] inline char32_t decode_utf8(std::string_view text, std::size_t& index)
{
    if(index >= text.size())
    {
        return U'\ufffd';
    }
    const auto first{static_cast<unsigned char>(text.at(index))};
    ++ index;
    if(first < 0x80)
    {
        return first;
    }
    const auto continuation{[&text, &index](unsigned char& value)
                            {
                                if(index >= text.size())
                                {
                                    return false;
                                }
                                value = static_cast<unsigned char>(text.at(index));
                                if((value & 0xc0) != 0x80)
                                {
                                    return false;
                                }
                                ++ index;
                                return true;
                            }};
    unsigned char second{};
    unsigned char third{};
    unsigned char fourth{};
    if(first >= 0xc2 && first <= 0xdf && continuation(second))
    {
        return static_cast<char32_t>((first & 0x1f) << 6 | (second & 0x3f));
    }
    if(first >= 0xe0 && first <= 0xef && continuation(second) && continuation(third))
    {
        const char32_t codepoint{static_cast<char32_t>((first & 0x0f) << 12 | (second & 0x3f) << 6 | (third & 0x3f))};
        return codepoint >= 0x800 && !(codepoint >= 0xd800 && codepoint <= 0xdfff) ? codepoint : U'\ufffd';
    }
    if(first >= 0xf0 && first <= 0xf4 && continuation(second) && continuation(third) && continuation(fourth))
    {
        const char32_t codepoint{static_cast<char32_t>((first & 0x07) << 18 | (second & 0x3f) << 12 |
                                                       (third & 0x3f) << 6 | (fourth & 0x3f))};
        return codepoint <= 0x10ffff ? codepoint : U'\ufffd';
    }
    return U'\ufffd';
}

[[nodiscard]] inline int terminal_cell_width(char32_t codepoint)
{
    if(codepoint < 0x20 || (codepoint >= 0x300 && codepoint <= 0x36f))
    {
        return 0;
    }
    if((codepoint >= 0x1100 && codepoint <= 0x115f) || (codepoint >= 0x2e80 && codepoint <= 0xa4cf) ||
       (codepoint >= 0xac00 && codepoint <= 0xd7a3) || (codepoint >= 0xf900 && codepoint <= 0xfaff) ||
       (codepoint >= 0xfe10 && codepoint <= 0xfe6f) || (codepoint >= 0xff01 && codepoint <= 0xff60) ||
       (codepoint >= 0xffe0 && codepoint <= 0xffe6))
    {
        return 2;
    }
    return 1;
}

} // namespace olagarro::oui
