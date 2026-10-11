#include "ignore_rules.h"

#include <oui/file_system.h>

#include <algorithm>

using namespace olagarro;

namespace
{

// The set at the start of `pattern`, after its '[': whether `character` is in it, and the length of the set
// up to and including the ']'. Nothing when the '[' starts no set, and then it stands for itself.
struct SetMatch
{
    bool matches{false};
    std::size_t length{};
};

[[nodiscard]] std::optional<SetMatch> match_set(std::string_view pattern, char character)
{
    std::size_t index{};
    const auto negated{index < pattern.size() && (pattern[index] == '!' || pattern[index] == '^')};
    if(negated)
    {
        ++ index;
    }
    auto matches{false};
    auto first{true};
    while(index < pattern.size() && (pattern[index] != ']' || first))
    {
        first = false;
        auto low{pattern[index]};
        if(low == '\\' && index + 1 < pattern.size())
        {
            low = pattern[++ index];
        }
        auto high{low};
        if(index + 2 < pattern.size() && pattern[index + 1] == '-' && pattern[index + 2] != ']')
        {
            high = pattern[index + 2];
            index += 2;
        }
        if(low <= character && character <= high)
        {
            matches = true;
        }
        ++ index;
    }
    if(index >= pattern.size())
    {
        return std::nullopt;
    }
    return SetMatch{matches != negated, index + 1};
}

[[nodiscard]] std::string read_text(const std::filesystem::path& path)
{
    return oui::read_file(path).value_or(std::string{});
}

// gitignore_match, knowing whether the pattern starts a path part: "**" means folders only as a whole part.
[[nodiscard]] bool match_from(std::string_view pattern, std::string_view text, bool part_start)
{
    while(!pattern.empty())
    {
        if(part_start && pattern.substr(0, 2) == "**" && (pattern.size() == 2 || pattern[2] == '/'))
        {
            // At the end, everything below; otherwise none or more whole folders.
            if(pattern.size() == 2)
            {
                return true;
            }
            const auto rest{pattern.substr(3)};
            for(std::size_t start{}; start <= text.size(); ++ start)
            {
                if((start == 0 || text[start - 1] == '/') && match_from(rest, text.substr(start), true))
                {
                    return true;
                }
            }
            return false;
        }
        const auto token{pattern.front()};
        if(token == '*')
        {
            const auto rest{pattern.substr(1)};
            for(std::size_t length{}; length <= text.size(); ++ length)
            {
                if(match_from(rest, text.substr(length), false))
                {
                    return true;
                }
                if(length < text.size() && text[length] == '/')
                {
                    break;
                }
            }
            return false;
        }
        if(text.empty())
        {
            return false;
        }
        if(token == '?')
        {
            if(text.front() == '/')
            {
                return false;
            }
            pattern.remove_prefix(1);
        }
        else if(token == '[')
        {
            const auto set{match_set(pattern.substr(1), text.front())};
            if(set)
            {
                if(!set->matches || text.front() == '/')
                {
                    return false;
                }
                pattern.remove_prefix(1 + set->length);
            }
            else
            {
                if(text.front() != '[')
                {
                    return false;
                }
                pattern.remove_prefix(1);
            }
        }
        else
        {
            auto literal{token};
            if(token == '\\' && pattern.size() > 1)
            {
                pattern.remove_prefix(1);
                literal = pattern.front();
            }
            if(text.front() != literal)
            {
                return false;
            }
            pattern.remove_prefix(1);
        }
        part_start = text.front() == '/';
        text.remove_prefix(1);
    }
    return text.empty();
}

} // namespace

bool gitignore_match(std::string_view pattern, std::string_view text)
{
    return match_from(pattern, text, true);
}

