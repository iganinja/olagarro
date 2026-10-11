#pragma once

#include <oui/application.h>
#include <oui/command.h>

#include <cstddef>
#include <string>
#include <vector>

// What the help is opened on: F1 opens it at the part about what is in the tab, as a calculator or an interactive
// script; on a document, at its start.
enum class HelpTopic
{
    general,
    searching,
    comparing,
    interactive_scripts,
    interactive_ui,
    tasks,
    terminal,
    calculator,
};

// The part of the help, from 0 as show_help counts them, about the topic.
[[nodiscard]] std::size_t help_section(HelpTopic topic);
// The titles of the parts of the help, as show_help makes a tab of each.
[[nodiscard]] std::vector<std::string> help_section_titles(const std::string& markdown);

// The help in the interface's language, ending with every command that has keys, read from the registry so
// that it never falls behind.
[[nodiscard]] std::string help_markdown(const std::string& language, const olagarro::oui::CommandRegistry& commands);
// The help in a window of tabs, one for each part: Left and Right go from one to another.
// It opens at the part given.
void show_help(olagarro::oui::Application& application, const std::string& markdown, std::size_t section = 0);
