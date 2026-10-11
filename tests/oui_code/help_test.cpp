#include "help.h"

#include "resources.h"

#include <oui/translation.h>

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace olagarro;

namespace
{

// The title of the section F1 shows for the topic, in the help as it reads in the language.
[[nodiscard]] std::string shown_for(HelpTopic topic, const std::string& language)
{
    oui::install_language(language, {});
    const oui::CommandRegistry commands;
    const auto titles{help_section_titles(help_markdown(language, commands))};
    oui::remove_translations();
    const auto section{help_section(topic)};
    return section < titles.size() ? titles[section] : "(none)";
}

} // namespace

TEST(OuiCodeHelpTests, BothLanguagesHaveTheSamePartsInTheSameOrder)
{
    // F1 finds a part by its place, so the two must go together.
    const auto help{embedded_oui_code_help()};
    EXPECT_EQ(help_section_titles(std::string{help.at(0)}).size(), help_section_titles(std::string{help.at(1)}).size());
}

TEST(OuiCodeHelpTests, F1ShowsThePartOnWhatIsInTheTab)
{
    EXPECT_EQ(shown_for(HelpTopic::general, "en"), "OUI Code");
    EXPECT_EQ(shown_for(HelpTopic::searching, "en"), "Searching");
    EXPECT_EQ(shown_for(HelpTopic::comparing, "en"), "Comparing files");
    EXPECT_EQ(shown_for(HelpTopic::interactive_scripts, "en"), "Interactive scripts");
    EXPECT_EQ(shown_for(HelpTopic::tasks, "en"), "Things to do");
    EXPECT_EQ(shown_for(HelpTopic::terminal, "en"), "A shell in a tab");
    EXPECT_EQ(shown_for(HelpTopic::calculator, "en"), "Calculator");
    EXPECT_EQ(shown_for(HelpTopic::interactive_scripts, "es"), "Scripts interactivos");
    EXPECT_EQ(shown_for(HelpTopic::calculator, "es"), "Calculadora");
    EXPECT_EQ(shown_for(HelpTopic::interactive_ui, "en"), "Interactive user interfaces");
    EXPECT_EQ(shown_for(HelpTopic::interactive_ui, "es"), "Interfaces interactivas");
}
