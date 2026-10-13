#include <oui/completion.h>

#include <algorithm>
#include <cctype>
#include <set>
#include <utility>

namespace olagarro::oui
{

namespace
{

// What a name is made of in the languages suggested for: letters, digits, _ and $, and whatever is not ASCII.
[[nodiscard]] bool in_word(char character)
{
    const auto byte{static_cast<unsigned char>(character)};
    return byte >= 0x80 || std::isalnum(byte) != 0 || character == '_' || character == '$';
}

[[nodiscard]] std::string lowercase(std::string_view text)
{
    std::string lower{text};
    std::transform(lower.begin(), lower.end(), lower.begin(), [](char character)
                   { return static_cast<char>(std::tolower(static_cast<unsigned char>(character))); });
    return lower;
}

[[nodiscard]] std::string trim(std::string_view text)
{
    const auto first{text.find_first_not_of(" \t")};
    if(first == std::string_view::npos)
    {
        return {};
    }
    return std::string{text.substr(first, text.find_last_not_of(" \t") - first + 1)};
}

// Whether `word` starts as `prefix` does, whatever the case of ASCII letters, without making copies of either.
[[nodiscard]] bool starts_loosely(std::string_view word, std::string_view prefix)
{
    if(word.size() < prefix.size())
    {
        return false;
    }
    for(std::size_t index{}; index < prefix.size(); ++ index)
    {
        if(std::tolower(static_cast<unsigned char>(word[index])) !=
           std::tolower(static_cast<unsigned char>(prefix[index])))
        {
            return false;
        }
    }
    return true;
}

// A detail as the list shows it: one line, cut when long.
[[nodiscard]] std::string shortened(std::string text)
{
    constexpr std::size_t longest{60};
    if(text.size() > longest)
    {
        text.resize(longest - 3);
        text += "...";
    }
    return text;
}

// A signature from a name and what follows it in a declaration, "(path: string): string | null", with where each
// parameter is: between the commas of its brackets, past those of the brackets inside, as in a type `{a: x, b: y}`.
[[nodiscard]] Signature signature_of(std::string_view name, std::string_view rest, std::vector<std::string> doc)
{
    Signature signature{std::string{name} + std::string{rest}, {}, std::move(doc)};
    const auto& text{signature.text};
    const auto open{name.size()};
    if(open >= text.size() || text[open] != '(')
    {
        return signature;
    }
    auto depth{0};
    auto from{open + 1};
    const auto add{[&](std::size_t to)
                   {
                       while(from < to && text[from] == ' ')
                       {
                           ++ from;
                       }
                       if(from < to)
                       {
                           signature.parameters.emplace_back(from, to);
                       }
                   }};
    for(auto index{open + 1}; index < text.size(); ++ index)
    {
        const auto character{text[index]};
        if(character == '(' || character == '[' || character == '{' || character == '<')
        {
            ++ depth;
        }
        else if(character == '>' && text[index - 1] == '=')
        {
            // The arrow of a function's type, which closes nothing.
        }
        else if(character == ')' && depth == 0)
        {
            add(index);
            break;
        }
        else if(character == ')' || character == ']' || character == '}' || character == '>')
        {
            -- depth;
        }
        else if(character == ',' && depth == 0)
        {
            add(index);
            from = index + 1;
        }
    }
    return signature;
}

} // namespace

std::optional<std::size_t> Signature::parameter_of(std::size_t argument) const
{
    if(argument < parameters.size())
    {
        return argument;
    }
    if(!parameters.empty() && text.compare(parameters.back().first, 3, "...") == 0)
    {
        return parameters.size() - 1;
    }
    return std::nullopt;
}

std::optional<Call> call_at(std::string_view text, std::size_t cursor)
{
    cursor = std::min(cursor, text.size());
    // From a line well before the cursor, so a long document costs no more than its last stretch.
    constexpr std::size_t looked_back{16384};
    std::size_t index{};
    if(cursor > looked_back)
    {
        index = text.find('\n', cursor - looked_back);
        index = index == std::string_view::npos ? cursor : index + 1;
    }
    // The brackets open at the cursor, each with how many commas of its own it has.
    struct Open
    {
        std::size_t at;
        char bracket;
        std::size_t commas;
    };
    std::vector<Open> open;
    while(index < cursor)
    {
        const auto character{text[index]};
        if(character == '"' || character == '\'' || character == '`')
        {
            // A string to its end, past what is escaped: a bracket in it is no call's.
            for(++ index; index < cursor && text[index] != character && (character == '`' || text[index] != '\n');
                ++ index)
            {
                if(text[index] == '\\')
                {
                    ++ index;
                }
            }
            ++ index;
            continue;
        }
        if(character == '/' && index + 1 < cursor && text[index + 1] == '/')
        {
            index = std::min(text.find('\n', index), cursor);
            continue;
        }
        if(character == '/' && index + 1 < cursor && text[index + 1] == '*')
        {
            const auto end{text.find("*/", index + 2)};
            index = end == std::string_view::npos ? cursor : end + 2;
            continue;
        }
        if(character == '(' || character == '[' || character == '{')
        {
            open.push_back({index, character, 0});
        }
        else if((character == ')' || character == ']' || character == '}') && !open.empty())
        {
            open.pop_back();
        }
        else if(character == ',' && !open.empty())
        {
            ++ open.back().commas;
        }
        ++ index;
    }
    // The innermost "(", though the cursor be in a list or an object inside it, as in oui.column([a, b]).
    const auto call{
        std::find_if(open.rbegin(), open.rend(), [](const Open& bracket) { return bracket.bracket == '('; })};
    if(call == open.rend())
    {
        return std::nullopt;
    }
    const auto writing{writing_at(text, call->at)};
    if(writing.prefix.empty())
    {
        return std::nullopt;
    }
    Call found{call->at, writing.prefix, writing.qualifier, false, call->commas};
    // `new` before the whole name: what is called is a class's constructor.
    auto before{writing.start - (writing.qualifier.empty() ? 0 : writing.qualifier.size() + 1)};
    while(before > 0 && text[before - 1] == ' ')
    {
        -- before;
    }
    found.made = before >= 3 && text.substr(before - 3, 3) == "new" && (before == 3 || !in_word(text[before - 4]));
    return found;
}

std::string class_made_as(std::string_view name, const std::vector<std::string_view>& documents)
{
    const auto made{std::string{name} + " = new "};
    for(const auto document : documents)
    {
        for(auto at{document.find(made)}; at != std::string_view::npos; at = document.find(made, at + 1))
        {
            if(at > 0 && in_word(document[at - 1]))
            {
                continue;
            }
            auto end{at + made.size()};
            while(end < document.size() && (in_word(document[end]) || document[end] == '.'))
            {
                ++ end;
            }
            return std::string{document.substr(at + made.size(), end - at - made.size())};
        }
    }
    return {};
}

Writing writing_at(std::string_view text, std::size_t cursor)
{
    cursor = std::min(cursor, text.size());
    auto start{cursor};
    while(start > 0 && in_word(text[start - 1]))
    {
        -- start;
    }
    Writing writing{start, std::string{text.substr(start, cursor - start)}, {}};
    // The names and dots before it, as "oui.languages" before "reg".
    const auto end{start};
    auto first{start};
    while(first > 0 && text[first - 1] == '.')
    {
        auto name{first - 1};
        while(name > 0 && in_word(text[name - 1]))
        {
            -- name;
        }
        if(name == first - 1)
        {
            break;
        }
        first = name;
    }
    if(first < end)
    {
        writing.qualifier = std::string{text.substr(first, end - 1 - first)};
    }
    return writing;
}

ScriptApi::ScriptApi(std::string_view declarations)
{
    // Where each line is: the names of the namespaces and classes round it, whether the innermost is a class, and
    // what a class does, which its constructor's signature tells.
    struct Scope
    {
        std::string name;
        bool is_class;
        std::vector<std::string> doc;
    };
    std::vector<Scope> scopes;
    const auto here{[&scopes]
                    {
                        std::string whole;
                        for(const auto& scope : scopes)
                        {
                            whole += (whole.empty() ? "" : ".") + scope.name;
                        }
                        return whole;
                    }};
    // The comment over the declaration to come, but for where it says which scripts have it.
    std::vector<std::string> doc;
    auto in_doc{false};
    for(std::size_t start{}; start < declarations.size();)
    {
        const auto end{std::min(declarations.find('\n', start), declarations.size())};
        auto line{trim(declarations.substr(start, end - start))};
        start = end + 1;
        if(line == "/**")
        {
            in_doc = true;
            doc.clear();
            continue;
        }
        if(in_doc)
        {
            if(line == "*/")
            {
                in_doc = false;
            }
            else if(line.rfind("* ", 0) == 0 && line.rfind("* In: ", 0) != 0)
            {
                doc.push_back(line.substr(2));
            }
            continue;
        }
        if(line.empty())
        {
            continue;
        }
        if(line.rfind("declare ", 0) == 0)
        {
            line.erase(0, 8);
        }
        const auto name_of{[&line](std::size_t from)
                           {
                               auto to{from};
                               while(to < line.size() && in_word(line[to]))
                               {
                                   ++ to;
                               }
                               return line.substr(from, to - from);
                           }};
        const auto without_semicolon{[](std::string text)
                                     {
                                         if(!text.empty() && text.back() == ';')
                                         {
                                             text.pop_back();
                                         }
                                         return text;
                                     }};
        const auto whole{[&here](const std::string& name)
                         {
                             const auto outer{here()};
                             return outer.empty() ? name : outer + "." + name;
                         }};
        if(line == "}")
        {
            if(!scopes.empty())
            {
                scopes.pop_back();
            }
        }
        else if(line.rfind("namespace ", 0) == 0 || line.rfind("class ", 0) == 0)
        {
            const auto is_class{line.front() == 'c'};
            const auto name{name_of(is_class ? 6 : 10)};
            const auto outer{here()};
            m_members[outer].push_back({name, is_class ? "class" : "namespace", {}});
            scopes.push_back({name, is_class, doc});
            if(is_class)
            {
                m_classes.insert(here());
            }
            if(const auto extends{line.find(" extends ")}; is_class && extends != std::string::npos)
            {
                // A base in the same namespace is written without it.
                const auto base{trim(line.substr(extends + 9, line.find('{') - extends - 9))};
                m_bases[here()] = base.find('.') != std::string::npos || outer.empty() ? base : outer + "." + base;
            }
        }
        else if(!scopes.empty() && scopes.back().is_class)
        {
            // A class's members are its instances'; its constructor is called as the class, with `new`.
            const auto name{name_of(0)};
            const auto rest{without_semicolon(line.substr(name.size()))};
            // What a class's members are declared with is written from the namespace the class is in.
            const auto class_name{here()};
            const auto dot{class_name.rfind('.')};
            const auto outer{dot == std::string::npos ? std::string{} : class_name.substr(0, dot)};
            if(name == "constructor")
            {
                m_signatures[here()] = signature_of(scopes.back().name, rest, scopes.back().doc);
                m_scopes[here()] = outer;
            }
            else if(!name.empty())
            {
                m_instance_members[here()].push_back({name, shortened(trim(rest)), {}});
                if(!rest.empty() && rest.front() == '(')
                {
                    m_signatures[here() + "#" + name] = signature_of(name, rest, doc);
                    m_scopes[here() + "#" + name] = outer;
                }
                else
                {
                    m_fields[here() + "#" + name] = Signature{name + rest, {}, doc};
                }
            }
        }
        else if(line.rfind("function ", 0) == 0)
        {
            const auto name{name_of(9)};
            const auto signature{without_semicolon(line.substr(9 + name.size()))};
            m_members[here()].push_back({name, shortened(signature), {}});
            m_signatures[whole(name)] = signature_of(name, signature, doc);
            m_scopes[whole(name)] = here();
        }
        else if(line.rfind("const ", 0) == 0)
        {
            const auto name{name_of(6)};
            m_members[here()].push_back({name, shortened(without_semicolon(trim(line.substr(6 + name.size())))), {}});
        }
        doc.clear();
    }
}

std::optional<Signature> ScriptApi::signature(const Call& call, const std::vector<std::string_view>& documents) const
{
    const auto whole{call.qualifier.empty() ? call.name : call.qualifier + "." + call.name};
    if(const auto found{m_signatures.find(whole)}; found != m_signatures.end())
    {
        return found->second;
    }
    if(call.qualifier.empty() || call.qualifier.find('.') != std::string::npos)
    {
        return std::nullopt;
    }
    // What an instance has, in its class or the classes it derives from.
    for(auto current{type_of(call.qualifier, documents)}; !current.empty();)
    {
        if(const auto found{m_signatures.find(current + "#" + call.name)}; found != m_signatures.end())
        {
            return found->second;
        }
        const auto base{m_bases.find(current)};
        current = base == m_bases.end() ? std::string{} : base->second;
    }
    return std::nullopt;
}

const Signature* ScriptApi::declared(std::string_view whole) const
{
    if(const auto found{m_signatures.find(whole)}; found != m_signatures.end())
    {
        return &found->second;
    }
    const auto field{m_fields.find(whole)};
    return field == m_fields.end() ? nullptr : &field->second;
}

std::string ScriptApi::resolved(std::string_view type, std::string_view scope) const
{
    const auto name{trim(type)};
    if(name.empty() ||
       !std::all_of(name.begin(), name.end(), [](char character) { return in_word(character) || character == '.'; }))
    {
        return {};
    }
    // From the namespace it is written in outwards: "Image" in paint is paint.Image before a top-level Image.
    for(std::string outer{scope};;)
    {
        const auto whole{outer.empty() ? name : outer + "." + name};
        if(m_classes.count(whole) != 0)
        {
            return whole;
        }
        if(outer.empty())
        {
            return {};
        }
        const auto dot{outer.rfind('.')};
        outer = dot == std::string::npos ? std::string{} : outer.substr(0, dot);
    }
}

std::string ScriptApi::returned(const std::string& key) const
{
    const auto signature{m_signatures.find(key)};
    if(signature == m_signatures.end())
    {
        return {};
    }
    // The type after the brackets of the parameters: "(radius: number): Image".
    const auto& text{signature->second.text};
    const auto open{text.find('(')};
    if(open == std::string::npos)
    {
        return {};
    }
    auto depth{0};
    for(auto index{open}; index < text.size(); ++ index)
    {
        if(text[index] == '(')
        {
            ++ depth;
        }
        else if(text[index] == ')' && -- depth == 0)
        {
            const auto colon{text.find(':', index)};
            if(colon == std::string::npos)
            {
                return {};
            }
            const auto scope{m_scopes.find(key)};
            return resolved(std::string_view{text}.substr(colon + 1), scope == m_scopes.end() ? "" : scope->second);
        }
    }
    return {};
}

std::string ScriptApi::type_of(std::string_view name, const std::vector<std::string_view>& documents) const
{
    return type_of(name, documents, 0);
}

std::string ScriptApi::type_of(std::string_view name, const std::vector<std::string_view>& documents, int depth) const
{
    // Far enough for `a = b.c(`, `b = d.e(`..., and no further, should two names be made of each other.
    if(name.empty() || depth > 4)
    {
        return {};
    }
    if(auto made{class_made_as(name, documents)}; !made.empty())
    {
        return made;
    }
    const auto skip_blanks{[](std::string_view document, std::size_t at)
                           {
                               while(at < document.size() && (document[at] == ' ' || document[at] == '\t'))
                               {
                                   ++ at;
                               }
                               return at;
                           }};
    for(const auto document : documents)
    {
        for(auto at{document.find(name)}; at != std::string_view::npos; at = document.find(name, at + 1))
        {
            const auto end{at + name.size()};
            if((at > 0 && (in_word(document[at - 1]) || document[at - 1] == '.')) ||
               (end < document.size() && in_word(document[end])))
            {
                continue;
            }
            // `name = a.b.c(`: what c returns.
            auto equals{skip_blanks(document, end)};
            if(equals >= document.size() || document[equals] != '=' ||
               (equals + 1 < document.size() && (document[equals + 1] == '=' || document[equals + 1] == '>')))
            {
                continue;
            }
            const auto from{skip_blanks(document, equals + 1)};
            auto to{from};
            while(to < document.size() && (in_word(document[to]) || document[to] == '.'))
            {
                ++ to;
            }
            if(to == from || skip_blanks(document, to) >= document.size() || document[skip_blanks(document, to)] != '(')
            {
                continue;
            }
            const auto path{document.substr(from, to - from)};
            const auto dot{path.rfind('.')};
            std::string found;
            if(dot == std::string_view::npos)
            {
                found = returned(std::string{path});
            }
            else
            {
                const auto qualifier{path.substr(0, dot)};
                const auto called{std::string{path.substr(dot + 1)}};
                if(members(qualifier) != nullptr)
                {
                    found = returned(std::string{qualifier} + "." + called);
                }
                else
                {
                    // What an instance's method returns, in its class or those it derives from.
                    for(auto current{type_of(qualifier, documents, depth + 1)}; !current.empty() && found.empty();)
                    {
                        found = returned(current + "#" + called);
                        const auto base{m_bases.find(current)};
                        current = base == m_bases.end() ? std::string{} : base->second;
                    }
                }
            }
            if(!found.empty())
            {
                return found;
            }
        }
    }
    // A parameter of a function written with the name of a method a class declares with typed parameters: the
    // callbacks an API's objects have, as `render(image, values) {` or `render: (image, values) =>`.
    for(const auto& [key, signature] : m_signatures)
    {
        const auto hash{key.find('#')};
        if(hash == std::string::npos)
        {
            continue;
        }
        const auto method{std::string_view{key}.substr(hash + 1)};
        for(const auto document : documents)
        {
            for(auto at{document.find(method)}; at != std::string_view::npos; at = document.find(method, at + 1))
            {
                const auto end{at + method.size()};
                if((at > 0 && (in_word(document[at - 1]) || document[at - 1] == '.')) ||
                   (end < document.size() && in_word(document[end])))
                {
                    continue;
                }
                auto open{skip_blanks(document, end)};
                if(open < document.size() && document[open] == ':')
                {
                    open = skip_blanks(document, open + 1);
                    if(document.substr(open, 8) == "function")
                    {
                        open = skip_blanks(document, open + 8);
                    }
                }
                if(open >= document.size() || document[open] != '(')
                {
                    continue;
                }
                const auto close{document.find(')', open)};
                if(close == std::string_view::npos)
                {
                    continue;
                }
                const auto after{skip_blanks(document, close + 1)};
                if(after >= document.size() || (document[after] != '{' && document.substr(after, 2) != "=>"))
                {
                    continue;
                }
                // The parameters written, by place, against those declared.
                const auto written{document.substr(open + 1, close - open - 1)};
                std::size_t place{};
                for(std::size_t start{};; ++ place)
                {
                    const auto comma{written.find(',', start)};
                    auto parameter{trim(written.substr(start, comma == std::string_view::npos ? std::string_view::npos
                                                                                              : comma - start))};
                    parameter = parameter.substr(0, parameter.find('='));
                    if(trim(parameter) == name && place < signature.parameters.size())
                    {
                        const auto [first, last]{signature.parameters[place]};
                        const auto declared{std::string_view{signature.text}.substr(first, last - first)};
                        const auto colon{declared.find(':')};
                        const auto scope{m_scopes.find(key)};
                        if(colon != std::string_view::npos)
                        {
                            if(auto type{
                                   resolved(declared.substr(colon + 1), scope == m_scopes.end() ? "" : scope->second)};
                               !type.empty())
                            {
                                return type;
                            }
                        }
                    }
                    if(comma == std::string_view::npos)
                    {
                        break;
                    }
                    start = comma + 1;
                }
            }
        }
    }
    return {};
}

std::vector<Suggestion> ScriptApi::instance_members(std::string_view name) const
{
    std::vector<Suggestion> found;
    for(std::string current{name}; !current.empty();)
    {
        if(const auto own{m_instance_members.find(current)}; own != m_instance_members.end())
        {
            found.insert(found.end(), own->second.begin(), own->second.end());
        }
        const auto base{m_bases.find(current)};
        current = base == m_bases.end() ? std::string{} : base->second;
    }
    return found;
}

const std::vector<Suggestion>* ScriptApi::members(std::string_view qualifier) const
{
    const auto found{m_members.find(qualifier)};
    return found == m_members.end() ? nullptr : &found->second;
}

std::vector<Suggestion> suggestions_for(const Writing& writing, const std::vector<std::string_view>& documents,
                                        const std::vector<Suggestion>* language, const ScriptApi* api)
{
    const auto lower{lowercase(writing.prefix)};
    // Two rounds: those that start exactly as written, then those that do whatever the case.
    std::vector<Suggestion> exact;
    std::vector<Suggestion> loose;
    std::set<std::string, std::less<>> seen;
    const auto offer{[&](const Suggestion& suggestion)
                     {
                         if(suggestion.label == writing.prefix || seen.count(suggestion.label) != 0 ||
                            lowercase(suggestion.label).rfind(lower, 0) != 0)
                         {
                             return;
                         }
                         seen.insert(suggestion.label);
                         (suggestion.label.rfind(writing.prefix, 0) == 0 ? exact : loose).push_back(suggestion);
                     }};
    const auto* const known{api != nullptr && !writing.qualifier.empty() ? api->members(writing.qualifier) : nullptr};
    // A name a document makes as one of the API's classes: `label = new oui.Label(`.
    std::vector<Suggestion> instance;
    if(known == nullptr && api != nullptr && !writing.qualifier.empty() &&
       writing.qualifier.find('.') == std::string::npos)
    {
        instance = api->instance_members(api->type_of(writing.qualifier, documents));
    }
    if(known != nullptr || !instance.empty())
    {
        for(const auto& member : known != nullptr ? *known : instance)
        {
            offer(member);
        }
    }
    else if(writing.qualifier.empty() || !writing.prefix.empty())
    {
        if(writing.qualifier.empty())
        {
            if(language != nullptr)
            {
                for(const auto& suggestion : *language)
                {
                    offer(suggestion);
                }
            }
            if(const auto* const top{api != nullptr ? api->members("") : nullptr})
            {
                for(const auto& member : *top)
                {
                    offer(member);
                }
            }
        }
        // Words of the documents, but for a word not begun, which would be every word there is.
        if(!writing.prefix.empty())
        {
            // Only those that start as the word does are kept, once each, so a long document costs a read through.
            std::set<std::string_view> words;
            for(const auto document : documents)
            {
                for(std::size_t index{}; index < document.size();)
                {
                    if(!in_word(document[index]))
                    {
                        ++ index;
                        continue;
                    }
                    auto end{index};
                    while(end < document.size() && in_word(document[end]))
                    {
                        ++ end;
                    }
                    const auto word{document.substr(index, end - index)};
                    if(word.size() >= 3 && !std::isdigit(static_cast<unsigned char>(word.front())) &&
                       starts_loosely(word, writing.prefix))
                    {
                        words.insert(word);
                    }
                    index = end;
                }
            }
            std::vector<std::string_view> sorted{words.begin(), words.end()};
            std::sort(sorted.begin(), sorted.end(),
                      [](std::string_view a, std::string_view b) { return lowercase(a) < lowercase(b); });
            for(const auto word : sorted)
            {
                offer({std::string{word}, {}, {}});
            }
        }
    }
    exact.insert(exact.end(), std::make_move_iterator(loose.begin()), std::make_move_iterator(loose.end()));
    constexpr std::size_t most{100};
    if(exact.size() > most)
    {
        exact.resize(most);
    }
    return exact;
}

} // namespace olagarro::oui
