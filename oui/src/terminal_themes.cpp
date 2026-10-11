#include <oui/terminal_themes.h>

#include <oui/translation.h>
#include <oui/turbo_c_theme.h>

namespace olagarro::oui
{

namespace
{

// The sixteen colours of a text console, as EGA and VGA showed them.
constexpr Color black{0, 0, 0};
constexpr Color blue{0, 0, 170};
constexpr Color green{0, 170, 0};
constexpr Color cyan{0, 170, 170};
constexpr Color red{170, 0, 0};
constexpr Color magenta{170, 0, 170};
constexpr Color brown{170, 85, 0};
constexpr Color light_gray{170, 170, 170};
constexpr Color dark_gray{85, 85, 85};
constexpr Color light_blue{85, 85, 255};
constexpr Color light_green{85, 255, 85};
constexpr Color light_cyan{85, 255, 255};
constexpr Color light_red{255, 85, 85};
constexpr Color light_magenta{255, 85, 255};
constexpr Color yellow{255, 255, 85};
constexpr Color white{255, 255, 255};

// Grey surfaces on black, the editor black: for a dark terminal.
Theme night()
{
    Theme theme{TurboCTheme::make()};
    theme.button_shortcut_foreground = red;
    theme.label_shortcut_foreground = yellow;
    theme.button_default_foreground = blue;
    theme.shadow_foreground = dark_gray;
    theme.dialog_icon_foreground = light_green;
    theme.list_background = black;
    theme.list_foreground = light_gray;
    theme.item_selected_background = blue;
    theme.item_selected_foreground = white;
    theme.cluster_background = dark_gray;
    theme.cluster_foreground = white;
    theme.cluster_focused_foreground = light_cyan;
    theme.cluster_shortcut_foreground = yellow;
    theme.background = black;
    theme.text = light_gray;
    theme.muted_text = dark_gray;
    theme.control_background = black;
    theme.panel_background = black;
    theme.panel_foreground = light_gray;
    theme.side_panel_background = black;
    theme.shadow = black;
    theme.focus_background = cyan;
    theme.focus_text = black;

    theme.dialog_background = dark_gray;
    theme.dialog_foreground = white;
    theme.dialog_border = light_gray;
    theme.dialog_heading_foreground = light_cyan;
    theme.table_header_background = dark_gray;
    theme.table_header_foreground = white;
    theme.table_background = black;
    theme.table_foreground = light_gray;

    theme.menu_background = black;
    theme.menu_foreground = white;
    theme.menu_border = light_gray;
    theme.menu_selected_background = cyan;
    theme.menu_selected_foreground = black;
    theme.status_background = dark_gray;
    theme.status_foreground = white;

    theme.button_background = light_gray;
    theme.button_foreground = black;
    theme.button_focused_background = cyan;
    theme.button_focused_foreground = black;
    theme.button_pressed_background = blue;
    theme.button_pressed_foreground = white;

    theme.tab_active_background = black;
    theme.tab_active_foreground = white;
    theme.tab_inactive_background = dark_gray;
    theme.tab_inactive_foreground = light_gray;

    theme.selection_background = blue;
    theme.selection_foreground = white;
    theme.search_match_background = brown;
    theme.search_match_foreground = white;
    theme.occurrence_background = dark_gray;
    theme.occurrence_foreground = white;
    theme.bracket_match_background = green;
    theme.bracket_match_foreground = white;
    theme.diff_removed_background = red;
    theme.diff_removed_foreground = white;
    theme.diff_added_background = green;
    theme.diff_added_foreground = white;
    theme.diff_changed_background = dark_gray;
    theme.diff_changed_foreground = white;
    theme.diff_filler_background = blue;
    theme.scrollbar_background = dark_gray;
    theme.scrollbar_foreground = light_gray;

    theme.editor_background = black;
    theme.editor_foreground = light_gray;
    theme.line_number_foreground = dark_gray;
    theme.reading_background = black;
    theme.reading_foreground = light_gray;
    theme.syntax_keyword = light_blue;
    theme.syntax_type = light_cyan;
    theme.syntax_function = white;
    theme.syntax_string = light_green;
    theme.syntax_number = light_magenta;
    theme.syntax_comment = dark_gray;
    theme.syntax_preprocessor = yellow;
    theme.syntax_operator = light_gray;
    theme.heading_foreground = yellow;
    theme.code_foreground = light_cyan;
    theme.code_background = dark_gray;
    theme.link_foreground = light_green;

    theme.input_background = black;
    theme.input_foreground = white;
    theme.disabled_foreground = dark_gray;
    theme.hotkey_foreground = yellow;
    theme.error_foreground = light_red;
    theme.warning_foreground = yellow;
    theme.info_foreground = light_cyan;
    return theme;
}

// Black text on white and grey: for a light terminal, or a bright room.
Theme light()
{
    Theme theme{TurboCTheme::make()};
    theme.button_shortcut_foreground = red;
    theme.label_shortcut_foreground = red;
    theme.button_default_foreground = blue;
    theme.shadow_foreground = light_gray;
    theme.dialog_icon_foreground = green;
    theme.list_background = white;
    theme.list_foreground = black;
    theme.item_selected_background = light_cyan;
    theme.item_selected_foreground = black;
    theme.cluster_background = light_gray;
    theme.cluster_foreground = black;
    theme.cluster_focused_foreground = blue;
    theme.cluster_shortcut_foreground = red;
    theme.background = white;
    theme.text = black;
    theme.muted_text = dark_gray;
    theme.control_background = white;
    theme.panel_background = white;
    theme.panel_foreground = black;
    theme.side_panel_background = light_gray;
    theme.shadow = dark_gray;
    theme.focus_background = blue;
    theme.focus_text = white;

    theme.dialog_background = light_gray;
    theme.dialog_foreground = black;
    theme.dialog_border = white;
    theme.dialog_heading_foreground = blue;
    theme.table_header_background = light_gray;
    theme.table_header_foreground = black;
    theme.table_background = white;
    theme.table_foreground = black;

    theme.menu_background = light_gray;
    theme.menu_foreground = black;
    theme.menu_border = dark_gray;
    theme.menu_selected_background = blue;
    theme.menu_selected_foreground = white;
    theme.status_background = light_gray;
    theme.status_foreground = black;

    theme.button_background = white;
    theme.button_foreground = black;
    theme.button_focused_background = blue;
    theme.button_focused_foreground = white;
    theme.button_pressed_background = dark_gray;
    theme.button_pressed_foreground = white;

    theme.tab_active_background = white;
    theme.tab_active_foreground = black;
    theme.tab_inactive_background = light_gray;
    theme.tab_inactive_foreground = dark_gray;

    theme.selection_background = light_cyan;
    theme.selection_foreground = black;
    theme.search_match_background = yellow;
    theme.search_match_foreground = black;
    theme.occurrence_background = light_gray;
    theme.occurrence_foreground = black;
    theme.bracket_match_background = light_green;
    theme.bracket_match_foreground = black;
    theme.diff_removed_background = light_red;
    theme.diff_removed_foreground = black;
    theme.diff_added_background = light_green;
    theme.diff_added_foreground = black;
    theme.diff_changed_background = light_cyan;
    theme.diff_changed_foreground = black;
    theme.diff_filler_background = light_gray;
    theme.scrollbar_background = light_gray;
    theme.scrollbar_foreground = dark_gray;

    theme.editor_background = white;
    theme.editor_foreground = black;
    theme.line_number_foreground = dark_gray;
    theme.reading_background = white;
    theme.reading_foreground = black;
    theme.syntax_keyword = blue;
    theme.syntax_type = cyan;
    theme.syntax_function = black;
    theme.syntax_string = green;
    theme.syntax_number = magenta;
    theme.syntax_comment = dark_gray;
    theme.syntax_preprocessor = brown;
    theme.syntax_operator = black;
    theme.heading_foreground = blue;
    theme.code_foreground = black;
    theme.code_background = light_gray;
    theme.link_foreground = blue;

    theme.input_background = white;
    theme.input_foreground = black;
    theme.disabled_foreground = dark_gray;
    theme.hotkey_foreground = red;
    theme.error_foreground = red;
    theme.warning_foreground = brown;
    theme.info_foreground = blue;
    return theme;
}

// Greys alone, what is chosen or has the focus shown the other way round: for a terminal of few colours, or eyes
// that tell them apart badly.
Theme monochrome()
{
    Theme theme{TurboCTheme::make()};
    theme.button_shortcut_foreground = white;
    theme.label_shortcut_foreground = black;
    theme.button_default_foreground = light_gray;
    theme.shadow_foreground = light_gray;
    theme.dialog_icon_foreground = white;
    theme.list_background = black;
    theme.list_foreground = light_gray;
    theme.item_selected_background = light_gray;
    theme.item_selected_foreground = black;
    theme.cluster_background = black;
    theme.cluster_foreground = light_gray;
    theme.cluster_focused_foreground = white;
    theme.cluster_shortcut_foreground = white;
    theme.background = black;
    theme.text = light_gray;
    theme.muted_text = dark_gray;
    theme.control_background = black;
    theme.panel_background = black;
    theme.panel_foreground = light_gray;
    theme.side_panel_background = black;
    theme.shadow = dark_gray;
    theme.focus_background = white;
    theme.focus_text = black;

    theme.dialog_background = light_gray;
    theme.dialog_foreground = black;
    theme.dialog_border = white;
    theme.dialog_heading_foreground = black;
    theme.dialog_border_placing = white;
    theme.table_header_background = light_gray;
    theme.table_header_foreground = black;
    theme.table_background = black;
    theme.table_foreground = light_gray;

    theme.menu_background = light_gray;
    theme.menu_foreground = black;
    theme.menu_border = white;
    theme.menu_selected_background = black;
    theme.menu_selected_foreground = white;
    theme.status_background = light_gray;
    theme.status_foreground = black;

    theme.button_background = dark_gray;
    theme.button_foreground = white;
    theme.button_focused_background = white;
    theme.button_focused_foreground = black;
    theme.button_pressed_background = black;
    theme.button_pressed_foreground = white;

    theme.tab_active_background = light_gray;
    theme.tab_active_foreground = black;
    theme.tab_inactive_background = black;
    theme.tab_inactive_foreground = light_gray;

    theme.selection_background = light_gray;
    theme.selection_foreground = black;
    theme.search_match_background = white;
    theme.search_match_foreground = black;
    theme.occurrence_background = dark_gray;
    theme.occurrence_foreground = white;
    theme.bracket_match_background = white;
    theme.bracket_match_foreground = black;
    theme.diff_removed_background = light_gray;
    theme.diff_removed_foreground = black;
    theme.diff_added_background = white;
    theme.diff_added_foreground = black;
    // Few greys to go round: a changed line stands out by its white text, and by what changed within it.
    theme.diff_changed_background = black;
    theme.diff_changed_foreground = white;
    theme.diff_filler_background = dark_gray;
    theme.scrollbar_background = dark_gray;
    theme.scrollbar_foreground = light_gray;

    theme.editor_background = black;
    theme.editor_foreground = light_gray;
    theme.line_number_foreground = dark_gray;
    theme.reading_background = black;
    theme.reading_foreground = light_gray;
    theme.syntax_keyword = white;
    theme.syntax_type = white;
    theme.syntax_function = light_gray;
    theme.syntax_string = light_gray;
    theme.syntax_number = light_gray;
    theme.syntax_comment = dark_gray;
    theme.syntax_preprocessor = white;
    theme.syntax_operator = light_gray;
    theme.heading_foreground = white;
    theme.code_foreground = white;
    theme.code_background = dark_gray;
    theme.link_foreground = white;

    theme.input_background = black;
    theme.input_foreground = white;
    theme.disabled_foreground = dark_gray;
    // Underlined, as every hotkey is, in the colour of the text.
    theme.hotkey_foreground = black;
    theme.error_foreground = white;
    theme.warning_foreground = white;
    theme.info_foreground = white;
    return theme;
}

// Yellow and brown on black, as an amber monitor: for a dark terminal, warmer than Night.
Theme amber()
{
    Theme theme{TurboCTheme::make()};
    theme.background = black;
    theme.text = yellow;
    theme.muted_text = brown;
    theme.control_background = black;
    theme.panel_background = black;
    theme.panel_foreground = brown;
    theme.side_panel_background = black;
    theme.shadow = dark_gray;
    theme.shadow_foreground = black;
    theme.focus_background = brown;
    theme.focus_text = black;

    theme.dialog_background = brown;
    theme.dialog_foreground = black;
    theme.dialog_border = yellow;
    theme.dialog_heading_foreground = yellow;
    theme.dialog_icon_foreground = yellow;
    theme.dialog_border_placing = white;
    theme.table_header_background = brown;
    theme.table_header_foreground = black;
    theme.table_background = black;
    theme.table_foreground = yellow;
    theme.list_background = black;
    theme.list_foreground = brown;
    theme.item_selected_background = yellow;
    theme.item_selected_foreground = black;
    theme.cluster_background = brown;
    theme.cluster_foreground = black;
    theme.cluster_focused_foreground = yellow;
    theme.cluster_shortcut_foreground = white;

    theme.menu_background = black;
    theme.menu_foreground = yellow;
    theme.menu_border = brown;
    theme.menu_selected_background = brown;
    theme.menu_selected_foreground = black;
    theme.status_background = brown;
    theme.status_foreground = black;

    theme.button_background = brown;
    theme.button_foreground = black;
    theme.button_default_foreground = yellow;
    theme.button_shortcut_foreground = white;
    theme.label_shortcut_foreground = white;
    theme.button_focused_background = yellow;
    theme.button_focused_foreground = black;
    theme.button_pressed_background = black;
    theme.button_pressed_foreground = yellow;

    theme.tab_active_background = black;
    theme.tab_active_foreground = yellow;
    theme.tab_inactive_background = brown;
    theme.tab_inactive_foreground = black;

    theme.selection_background = brown;
    theme.selection_foreground = black;
    theme.search_match_background = yellow;
    theme.search_match_foreground = black;
    theme.occurrence_background = dark_gray;
    theme.occurrence_foreground = yellow;
    theme.bracket_match_background = red;
    theme.bracket_match_foreground = yellow;
    theme.diff_removed_background = red;
    theme.diff_removed_foreground = yellow;
    theme.diff_added_background = green;
    theme.diff_added_foreground = black;
    theme.diff_changed_background = brown;
    theme.diff_changed_foreground = black;
    theme.diff_filler_background = dark_gray;
    theme.scrollbar_background = black;
    theme.scrollbar_foreground = brown;

    theme.editor_background = black;
    theme.editor_foreground = yellow;
    theme.line_number_foreground = dark_gray;
    theme.reading_background = black;
    theme.reading_foreground = yellow;
    theme.syntax_keyword = white;
    theme.syntax_type = yellow;
    theme.syntax_function = yellow;
    theme.syntax_string = brown;
    theme.syntax_number = brown;
    theme.syntax_comment = dark_gray;
    theme.syntax_preprocessor = white;
    theme.syntax_operator = yellow;
    theme.heading_foreground = yellow;
    theme.code_foreground = yellow;
    theme.code_background = dark_gray;
    theme.link_foreground = white;

    theme.input_background = black;
    theme.input_foreground = yellow;
    theme.disabled_foreground = dark_gray;
    theme.hotkey_foreground = white;
    theme.error_foreground = light_red;
    theme.warning_foreground = yellow;
    theme.info_foreground = white;
    return theme;
}

// Light green and green on black, as a green phosphor monitor: for a dark terminal, the other old one.
Theme phosphor()
{
    Theme theme{TurboCTheme::make()};
    theme.background = black;
    theme.text = light_green;
    theme.muted_text = green;
    theme.control_background = black;
    theme.panel_background = black;
    theme.panel_foreground = green;
    theme.side_panel_background = black;
    theme.shadow = dark_gray;
    theme.shadow_foreground = black;
    theme.focus_background = green;
    theme.focus_text = black;

    theme.dialog_background = green;
    theme.dialog_foreground = black;
    theme.dialog_border = light_green;
    theme.dialog_heading_foreground = white;
    theme.dialog_icon_foreground = light_green;
    theme.dialog_border_placing = white;
    theme.table_header_background = green;
    theme.table_header_foreground = black;
    theme.table_background = black;
    theme.table_foreground = light_green;
    theme.list_background = black;
    theme.list_foreground = green;
    theme.item_selected_background = light_green;
    theme.item_selected_foreground = black;
    theme.cluster_background = green;
    theme.cluster_foreground = black;
    theme.cluster_focused_foreground = white;
    theme.cluster_shortcut_foreground = white;

    theme.menu_background = black;
    theme.menu_foreground = light_green;
    theme.menu_border = green;
    theme.menu_selected_background = green;
    theme.menu_selected_foreground = black;
    theme.status_background = green;
    theme.status_foreground = black;

    theme.button_background = green;
    theme.button_foreground = black;
    theme.button_default_foreground = white;
    theme.button_shortcut_foreground = white;
    theme.label_shortcut_foreground = white;
    theme.button_focused_background = light_green;
    theme.button_focused_foreground = black;
    theme.button_pressed_background = black;
    theme.button_pressed_foreground = light_green;

    theme.tab_active_background = black;
    theme.tab_active_foreground = light_green;
    theme.tab_inactive_background = green;
    theme.tab_inactive_foreground = black;

    theme.selection_background = green;
    theme.selection_foreground = black;
    theme.search_match_background = light_green;
    theme.search_match_foreground = black;
    theme.occurrence_background = dark_gray;
    theme.occurrence_foreground = light_green;
    theme.bracket_match_background = red;
    theme.bracket_match_foreground = light_green;
    theme.diff_removed_background = red;
    theme.diff_removed_foreground = light_green;
    theme.diff_added_background = green;
    theme.diff_added_foreground = black;
    theme.diff_changed_background = green;
    theme.diff_changed_foreground = black;
    theme.diff_filler_background = dark_gray;
    theme.scrollbar_background = black;
    theme.scrollbar_foreground = green;

    theme.editor_background = black;
    theme.editor_foreground = light_green;
    theme.line_number_foreground = dark_gray;
    theme.reading_background = black;
    theme.reading_foreground = light_green;
    theme.syntax_keyword = white;
    theme.syntax_type = light_green;
    theme.syntax_function = light_green;
    theme.syntax_string = green;
    theme.syntax_number = green;
    theme.syntax_comment = dark_gray;
    theme.syntax_preprocessor = white;
    theme.syntax_operator = light_green;
    theme.heading_foreground = light_green;
    theme.code_foreground = light_green;
    theme.code_background = dark_gray;
    theme.link_foreground = white;

    theme.input_background = black;
    theme.input_foreground = light_green;
    theme.disabled_foreground = dark_gray;
    theme.hotkey_foreground = white;
    theme.error_foreground = light_red;
    theme.warning_foreground = yellow;
    theme.info_foreground = white;
    return theme;
}

// Light cyan on blue panels, cyan bars and grey dialogs, yellow where something stands out, as Norton Commander and
// Midnight Commander: for a file manager's look in any terminal.
Theme commander()
{
    Theme theme{TurboCTheme::make()};
    theme.background = blue;
    theme.text = light_cyan;
    theme.muted_text = dark_gray;
    theme.control_background = blue;
    theme.panel_background = blue;
    theme.panel_foreground = light_cyan;
    theme.side_panel_background = blue;
    theme.shadow = black;
    theme.shadow_foreground = dark_gray;
    theme.focus_background = cyan;
    theme.focus_text = black;

    theme.dialog_background = light_gray;
    theme.dialog_foreground = black;
    theme.dialog_border = white;
    theme.dialog_heading_foreground = blue;
    theme.dialog_icon_foreground = blue;
    theme.dialog_border_placing = yellow;
    theme.table_header_background = blue;
    theme.table_header_foreground = yellow;
    theme.table_background = blue;
    theme.table_foreground = light_cyan;
    theme.list_background = blue;
    theme.list_foreground = light_cyan;
    theme.item_selected_background = cyan;
    theme.item_selected_foreground = black;
    theme.cluster_background = light_gray;
    theme.cluster_foreground = black;
    theme.cluster_focused_foreground = blue;
    theme.cluster_shortcut_foreground = red;

    theme.menu_background = cyan;
    theme.menu_foreground = black;
    theme.menu_border = black;
    theme.menu_selected_background = black;
    theme.menu_selected_foreground = white;
    theme.status_background = cyan;
    theme.status_foreground = black;

    theme.button_background = cyan;
    theme.button_foreground = black;
    theme.button_default_foreground = blue;
    theme.button_shortcut_foreground = white;
    theme.label_shortcut_foreground = red;
    theme.button_focused_background = blue;
    theme.button_focused_foreground = white;
    theme.button_pressed_background = black;
    theme.button_pressed_foreground = cyan;

    theme.tab_active_background = blue;
    theme.tab_active_foreground = yellow;
    theme.tab_inactive_background = cyan;
    theme.tab_inactive_foreground = black;

    theme.selection_background = cyan;
    theme.selection_foreground = black;
    theme.search_match_background = yellow;
    theme.search_match_foreground = black;
    theme.occurrence_background = dark_gray;
    theme.occurrence_foreground = white;
    theme.bracket_match_background = magenta;
    theme.bracket_match_foreground = white;
    theme.diff_removed_background = red;
    theme.diff_removed_foreground = white;
    theme.diff_added_background = green;
    theme.diff_added_foreground = black;
    theme.diff_changed_background = brown;
    theme.diff_changed_foreground = white;
    theme.diff_filler_background = dark_gray;
    theme.scrollbar_background = cyan;
    theme.scrollbar_foreground = blue;

    theme.editor_background = blue;
    theme.editor_foreground = white;
    theme.line_number_foreground = cyan;
    theme.reading_background = blue;
    theme.reading_foreground = white;
    theme.syntax_keyword = yellow;
    theme.syntax_type = light_green;
    theme.syntax_function = white;
    theme.syntax_string = light_red;
    theme.syntax_number = light_magenta;
    theme.syntax_comment = cyan;
    theme.syntax_preprocessor = light_green;
    theme.syntax_operator = white;
    theme.heading_foreground = yellow;
    theme.code_foreground = light_green;
    theme.code_background = black;
    theme.link_foreground = light_cyan;

    theme.input_background = cyan;
    theme.input_foreground = black;
    theme.disabled_foreground = dark_gray;
    theme.hotkey_foreground = yellow;
    theme.error_foreground = light_red;
    theme.warning_foreground = yellow;
    theme.info_foreground = light_cyan;
    return theme;
}

} // namespace

std::vector<TerminalTheme> terminal_themes()
{
    return {{"turbo", translate("TerminalTheme", "Turbo C++"), TurboCTheme::make()},
            {"night", translate("TerminalTheme", "Night"), night()},
            {"light", translate("TerminalTheme", "Light"), light()},
            {"monochrome", translate("TerminalTheme", "Monochrome"), monochrome()},
            {"amber", translate("TerminalTheme", "Amber"), amber()},
            {"phosphor", translate("TerminalTheme", "Phosphor"), phosphor()},
            {"commander", translate("TerminalTheme", "Commander"), commander()}};
}

} // namespace olagarro::oui
