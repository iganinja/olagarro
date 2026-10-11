#include "session_entry.h"

#include <cstdio>
#include <cstdlib>

namespace
{

[[nodiscard]] std::string encoded(std::string_view text)
{
    std::string result;
    for(const auto character : text)
    {
        if(character == '%' || character == ';' || character == '\t' || character == '\n' || character == '\r')
        {
            char escape[4];
            std::snprintf(escape, sizeof escape, "%%%02X", static_cast<unsigned char>(character));
            result += escape;
        }
        else
        {
            result += character;
        }
    }
    return result;
}

[[nodiscard]] std::string decoded(std::string_view text)
{
    std::string result;
    for(std::size_t index{}; index < text.size(); ++ index)
    {
        if(text[index] == '%' && index + 2 < text.size())
        {
            const std::string digits{text.substr(index + 1, 2)};
            result += static_cast<char>(std::strtol(digits.c_str(), nullptr, 16));
            index += 2;
        }
        else
        {
            result += text[index];
        }
    }
    return result;
}

} // namespace

const std::vector<std::string_view>& session_kinds()
{
    static const std::vector<std::string_view> kinds{"tasks",   "calculator",    "interactive", "preview",
                                                     "compare", "compare-saved", "terminal",    "interactive-ui"};
    return kinds;
}

std::string session_entry(const SessionTab& tab)
{
    std::string entry{tab.kind.empty() ? std::string{} : tab.kind + ":"};
    for(std::size_t index{}; index < tab.parts.size(); ++ index)
    {
        entry += (index > 0 ? "\t" : "") + encoded(tab.parts[index]);
    }
    return entry;
}

SessionTab session_tab(std::string_view entry)
{
    SessionTab tab;
    const auto colon{entry.find(':')};
    for(const auto kind : session_kinds())
    {
        if(colon == kind.size() && entry.substr(0, colon) == kind)
        {
            tab.kind = std::string{kind};
            entry.remove_prefix(colon + 1);
            break;
        }
    }
    for(std::size_t start{};;)
    {
        const auto end{entry.find('\t', start)};
        tab.parts.push_back(
            decoded(entry.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start)));
        if(end == std::string_view::npos)
        {
            break;
        }
        start = end + 1;
    }
    return tab;
}
