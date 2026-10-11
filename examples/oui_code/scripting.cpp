#include "scripting.h"

#include <oui/file_system.h>

#include <algorithm>
#include <array>
#include <cctype>

using namespace olagarro;

namespace
{

using oui::script::ScriptError;
using oui::script::Value;

} // namespace

// What the scripts that give OUI Code its languages and text commands see, for tools/extract_api.js to put in
// oui/oui.d.ts:
//
// api(oui_code): oui.languages.register(language: {id: string, name?: string, extensions?: string[], files?: string[], comment?: string, brackets?: string[], states: {[state: string]: object[]}, suggestions?: (string | {label: string, detail?: string, text?: string})[]}): void
// Adds a language, or replaces one with the same id: how files of its extensions and names are coloured, from the
// rules of each state, how a line is commented, which brackets pair, and what it suggests as one writes: words,
// as its keywords, or pieces of text with a name, where "$0" marks where the cursor goes.
// api(oui_code): oui.commands.register(command: {id: string, title: string | {[language: string]: string}, keys?: string, menuPath?: string | {[language: string]: string}, run: (text: string) => string | undefined}): void
// Adds a text command, in the palette and Alt+X, on keys when given: run gets the selection, or the whole document
// when nothing is selected, and gives back what goes in its place, or nothing to leave it. It shows in Tools > Text
// Commands, or where menuPath puts it, as a path of files: "edit" in the Edit menu, "edit/Case" in a submenu Case of
// it, made when there is none. The first name is the menu's, file, edit, view, tools or help, whatever language
// OUI Code is in; the rest are submenus as they show, OUI Code's own found by their name in English or as translated.
// Like the title, it can be one per language: {en: "edit/Case", es: "edit/Mayúsculas"}.
Scripting::Scripting(std::string language) : m_language{std::move(language)}
{
    m_engine.set_print([this](std::string_view text) { m_output.emplace_back(text); });
    m_engine.define("oui.languages.register",
                    [this](const std::vector<Value>& arguments)
                    {
                        m_languages.add(arguments.empty() ? Value{} : arguments.front());
                        return Value{};
                    });
    m_engine.define("oui.commands.register",
                    [this](const std::vector<Value>& arguments)
                    {
                        add_command(arguments.empty() ? Value{} : arguments.front());
                        return Value{};
                    });
}

// A command as scripts write it: {id, title: "Title" or {en: ..., es: ...}, keys: "Ctrl+Alt+U", run(text)}.
void Scripting::add_command(const Value& definition)
{
    if(!definition.is_object())
    {
        throw ScriptError{"a command is an object such as {id: \"user.shout\", title: \"Shout\", run: text => ...}"};
    }
    TextCommand command;
    command.id = definition.get("id").to_string();
    if(definition.get("id").is_undefined() || command.id.empty())
    {
        throw ScriptError{"a command needs an id"};
    }
    const auto title{definition.get("title")};
    if(title.is_object())
    {
        const auto translated{title.get(m_language)};
        command.title = (translated.is_undefined() ? title.get("en") : translated).to_string();
    }
    else
    {
        command.title = title.is_undefined() ? command.id : title.to_string();
    }
    if(const auto keys{definition.get("keys")}; !keys.is_undefined())
    {
        command.keys = oui::parse_shortcut(keys.to_string());
        if(!command.keys)
        {
            throw ScriptError{"command " + command.id + ": \"" + keys.to_string() +
                              "\" is not a shortcut, such as \"Ctrl+Alt+U\""};
        }
    }
    if(const auto path{definition.get("menuPath")}; !path.is_undefined())
    {
        const auto translated{path.is_object() ? path.get(m_language) : path};
        const auto written{(translated.is_undefined() && path.is_object() ? path.get("en") : translated).to_string()};
        for(std::size_t start{};;)
        {
            const auto end{std::min(written.find('/', start), written.size())};
            command.menu_path.push_back(written.substr(start, end - start));
            if(end == written.size())
            {
                break;
            }
            start = end + 1;
        }
        const auto empty{std::find(command.menu_path.begin(), command.menu_path.end(), std::string{})};
        if(empty != command.menu_path.end())
        {
            throw ScriptError{"command " + command.id + ": menuPath \"" + written +
                              "\" has a name missing, such as \"edit/Case\""};
        }
        auto& menu{command.menu_path.front()};
        std::transform(menu.begin(), menu.end(), menu.begin(),
                       [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        static constexpr std::array<std::string_view, 5> menus{"file", "edit", "view", "tools", "help"};
        if(std::find(menus.begin(), menus.end(), menu) == menus.end())
        {
            throw ScriptError{"command " + command.id + ": menuPath \"" + written +
                              "\" starts with no menu of OUI Code's: file, edit, view, tools or help"};
        }
    }
    command.run = definition.get("run");
    if(!command.run.is_function())
    {
        throw ScriptError{"command " + command.id + " needs run, a function"};
    }
    const auto same{std::find_if(m_commands.begin(), m_commands.end(),
                                 [&command](const TextCommand& other) { return other.id == command.id; })};
    if(same != m_commands.end())
    {
        *same = std::move(command);
    }
    else
    {
        m_commands.push_back(std::move(command));
    }
}

std::vector<std::string> Scripting::load(const std::vector<Script>& built_in,
                                         const std::optional<std::filesystem::path>& folder)
{
    std::vector<std::string> problems;
    const auto run{[this, &problems](std::string_view source, const std::string& name)
                   {
                       try
                       {
                           m_engine.run(source, name);
                       }
                       catch(const ScriptError& error)
                       {
                           problems.push_back(name + ": " + error.what());
                       }
                   }};
    for(const auto& script : built_in)
    {
        run(script.source, script.name);
    }
    std::error_code error;
    if(!folder || !std::filesystem::is_directory(*folder, error))
    {
        return problems;
    }
    std::vector<std::filesystem::path> files;
    for(const auto& entry : oui::list_directory(*folder, error))
    {
        if(!entry.is_directory && oui::utf8_of(entry.path.extension()) == ".js")
        {
            files.push_back(entry.path);
        }
    }
    std::sort(files.begin(), files.end());
    for(const auto& file : files)
    {
        run(oui::read_file(file).value_or(std::string{}), oui::utf8_of(file.filename()));
    }
    return problems;
}

std::optional<std::string> Scripting::run(const TextCommand& command, std::string_view text)
{
    const auto result{command.run.call({m_engine.string(text)})};
    if(result.is_undefined() || result.is_null())
    {
        return std::nullopt;
    }
    return result.to_string();
}

const Languages& Scripting::languages() const
{
    return m_languages;
}

const std::vector<TextCommand>& Scripting::commands() const
{
    return m_commands;
}

const std::vector<std::string>& Scripting::output() const
{
    return m_output;
}
