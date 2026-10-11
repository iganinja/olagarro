#pragma once

#include <oui/utf8.h>

#include <cstddef>
#include <string_view>
#include <utility>

namespace olagarro::oui
{

namespace word_boundary
{

enum class CharacterClass
{
    space,
    word,
    punctuation
};

// Letters, digits, underscore, and every non-ASCII codepoint form words; the rest is punctuation.
[[nodiscard]] inline CharacterClass classify(char32_t codepoint)
{
    if(codepoint == U' ' || codepoint == U'\t' || codepoint == U'\n' || codepoint == U'\r')
    {
        return CharacterClass::space;
    }
    if((codepoint >= U'a' && codepoint <= U'z') || (codepoint >= U'A' && codepoint <= U'Z') ||
       (codepoint >= U'0' && codepoint <= U'9') || codepoint == U'_' || codepoint >= 0x80)
    {
        return CharacterClass::word;
    }
    return CharacterClass::punctuation;
}

[[nodiscard]] inline std::size_t previous_codepoint_start(std::string_view text, std::size_t index)
{
    do
    {
        -- index;
    } while(index > 0 && (static_cast<unsigned char>(text.at(index)) & 0xc0) == 0x80);
    return index;
}

// Where Ctrl+Right lands: past any whitespace, then past one run of word or punctuation characters.
[[nodiscard]] inline std::size_t next(std::string_view text, std::size_t index)
{
    auto run_class{CharacterClass::space};
    while(index < text.size())
    {
        auto following{index};
        const auto current{classify(decode_utf8(text, following))};
        if(current != CharacterClass::space && run_class == CharacterClass::space)
        {
            run_class = current;
        }
        else if(current != run_class)
        {
            break;
        }
        index = following;
    }
    return index;
}

// Where Ctrl+Left lands: back over any whitespace, then to the start of the word or punctuation run.
[[nodiscard]] inline std::size_t previous(std::string_view text, std::size_t index)
{
    auto run_class{CharacterClass::space};
    while(index > 0)
    {
        const auto preceding{previous_codepoint_start(text, index)};
        auto cursor{preceding};
        const auto current{classify(decode_utf8(text, cursor))};
        if(current != CharacterClass::space && run_class == CharacterClass::space)
        {
            run_class = current;
        }
        else if(current != run_class)
        {
            break;
        }
        index = preceding;
    }
    return index;
}

// The run of word, punctuation, or blank characters around `index`, without crossing a line break: what a
// double click selects. At a line break or the end of the text it looks at the character before.
[[nodiscard]] inline std::pair<std::size_t, std::size_t> run_at(std::string_view text, std::size_t index)
{
    auto probe{index};
    if(probe >= text.size() || text.at(probe) == '\n')
    {
        if(probe == 0 || text.at(probe - 1) == '\n')
        {
            return {index, index};
        }
        probe = previous_codepoint_start(text, probe);
    }
    auto probe_end{probe};
    const auto run_class{classify(decode_utf8(text, probe_end))};
    auto start{probe};
    while(start > 0)
    {
        const auto before{previous_codepoint_start(text, start)};
        auto after{before};
        const auto codepoint{decode_utf8(text, after)};
        if(codepoint == U'\n' || classify(codepoint) != run_class)
        {
            break;
        }
        start = before;
    }
    auto end{probe_end};
    while(end < text.size())
    {
        auto after{end};
        const auto codepoint{decode_utf8(text, after)};
        if(codepoint == U'\n' || classify(codepoint) != run_class)
        {
            break;
        }
        end = after;
    }
    return {start, end};
}

} // namespace word_boundary

} // namespace olagarro::oui
