#include "languages.h"

#include <oui/file_system.h>
#include <oui/script/script_highlighter.h>

#include <algorithm>
#include <cctype>

using namespace olagarro;

namespace
{

using oui::script::ScriptError;
using oui::script::Value;

[[nodiscard]] std::string lowercase(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return text;
}

[[nodiscard]] std::vector<std::string> strings_of(const Value& value, const std::string& what)
{
    std::vector<std::string> result;
    if(value.is_undefined())
    {
        return result;
    }
    if(!value.is_array())
    {
        throw ScriptError{what + " must be a list of strings"};
    }
    for(std::size_t index{}; index < value.length(); ++ index)
    {
        result.push_back(value.at(index).to_string());
    }
    return result;
}

} // namespace

void Languages::add(const Value& definition)
{
    if(!definition.is_object())
    {
        throw ScriptError{"a language is an object such as {id: \"cpp\", extensions: [\".cpp\"], states: {...}}"};
    }
    auto language{std::make_shared<Language>()};
    language->id = definition.get("id").to_string();
    if(definition.get("id").is_undefined() || language->id.empty())
    {
        throw ScriptError{"a language needs an id"};
    }
    const auto where{"language " + language->id};
    language->name = definition.get("name").is_undefined() ? language->id : definition.get("name").to_string();
    for(auto& extension : strings_of(definition.get("extensions"), where + ": extensions"))
    {
        if(!extension.empty())
        {
            language->extensions.push_back(lowercase(extension.front() == '.' ? extension : "." + extension));
        }
    }
    language->file_names = strings_of(definition.get("files"), where + ": files");
    if(const auto comment{definition.get("comment")}; !comment.is_undefined())
    {
        language->comment = comment.to_string();
    }
    for(const auto& pair : strings_of(definition.get("brackets"), where + ": brackets"))
    {
        if(pair.size() != 2)
        {
            throw ScriptError{where + ": a bracket pair is two characters, such as \"()\", not \"" + pair + "\""};
        }
        language->brackets.emplace_back(pair[0], pair[1]);
    }
    if(language->brackets.empty())
    {
        language->brackets = {{'(', ')'}, {'[', ']'}, {'{', '}'}};
    }
    // Words, as keywords, or {label, detail, text} for a piece of text, as a loop.
    if(const auto suggestions{definition.get("suggestions")}; !suggestions.is_undefined())
    {
        if(!suggestions.is_array())
        {
            throw ScriptError{where + ": suggestions is a list of words, or of {label, detail, text}"};
        }
        for(std::size_t index{}; index < suggestions.length(); ++ index)
        {
            const auto item{suggestions.at(index)};
            if(item.is_string())
            {
                language->suggestions.push_back({item.to_string(), {}, {}});
                continue;
            }
            if(!item.is_object() || !item.get("label").is_string())
            {
                throw ScriptError{where + ": a suggestion is a word, or {label, detail, text}"};
            }
            Suggestion suggestion{item.get("label").to_string(), {}, {}};
            if(item.get("detail").is_string())
            {
                suggestion.detail = item.get("detail").to_string();
            }
            if(item.get("text").is_string())
            {
                suggestion.text = item.get("text").to_string();
            }
            language->suggestions.push_back(std::move(suggestion));
        }
    }
    if(const auto states{definition.get("states")}; !states.is_undefined())
    {
        language->highlighter = oui::script::highlighter_of(states, where);
    }
    const auto same{std::find_if(m_languages.begin(), m_languages.end(),
                                 [&language](const auto& other) { return other->id == language->id; })};
    if(same != m_languages.end())
    {
        *same = std::move(language);
    }
    else
    {
        m_languages.push_back(std::move(language));
    }
}

std::shared_ptr<const Language> Languages::for_path(const std::filesystem::path& file) const
{
    const auto name{oui::utf8_of(file.filename())};
    // The last defined wins, so a user's definition overrides a built-in one.
    for(auto language{m_languages.rbegin()}; language != m_languages.rend(); ++ language)
    {
        const auto& names{(*language)->file_names};
        if(std::find(names.begin(), names.end(), name) != names.end())
        {
            return *language;
        }
    }
    const auto extension{lowercase(oui::utf8_of(file.extension()))};
    if(extension.empty())
    {
        return nullptr;
    }
    for(auto language{m_languages.rbegin()}; language != m_languages.rend(); ++ language)
    {
        const auto& extensions{(*language)->extensions};
        if(std::find(extensions.begin(), extensions.end(), extension) != extensions.end())
        {
            return *language;
        }
    }
    return nullptr;
}

std::shared_ptr<const Language> Languages::with_id(std::string_view id) const
{
    const auto found{std::find_if(m_languages.begin(), m_languages.end(),
                                  [id](const auto& language) { return language->id == id; })};
    return found == m_languages.end() ? nullptr : *found;
}

const std::vector<std::shared_ptr<const Language>>& Languages::all() const
{
    return m_languages;
}
