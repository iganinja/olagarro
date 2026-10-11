#include "help.h"

#include "resources.h"
#include "ui_descriptions.h"

#include <oui/oui.h>

#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

using namespace olagarro;

namespace
{

// Markdown cut at its second-level headings ("## "), outside code blocks: each section with its title, the
// text before the first heading under `first_title`.
[[nodiscard]] std::vector<std::pair<std::string, std::string>> markdown_sections(std::string_view markdown,
                                                                                 const std::string& first_title)
{
    std::vector<std::pair<std::string, std::string>> sections{{first_title, {}}};
    auto in_code{false};
    for(std::size_t start{}; start < markdown.size();)
    {
        const auto end{std::min(markdown.find('\n', start), markdown.size())};
        const auto line{markdown.substr(start, end - start)};
        if(line.substr(0, 3) == "```")
        {
            in_code = !in_code;
        }
        if(!in_code && line.substr(0, 3) == "## ")
        {
            sections.emplace_back(std::string{line.substr(3)}, std::string{});
        }
        sections.back().second.append(line).append("\n");
        start = end + 1;
    }
    return sections;
}

} // namespace

std::vector<std::string> help_section_titles(const std::string& markdown)
{
    std::vector<std::string> titles;
    for(auto& [title, text] : markdown_sections(markdown, "OUI Code"))
    {
        titles.push_back(std::move(title));
    }
    return titles;
}

std::size_t help_section(HelpTopic topic)
{
    // Found by their titles in the English help, whose parts the Spanish one has in the same order; the calculator's
    // comes after them.
    const auto english{help_section_titles(std::string{embedded_oui_code_help().at(0)})};
    if(topic == HelpTopic::calculator)
    {
        return english.size();
    }
    const auto* const title{topic == HelpTopic::searching             ? "Searching"
                            : topic == HelpTopic::comparing           ? "Comparing files"
                            : topic == HelpTopic::interactive_scripts ? "Interactive scripts"
                            : topic == HelpTopic::interactive_ui      ? "Interactive user interfaces"
                            : topic == HelpTopic::tasks               ? "Things to do"
                            : topic == HelpTopic::terminal            ? "A shell in a tab"
                                                                      : "OUI Code"};
    const auto found{std::find(english.begin(), english.end(), title)};
    return found == english.end() ? 0 : static_cast<std::size_t>(std::distance(english.begin(), found));
}

std::string help_markdown(const std::string& language, const oui::CommandRegistry& commands)
{
    std::string markdown{oui::text_for_language(embedded_oui_code_help(), embedded_oui_code_help_names(), language)};
    auto calculator{oui::calculator_help()};
    for(std::size_t at{calculator.find("\n## ")}; at != std::string::npos; at = calculator.find("\n## ", at + 4))
    {
        calculator.insert(at + 1, "#");
    }
    if(calculator.rfind("# ", 0) == 0)
    {
        calculator.insert(0, "#");
    }
    // Where it is, under its title.
    const auto title_end{calculator.find('\n')};
    calculator.insert(title_end + 1, "\n" + oui::translate("Help", "**Tools > Calculator** opens it in a tab.") + "\n");
    markdown += "\n" + calculator;
    markdown += "\n## " + oui::translate("Help", "All commands with keys") + "\n\n| " + oui::translate("Help", "Keys") +
                " | " + oui::translate("Help", "Command") + " |\n|---|---|\n";
    auto listed{commands.commands()};
    std::sort(listed.begin(), listed.end(), [](const oui::CommandInfo& left, const oui::CommandInfo& right)
              { return oui::strip_mnemonic(left.title) < oui::strip_mnemonic(right.title); });
    for(const auto& command : listed)
    {
        if(command.shortcut)
        {
            markdown +=
                "| " + oui::format_shortcut(*command.shortcut) + " | " + oui::strip_mnemonic(command.title) + " |\n";
        }
    }
    return markdown;
}

void show_help(oui::Application& application, const std::string& markdown, std::size_t section)
{
    auto ui{build_oui_code_ui(OuiCodeUi::help)};
    auto& tabs{ui.get<oui::TabView>("tabs")};
    std::vector<oui::MarkdownView*> views;
    for(auto& [title, text] : markdown_sections(markdown, "OUI Code"))
    {
        auto& view{tabs.add_tab(title, oui::markdown_view(std::move(text)))};
        // Apart from the documents, as a manual page.
        view.set_background(&oui::Theme::reading_background);
        view.set_foreground(&oui::Theme::reading_foreground);
        views.push_back(&view);
    }
    const auto focus_shown{[&application, &views, &tabs] { application.focus(*views.at(tabs.selected_index())); }};
    const auto selection{tabs.on_selection_changed([&focus_shown](std::size_t) { focus_shown(); })};
    const auto clicked{tabs.on_tab_activated([&focus_shown](std::size_t) { focus_shown(); })};
    tabs.select(std::min(section, tabs.child_count() - 1));
    application.post(focus_shown);
    static_cast<void>(application.show_modal(ui.take_window()));
}
