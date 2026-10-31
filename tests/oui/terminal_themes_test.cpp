#include <oui/terminal_themes.h>
#include <oui/turbo_c_theme.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace olagarro;

namespace
{

using oui::Color;
using oui::Theme;

// How far apart two colours are to the eye, as WCAG counts it: 1 for the same, 21 for black and white.
[[nodiscard]] double contrast(Color first, Color second)
{
    const auto luminance{
        [](Color colour)
        {
            const auto channel{[](std::uint8_t value)
                               {
                                   const auto c{value / 255.0};
                                   return c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
                               }};
            return 0.2126 * channel(colour.red) + 0.7152 * channel(colour.green) + 0.0722 * channel(colour.blue);
        }};
    const auto a{luminance(first)};
    const auto b{luminance(second)};
    return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
}

// What OUI paints on what: text, and its background.
[[nodiscard]] std::vector<std::pair<std::string, std::pair<Color, Color>>> painted(const Theme& t)
{
    return {
        {"editor", {t.editor_foreground, t.editor_background}},
        {"line numbers", {t.line_number_foreground, t.editor_background}},
        {"selection", {t.selection_foreground, t.selection_background}},
        {"search match", {t.search_match_foreground, t.search_match_background}},
        {"occurrence", {t.occurrence_foreground, t.occurrence_background}},
        {"bracket", {t.bracket_match_foreground, t.bracket_match_background}},
        {"removed", {t.diff_removed_foreground, t.diff_removed_background}},
        {"added", {t.diff_added_foreground, t.diff_added_background}},
        {"changed", {t.diff_changed_foreground, t.diff_changed_background}},
        {"keyword", {t.syntax_keyword, t.editor_background}},
        {"type", {t.syntax_type, t.editor_background}},
        {"string", {t.syntax_string, t.editor_background}},
        {"number", {t.syntax_number, t.editor_background}},
        {"comment", {t.syntax_comment, t.editor_background}},
        {"preprocessor", {t.syntax_preprocessor, t.editor_background}},
        {"menu", {t.menu_foreground, t.menu_background}},
        {"menu shortcut", {t.muted_text, t.menu_background}},
        {"menu disabled", {t.disabled_foreground, t.menu_background}},
        {"menu hotkey", {t.hotkey_foreground, t.menu_background}},
        {"menu selected", {t.menu_selected_foreground, t.menu_selected_background}},
        {"status", {t.status_foreground, t.status_background}},
        {"dialog", {t.dialog_foreground, t.dialog_background}},
        {"dialog heading", {t.dialog_heading_foreground, t.dialog_background}},
        {"button", {t.button_foreground, t.button_background}},
        {"button focused", {t.button_focused_foreground, t.button_focused_background}},
        {"default button", {t.button_default_foreground, t.button_background}},
        {"button shortcut", {t.button_shortcut_foreground, t.button_background}},
        {"label shortcut", {t.label_shortcut_foreground, t.dialog_background}},
        {"input", {t.input_foreground, t.input_background}},
        {"tab", {t.tab_active_foreground, t.tab_active_background}},
        {"other tab", {t.tab_inactive_foreground, t.tab_inactive_background}},
        {"table", {t.table_foreground, t.table_background}},
        {"table header", {t.table_header_foreground, t.table_header_background}},
        {"side panel", {t.panel_foreground, t.side_panel_background}},
        {"reading", {t.reading_foreground, t.reading_background}},
        {"focus", {t.focus_text, t.focus_background}},
        {"list", {t.list_foreground, t.list_background}},
        {"chosen item", {t.item_selected_foreground, t.item_selected_background}},
        {"check box", {t.cluster_foreground, t.cluster_background}},
        {"focused check box", {t.cluster_focused_foreground, t.cluster_background}},
        {"check box shortcut", {t.cluster_shortcut_foreground, t.cluster_background}},
        {"under a shadow", {t.shadow_foreground, t.shadow}},
        {"scroll bar", {t.scrollbar_foreground, t.scrollbar_background}},
    };
}

} // namespace

TEST(TerminalThemesTests, TurboComesFirstAndEachHasANameOfItsOwn)
{
    const auto themes{oui::terminal_themes()};
    ASSERT_GE(themes.size(), 4u);
    EXPECT_EQ(themes.front().id, "turbo");
    EXPECT_EQ(themes.front().theme.editor_background, oui::TurboCTheme::make().editor_background);
    std::set<std::string> ids;
    for(const auto& theme : themes)
    {
        EXPECT_TRUE(ids.insert(theme.id).second) << theme.id;
        EXPECT_FALSE(theme.name.empty());
    }
}

TEST(TerminalThemesTests, EveryTextReadsOnWhatItIsPaintedOn)
{
    // Dark grey on black, as line numbers, is about the least that still reads.
    constexpr double least{2.5};
    for(const auto& [id, name, theme] : oui::terminal_themes())
    {
        for(const auto& [what, colours] : painted(theme))
        {
            // A shortcut letter is one underlined letter of a word that reads: Turbo Vision's yellow on grey is less
            // than the rest, and is kept as the original has it.
            const auto needed{std::string_view{what} == "label shortcut" ? 2.0 : least};
            EXPECT_GE(contrast(colours.first, colours.second), needed) << id << ": " << what;
        }
    }
}

TEST(TerminalThemesTests, SomeAreSoftOnTheEyesWithTheirTextClearOnTheEditor)
{
    // Beyond the sixteen colours of a console: a dark slate, a page of paper, a midnight blue, and the most contrast
    // there is, each with the editor's text at least at WCAG's AAA, 7.
    const auto themes{oui::terminal_themes()};
    for(const std::string_view wanted : {"slate", "paper", "midnight", "contrast"})
    {
        const auto found{
            std::find_if(themes.begin(), themes.end(), [wanted](const auto& theme) { return theme.id == wanted; })};
        ASSERT_NE(found, themes.end()) << wanted;
        EXPECT_GE(contrast(found->theme.editor_foreground, found->theme.editor_background), 7.0) << wanted;
        for(const auto colour : {found->theme.syntax_keyword, found->theme.syntax_type, found->theme.syntax_string,
                                 found->theme.syntax_number, found->theme.syntax_function})
        {
            EXPECT_GE(contrast(colour, found->theme.editor_background), 4.5) << wanted;
        }
    }
    const auto contrasted{
        std::find_if(themes.begin(), themes.end(), [](const auto& theme) { return theme.id == "contrast"; })};
    EXPECT_GE(contrast(contrasted->theme.editor_foreground, contrasted->theme.editor_background), 18.0);
}
