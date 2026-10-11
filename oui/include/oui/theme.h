#pragma once

#include <oui/color.h>

#include <optional>
#include <string_view>

namespace olagarro::oui
{

struct Theme
{
    Color background{0, 0, 170};
    Color text{170, 170, 170};
    Color muted_text{85, 85, 85};
    Color control_background{0, 0, 170};
    Color panel_background{0, 0, 170};
    Color panel_foreground{170, 170, 170};
    Color shadow{0, 0, 0};
    // What stays visible under a shadow: the cells there keep their characters, in this colour on `shadow`.
    Color shadow_foreground{85, 85, 85};
    Color focus_background{0, 0, 170};
    Color focus_text{255, 255, 255};

    Color dialog_background{170, 170, 170};
    Color dialog_foreground{0, 0, 0};
    Color dialog_border{255, 255, 255};
    // A title inside a dialog, over a group of its controls.
    Color dialog_heading_foreground{0, 0, 170};
    // The marks on a window's frame, as its close button.
    Color dialog_icon_foreground{85, 255, 85};
    // The frame of a window being moved or resized from the keyboard.
    Color dialog_border_placing{0, 170, 0};
    Color table_header_background{85, 85, 85};
    Color table_header_foreground{255, 255, 255};
    Color table_background{170, 170, 170};
    Color table_foreground{0, 0, 0};
    // A list to pick from, and the item chosen in it or in a field of text; apart from the selection in an editor.
    Color list_background{0, 170, 170};
    Color list_foreground{0, 0, 0};
    Color item_selected_background{0, 170, 0};
    Color item_selected_foreground{255, 255, 255};
    // A group of check boxes or radio buttons, and the one with the focus.
    Color cluster_background{0, 170, 170};
    Color cluster_foreground{0, 0, 0};
    Color cluster_focused_foreground{255, 255, 255};
    // The letter that turns one with Alt.
    Color cluster_shortcut_foreground{255, 255, 85};

    Color menu_background{170, 170, 170};
    Color menu_foreground{0, 0, 0};
    Color menu_border{255, 255, 255};
    Color menu_selected_background{0, 0, 170};
    Color menu_selected_foreground{255, 255, 255};
    Color status_background{170, 170, 170};
    Color status_foreground{0, 0, 0};

    Color button_background{85, 255, 85};
    Color button_foreground{0, 0, 0};
    // The text of the button Enter presses from anywhere in its dialog.
    Color button_default_foreground{85, 255, 255};
    // The letter that presses a button with Alt, and the one that goes to what a label names.
    Color button_shortcut_foreground{255, 255, 85};
    Color label_shortcut_foreground{255, 255, 85};
    Color button_focused_background{255, 255, 255};
    Color button_focused_foreground{0, 0, 0};
    Color button_pressed_background{0, 128, 0};
    Color button_pressed_foreground{255, 255, 255};

    Color tab_active_background{85, 85, 255};
    Color tab_active_foreground{255, 255, 255};
    Color tab_inactive_background{0, 0, 170};
    Color tab_inactive_foreground{85, 85, 255};

    Color selection_background{255, 255, 255};
    Color selection_foreground{0, 0, 0};
    // Every match of a search, around the selected one.
    Color search_match_background{170, 85, 0};
    Color search_match_foreground{255, 255, 255};
    // The other places with the text selected in an editor, as where else a name is used.
    Color occurrence_background{85, 85, 85};
    Color occurrence_foreground{255, 255, 255};
    // The bracket beside the cursor in a text editor, and its pair.
    Color bracket_match_background{0, 170, 0};
    Color bracket_match_foreground{255, 255, 255};
    // Two texts compared side by side: a line only on the left, removed, and only on the right, added, which are also
    // the colours of what changed within a line on both sides but not the same; such a line; and the room facing a
    // line that the other side does not have.
    Color diff_removed_background{170, 0, 0};
    Color diff_removed_foreground{255, 255, 255};
    Color diff_added_background{0, 170, 0};
    Color diff_added_foreground{255, 255, 255};
    Color diff_changed_background{0, 170, 170};
    Color diff_changed_foreground{0, 0, 0};
    Color diff_filler_background{0, 0, 0};
    Color scrollbar_background{85, 85, 85};
    Color scrollbar_foreground{170, 170, 170};

    Color editor_background{0, 0, 170};
    Color editor_foreground{170, 170, 170};
    // The numbers of the lines, beside the text.
    Color line_number_foreground{0, 170, 170};
    // Text to read rather than edit, such as help, set apart from the documents: black, as a manual page.
    Color reading_background{0, 0, 0};
    Color reading_foreground{170, 170, 170};
    // Panels beside the main content, such as a project tree.
    Color side_panel_background{0, 85, 85};
    // Source text by kind, as a syntax highlighter tells it: from the sixteen colours of a text console, so it
    // reads the same on the Linux console.
    Color syntax_keyword{255, 255, 255};
    Color syntax_type{85, 255, 255};
    Color syntax_function{255, 255, 255};
    Color syntax_string{85, 255, 85};
    Color syntax_number{255, 85, 255};
    Color syntax_comment{0, 170, 170};
    Color syntax_preprocessor{255, 255, 85};
    Color syntax_operator{170, 170, 170};

    // Formatted text, such as Markdown: headings, code and links stand out from the editor colours.
    Color heading_foreground{255, 255, 85};
    Color code_foreground{85, 255, 255};
    Color code_background{0, 0, 0};
    Color link_foreground{85, 255, 85};

    Color input_background{170, 170, 170};
    Color input_foreground{0, 0, 0};

    Color disabled_foreground{85, 85, 85};
    Color hotkey_foreground{170, 0, 0};
    Color error_foreground{255, 85, 85};
    Color warning_foreground{255, 255, 85};
    Color info_foreground{85, 255, 255};
};

// A colour that is either fixed or one of the theme's, read when painting, so it follows the application
// changing its theme: set_color(&Theme::error_foreground).
class ColorRef
{
  public:
    ColorRef(Color color) : m_color{color} {}
    // So a braced colour, set_background({0, 85, 85}), still reads as a fixed one.
    ColorRef(std::uint8_t red, std::uint8_t green, std::uint8_t blue) : m_color{red, green, blue} {}
    ColorRef(Color Theme::*const field) : m_field{field} {}

    [[nodiscard]] Color resolve(const Theme& theme) const { return m_field != nullptr ? theme.*m_field : m_color; }

  private:
    Color m_color{};
    Color Theme::*m_field{};
};

// The colour a widget was given, or `fallback` from the theme when it was given none.
[[nodiscard]] inline Color resolve(const std::optional<ColorRef>& color, const Theme& theme, Color fallback)
{
    return color ? color->resolve(theme) : fallback;
}

// The theme's colour by its name in Theme, as a description or a script names it: "table_background".
[[nodiscard]] std::optional<Color Theme::*> named_theme_color(std::string_view name);

} // namespace olagarro::oui
