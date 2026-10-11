#include <oui/type_ahead.h>

#include <oui/utf8.h>

#include <algorithm>
#include <cctype>

namespace olagarro::oui
{

namespace
{

// ASCII folding keeps byte lengths, so a prefix compares byte by byte.
[[nodiscard]] std::string folded(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

} // namespace

TypeAhead::TypeAhead(std::chrono::milliseconds pause) : m_pause{pause}
{
}

std::optional<std::size_t> TypeAhead::find(char32_t character, std::size_t count, std::size_t current,
                                           const std::function<std::string(std::size_t row)>& text_of)
{
    const auto now{std::chrono::steady_clock::now()};
    if(now - m_last > m_pause)
    {
        m_typed.clear();
    }
    m_last = now;
    m_typed += folded(encode_utf8(character));
    if(count == 0)
    {
        return std::nullopt;
    }
    const auto letter{folded(encode_utf8(character))};
    // The same character again and again steps through the rows starting with it; a longer prefix keeps the
    // current row while it still matches.
    const auto repeated{m_typed.size() > letter.size() &&
                        std::all_of(m_typed.begin(), m_typed.end(), [&letter](char c) { return c == letter[0]; }) &&
                        letter.size() == 1};
    const auto& prefix{repeated ? letter : m_typed};
    const auto first{m_typed.size() == letter.size() || repeated ? current + 1 : current};
    for(std::size_t step{}; step < count; ++ step)
    {
        const auto row{(first + step) % count};
        if(folded(text_of(row)).compare(0, prefix.size(), prefix) == 0)
        {
            return row;
        }
    }
    return std::nullopt;
}

} // namespace olagarro::oui
