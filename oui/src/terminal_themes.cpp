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

// The few colours a look softer on the eyes is made of, in any colour a terminal of 24 bits shows; one of 256 or 16
// shows the nearest it has.
struct Palette
{
    // The editor, lists and fields, their text, and what is quieter: line numbers, comments, what cannot be chosen.
    Color editor;
    Color text;
    Color muted;
    // Menus, dialogs, the status bar and the side panel, and their text.
    Color surface;
    Color surface_text;
    // What has the focus or is selected, and its text.
    Color accent;
    Color accent_text;
    // Shortcut letters, headings and the default button.
    Color highlight;
    Color keyword;
    Color type;
    Color string;
    Color number;
    Color preprocessor;
    Color operator_;
    // A search's match, and lines added, removed and changed in a comparison, with the text on them.
    Color match;
    Color match_text;
    Color added;
    Color removed;
    Color changed;
    Color difference_text;
    Color error;
    Color shadow;
};

Theme from_palette(const Palette& p)
{
    Theme theme{TurboCTheme::make()};
    theme.button_shortcut_foreground = p.highlight;
    theme.label_shortcut_foreground = p.highlight;
    theme.button_default_foreground = p.highlight;
    theme.shadow_foreground = p.muted;
    theme.dialog_icon_foreground = p.string;
    theme.list_background = p.editor;
    theme.list_foreground = p.text;
    theme.item_selected_background = p.accent;
    theme.item_selected_foreground = p.accent_text;
    theme.cluster_background = p.surface;
    theme.cluster_foreground = p.surface_text;
    theme.cluster_focused_foreground = p.highlight;
    theme.cluster_shortcut_foreground = p.highlight;
    theme.background = p.editor;
    theme.text = p.text;
    theme.muted_text = p.muted;
    theme.control_background = p.editor;
    theme.panel_background = p.editor;
    theme.panel_foreground = p.text;
    theme.side_panel_background = p.surface;
    theme.shadow = p.shadow;
    theme.focus_background = p.accent;
    theme.focus_text = p.accent_text;

    theme.dialog_background = p.surface;
    theme.dialog_foreground = p.surface_text;
    theme.dialog_border = p.muted;
    theme.dialog_border_placing = p.highlight;
    theme.dialog_heading_foreground = p.highlight;
    theme.table_header_background = p.surface;
    theme.table_header_foreground = p.surface_text;
    theme.table_background = p.editor;
    theme.table_foreground = p.text;

    theme.menu_background = p.surface;
    theme.menu_foreground = p.surface_text;
    theme.menu_border = p.muted;
    theme.menu_selected_background = p.accent;
    theme.menu_selected_foreground = p.accent_text;
    theme.status_background = p.surface;
    theme.status_foreground = p.surface_text;

    theme.button_background = p.editor;
    theme.button_foreground = p.text;
    theme.button_focused_background = p.accent;
    theme.button_focused_foreground = p.accent_text;
    theme.button_pressed_background = p.accent;
    theme.button_pressed_foreground = p.accent_text;

    theme.tab_active_background = p.editor;
    theme.tab_active_foreground = p.text;
    theme.tab_inactive_background = p.surface;
    theme.tab_inactive_foreground = p.muted;

    theme.selection_background = p.accent;
    theme.selection_foreground = p.accent_text;
    theme.search_match_background = p.match;
    theme.search_match_foreground = p.match_text;
    theme.occurrence_background = p.surface;
    theme.occurrence_foreground = p.text;
    theme.bracket_match_background = p.added;
    theme.bracket_match_foreground = p.difference_text;
    theme.diff_removed_background = p.removed;
    theme.diff_removed_foreground = p.difference_text;
    theme.diff_added_background = p.added;
    theme.diff_added_foreground = p.difference_text;
    theme.diff_changed_background = p.changed;
    theme.diff_changed_foreground = p.difference_text;
    theme.diff_filler_background = p.shadow;
    theme.scrollbar_background = p.surface;
    theme.scrollbar_foreground = p.muted;

    theme.editor_background = p.editor;
    theme.editor_foreground = p.text;
    theme.line_number_foreground = p.muted;
    theme.reading_background = p.editor;
    theme.reading_foreground = p.text;
    theme.syntax_keyword = p.keyword;
    theme.syntax_type = p.type;
    theme.syntax_function = p.text;
    theme.syntax_string = p.string;
    theme.syntax_number = p.number;
    theme.syntax_comment = p.muted;
    theme.syntax_preprocessor = p.preprocessor;
    theme.syntax_operator = p.operator_;
    theme.heading_foreground = p.highlight;
    theme.code_foreground = p.type;
    theme.code_background = p.surface;
    theme.link_foreground = p.keyword;

    theme.input_background = p.editor;
    theme.input_foreground = p.text;
    theme.disabled_foreground = p.muted;
    theme.hotkey_foreground = p.highlight;
    theme.error_foreground = p.error;
    theme.warning_foreground = p.number;
    theme.info_foreground = p.keyword;
    return theme;
}