IgnoreRules IgnoreRules::for_folder(const std::filesystem::path& folder)
{
    IgnoreRules rules;
    std::error_code error;
    const auto absolute{oui::whole_path(folder)};
    for(auto candidate{absolute};; candidate = candidate.parent_path())
    {
        if(std::filesystem::exists(candidate / oui::path_from_utf8(".git"), error))
        {
            rules.m_root = candidate;
            break;
        }
        if(candidate == candidate.parent_path())
        {
            return rules;
        }
    }
    rules.add(read_text(*rules.m_root / oui::path_from_utf8(".git/info/exclude")), "");
    // The .gitignore files on the way down, the top's first, as git reads them.
    std::vector<std::filesystem::path> folders;
    for(auto candidate{absolute}; candidate != rules.m_root->parent_path(); candidate = candidate.parent_path())
    {
        folders.push_back(candidate);
        if(candidate == *rules.m_root)
        {
            break;
        }
    }
    for(auto folder_on_the_way{folders.rbegin()}; folder_on_the_way != folders.rend(); ++ folder_on_the_way)
    {
        rules.add_folder(*folder_on_the_way, rules.relative(*folder_on_the_way));
    }
    return rules;
}

void IgnoreRules::add(std::string_view text, const std::string& base)
{
    // A byte order mark, which some editors write, is no part of the first pattern.
    if(text.substr(0, 3) == "\xEF\xBB\xBF")
    {
        text.remove_prefix(3);
    }
    const auto prefix{base.empty() ? std::string{} : base + "/"};
    for(std::size_t start{}; start < text.size();)
    {
        const auto end{std::min(text.find('\n', start), text.size())};
        std::string line{text.substr(start, end - start)};
        start = end + 1;
        if(!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        // Trailing spaces go, unless a backslash keeps the last one.
        while(!line.empty() && line.back() == ' ' && (line.size() < 2 || line[line.size() - 2] != '\\'))
        {
            line.pop_back();
        }
        if(line.empty() || line.front() == '#')
        {
            continue;
        }
        Rule rule;
        rule.base = prefix;
        if(line.front() == '!')
        {
            rule.negated = true;
            line.erase(0, 1);
        }
        else if(line.front() == '\\' && line.size() > 1 && (line[1] == '#' || line[1] == '!'))
        {
            line.erase(0, 1);
        }
        if(!line.empty() && line.back() == '/')
        {
            rule.folders_only = true;
            line.pop_back();
        }
        rule.anchored = line.find('/') != std::string::npos;
        if(!line.empty() && line.front() == '/')
        {
            line.erase(0, 1);
        }
        if(line.empty())
        {
            continue;
        }
        rule.pattern = std::move(line);
        m_rules.push_back(std::move(rule));
    }
}

void IgnoreRules::add_folder(const std::filesystem::path& folder, const std::string& inside)
{
    if(!m_root)
    {
        return;
    }
    std::error_code error;
    const auto file{folder / oui::path_from_utf8(".gitignore")};
    if(std::filesystem::is_regular_file(file, error))
    {
        add(read_text(file), inside);
    }
}

bool IgnoreRules::in_repository() const
{
    return m_root.has_value();
}

bool IgnoreRules::is_ignored(std::string_view path, bool is_folder) const
{
    const auto slash{path.rfind('/')};
    const auto name{slash == std::string_view::npos ? path : path.substr(slash + 1)};
    // The last rule that matches decides, as in git.
    for(auto rule{m_rules.rbegin()}; rule != m_rules.rend(); ++ rule)
    {
        if((rule->folders_only && !is_folder) || path.substr(0, rule->base.size()) != rule->base)
        {
            continue;
        }
        const auto matches{rule->anchored ? gitignore_match(rule->pattern, path.substr(rule->base.size()))
                                          : gitignore_match(rule->pattern, name)};
        if(matches)
        {
            return !rule->negated;
        }
    }
    return false;
}

std::string IgnoreRules::relative(const std::filesystem::path& path) const
{
    if(!m_root)
    {
        return oui::utf8_of(path);
    }
    const auto inside{oui::utf8_of(oui::relative_to(oui::whole_path(path), *m_root))};
    return inside == "." ? std::string{} : inside;
}
