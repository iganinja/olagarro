#pragma once

#include "languages.h"

#include <oui/command.h>
#include <oui/script/script_engine.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// A command a script defined: it gets text, the selection or the whole document, and gives back what goes in
// its place, or undefined to leave it.
struct TextCommand
{
    std::string id;
    std::string title;
    std::optional<olagarro::oui::Shortcut> keys;
    // Where in the menus it goes, from menuPath: one of OUI Code's menus by its name, "edit", then the submenus under
    // it, as written. Empty for Tools > Text Commands.
    std::vector<std::string> menu_path;
    olagarro::oui::script::Value run;
};

// The scripts OUI Code runs: its own, built in, then the user's, every .js file in a folder in name order. They
// define languages, with oui.languages.register, and text commands, with oui.commands.register.
class Scripting
{
  public:
    // A script's own source and the name its errors carry.
    struct Script
    {
        std::string name;
        std::string_view source;
    };

    // `language` picks the titles of commands that give one per language, such as {en: ..., es: ...}.
    explicit Scripting(std::string language);
    Scripting(const Scripting&) = delete;
    Scripting& operator=(const Scripting&) = delete;

    // Runs `built_in`, then the user's scripts in `folder` when it is given. A script that fails stops there,
    // and what it defined before stays; the messages say which and why, one per script.
    [[nodiscard]] std::vector<std::string> load(const std::vector<Script>& built_in,
                                                const std::optional<std::filesystem::path>& folder);
    // Runs a text command on `text`: what goes in its place, or nothing to leave it. Throws ScriptError.
    [[nodiscard]] std::optional<std::string> run(const TextCommand& command, std::string_view text);

    [[nodiscard]] const Languages& languages() const;
    [[nodiscard]] const std::vector<TextCommand>& commands() const;
    // What scripts printed with console.log, a line each.
    [[nodiscard]] const std::vector<std::string>& output() const;

  private:
    void add_command(const olagarro::oui::script::Value& definition);

    std::string m_language;
    olagarro::oui::script::ScriptEngine m_engine;
    Languages m_languages;
    std::vector<TextCommand> m_commands;
    std::vector<std::string> m_output;
};