// Light text on a blue-grey slate, the surfaces a shade lighter: a dark look without black's glare.
Theme slate()
{
    Palette p;
    p.editor = {38, 42, 50};
    p.text = {222, 226, 232};
    p.muted = {128, 136, 150};
    p.surface = {54, 60, 72};
    p.surface_text = {230, 232, 238};
    p.accent = {62, 110, 180};
    p.accent_text = {255, 255, 255};
    p.highlight = {240, 200, 110};
    p.keyword = {130, 170, 255};
    p.type = {110, 210, 200};
    p.string = {160, 210, 130};
    p.number = {230, 160, 120};
    p.preprocessor = {200, 150, 230};
    p.operator_ = {190, 196, 206};
    p.match = {150, 110, 30};
    p.match_text = {255, 255, 255};
    p.added = {40, 100, 60};
    p.removed = {130, 44, 44};
    p.changed = {50, 70, 100};
    p.difference_text = {255, 255, 255};
    p.error = {240, 110, 110};
    p.shadow = {20, 22, 26};
    return from_palette(p);
}

// Dark ink on a warm page: a light look without white's glare.
Theme paper()
{
    Palette p;
    p.editor = {250, 247, 238};
    p.text = {36, 36, 40};
    p.muted = {110, 108, 100};
    p.surface = {226, 220, 206};
    p.surface_text = {30, 30, 34};
    p.accent = {40, 90, 170};
    p.accent_text = {255, 255, 255};
    p.highlight = {170, 60, 20};
    p.keyword = {20, 80, 170};
    p.type = {0, 110, 120};
    p.string = {40, 110, 30};
    p.number = {160, 70, 10};
    p.preprocessor = {120, 40, 140};
    p.operator_ = {70, 70, 74};
    p.match = {240, 200, 90};
    p.match_text = {30, 30, 34};
    p.added = {190, 226, 180};
    p.removed = {240, 190, 180};
    p.changed = {200, 215, 235};
    p.difference_text = {30, 30, 34};
    p.error = {180, 30, 30};
    p.shadow = {196, 190, 176};
    return from_palette(p);
}

// Pale text on a deep night blue: Turbo's blue, darkened to rest the eyes.
Theme midnight()
{
    Palette p;
    p.editor = {16, 24, 44};
    p.text = {214, 222, 240};
    p.muted = {120, 134, 166};
    p.surface = {30, 42, 72};
    p.surface_text = {226, 232, 246};
    p.accent = {60, 100, 190};
    p.accent_text = {255, 255, 255};
    p.highlight = {255, 210, 110};
    p.keyword = {120, 170, 255};
    p.type = {100, 210, 230};
    p.string = {170, 220, 140};
    p.number = {240, 170, 120};
    p.preprocessor = {210, 160, 240};
    p.operator_ = {180, 190, 215};
    p.match = {160, 110, 30};
    p.match_text = {255, 255, 255};
    p.added = {30, 100, 70};
    p.removed = {130, 40, 60};
    p.changed = {44, 64, 110};
    p.difference_text = {255, 255, 255};
    p.error = {250, 120, 130};
    p.shadow = {6, 10, 20};
    return from_palette(p);
}

// White on black and bright colours, what is chosen in yellow: the most contrast there is, for eyes that need it.
Theme contrast()
{
    Palette p;
    p.editor = {0, 0, 0};
    p.text = {255, 255, 255};
    p.muted = {170, 170, 170};
    p.surface = {24, 24, 24};
    p.surface_text = {255, 255, 255};
    p.accent = {255, 230, 0};
    p.accent_text = {0, 0, 0};
    p.highlight = {255, 230, 0};
    p.keyword = {120, 200, 255};
    p.type = {0, 255, 255};
    p.string = {120, 255, 120};
    p.number = {255, 180, 80};
    p.preprocessor = {255, 140, 255};
    p.operator_ = {255, 255, 255};
    p.match = {255, 230, 0};
    p.match_text = {0, 0, 0};
    p.added = {0, 110, 0};
    p.removed = {170, 0, 0};
    p.changed = {0, 60, 140};
    p.difference_text = {255, 255, 255};
    p.error = {255, 90, 90};
    p.shadow = {0, 0, 0};
    return from_palette(p);
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
            {"commander", translate("TerminalTheme", "Commander"), commander()},
            {"slate", translate("TerminalTheme", "Slate"), slate()},
            {"paper", translate("TerminalTheme", "Paper"), paper()},
            {"midnight", translate("TerminalTheme", "Midnight"), midnight()},
            {"contrast", translate("TerminalTheme", "High Contrast"), contrast()}};
}

} // namespace olagarro::oui
