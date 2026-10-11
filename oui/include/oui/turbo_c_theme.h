#pragma once

#include <oui/theme.h>

namespace olagarro::oui
{

// The look of Turbo C++, Borland C++ and Turbo Pascal: Turbo Vision's colour palette (cpAppColor), the one their
// IDEs were built with, and their editor's colours. The comments name the palette entry each colour comes from.
class TurboCTheme
{
  public:
    static constexpr Color black{0, 0, 0};
    static constexpr Color blue{0, 0, 170};
    static constexpr Color green{0, 170, 0};
    static constexpr Color cyan{0, 170, 170};
    static constexpr Color red{170, 0, 0};
    static constexpr Color brown{170, 85, 0};
    static constexpr Color light_gray{170, 170, 170};
    static constexpr Color dark_gray{85, 85, 85};
    static constexpr Color light_green{85, 255, 85};
    static constexpr Color light_cyan{85, 255, 255};
    static constexpr Color light_red{255, 85, 85};
    static constexpr Color yellow{255, 255, 85};
    static constexpr Color white{255, 255, 255};

    [[nodiscard]] static Theme make()
    {
        Theme theme{};
        // The desktop, and the blue windows that hold text.
        theme.background = blue;
        theme.control_background = blue;
        theme.text = light_gray;
        // Panels and their lists, as a cyan window's: black on cyan.
        theme.panel_background = cyan;
        theme.panel_foreground = black;
        theme.side_panel_background = cyan;
        // A menu's shortcuts, as its text (0x70).
        theme.muted_text = black;
        // Shadows: what lies under them stays, dark grey on black.
        theme.shadow = black;
        theme.shadow_foreground = dark_gray;
        theme.focus_background = blue;
        theme.focus_text = white;

        // Gray dialog: frame and title white (0x7F), its icons light green (0x7A), static text black (0x70).
        theme.dialog_background = light_gray;
        theme.dialog_foreground = black;
        theme.dialog_border = white;
        theme.dialog_icon_foreground = light_green;
        theme.dialog_heading_foreground = black;
        theme.table_header_background = blue;
        theme.table_header_foreground = white;
        theme.table_background = cyan;
        theme.table_foreground = black;
        // List viewer: black on cyan (0x30), the focused item white on green (0x2F).
        theme.list_background = cyan;
        theme.list_foreground = black;
        theme.item_selected_background = green;
        theme.item_selected_foreground = white;
        // Cluster: black on cyan (0x30), the focused one white (0x3F).
        theme.cluster_background = cyan;
        theme.cluster_foreground = black;
        theme.cluster_focused_foreground = white;
        theme.cluster_shortcut_foreground = yellow;

        // Menus and the status line: black on grey (0x70), shortcuts red (0x74), disabled dark grey (0x78), the
        // selected item black on green (0x20).
        theme.menu_background = light_gray;
        theme.menu_foreground = black;
        theme.menu_border = black;
        theme.menu_selected_background = green;
        theme.menu_selected_foreground = black;
        theme.status_background = light_gray;
        theme.status_foreground = black;
        theme.hotkey_foreground = red;
        theme.disabled_foreground = dark_gray;

        // Buttons: black on green (0x20), the selected one white (0x2F).
        theme.button_background = green;
        theme.button_foreground = black;
        // The default button's text light cyan (0x2B).
        theme.button_default_foreground = light_cyan;
        // Shortcut letters yellow, on a button (0x2E) and on a dialog (0x7E).
        theme.button_shortcut_foreground = yellow;
        theme.label_shortcut_foreground = yellow;
        theme.button_focused_background = green;
        theme.button_focused_foreground = white;
        theme.button_pressed_background = green;
        theme.button_pressed_foreground = white;

        // The documents' row: titles as on a blue window's frame, white the active one (0x1F), light grey the rest
        // (0x17).
        theme.tab_active_background = blue;
        theme.tab_active_foreground = white;
        theme.tab_inactive_background = blue;
        theme.tab_inactive_foreground = light_gray;

        // The editor: yellow on blue (0x1E), its selection blue on light grey (0x71).
        theme.editor_background = blue;
        theme.editor_foreground = yellow;
        theme.selection_background = light_gray;
        theme.selection_foreground = blue;
        theme.line_number_foreground = cyan;
        theme.search_match_background = brown;
        theme.search_match_foreground = white;
        theme.occurrence_background = cyan;
        theme.occurrence_foreground = black;
        theme.bracket_match_background = green;
        theme.bracket_match_foreground = white;
        // Comparing: white on red removed and on green added, as an error and a button are; a changed line black on
        // cyan, as a list; and the room facing nothing black, as a shadow.
        theme.diff_removed_background = red;
        theme.diff_removed_foreground = white;
        theme.diff_added_background = green;
        theme.diff_added_foreground = white;
        theme.diff_changed_background = cyan;
        theme.diff_changed_foreground = black;
        theme.diff_filler_background = black;
        // A blue window's scroll bar: blue on cyan (0x31).
        theme.scrollbar_background = cyan;
        theme.scrollbar_foreground = blue;
        // Turbo C++'s highlighting: reserved words white, comments light grey, strings and numbers light cyan,
        // preprocessor light green; everything else in the editor's yellow.
        theme.syntax_keyword = white;
        theme.syntax_type = white;
        theme.syntax_function = yellow;
        theme.syntax_string = light_cyan;
        theme.syntax_number = light_cyan;
        theme.syntax_comment = light_gray;
        theme.syntax_preprocessor = light_green;
        theme.syntax_operator = yellow;

        // Help, as Turbo C++'s own: yellow headings, cyan code on black.
        theme.heading_foreground = yellow;
        theme.code_foreground = light_cyan;
        theme.code_background = black;
        theme.link_foreground = light_green;

        // Input line: white on blue (0x1F).
        theme.input_background = blue;
        theme.input_foreground = white;
        theme.error_foreground = light_red;
        theme.warning_foreground = yellow;
        theme.info_foreground = light_cyan;
        return theme;
    }
};

} // namespace olagarro::oui
