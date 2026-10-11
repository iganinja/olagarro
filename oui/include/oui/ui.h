#pragma once

#include <oui/declarative.h>
#include <oui/widget.h>
#include <oui/window.h>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace olagarro::oui
{

// A user interface built from its description, as declarative.h writes one: what there is and where, while the code
// says what it does, finding each widget by its id:
//
//     auto ui{build_ui(description)};
//     ui.get<Button>("find").on_clicked([&] { ... });
//     application.show_modal(ui.take_window());
//
// The widgets and their properties, and what each means:
//
//     Window "title"         width=cells, max_width, min_width, min_height, height_share=0.9 (of the screen),
//                            resizable, border=none|single|double, placement=key (where it was is remembered under
//                            it), keep_on_escape; a widget alone in it fills it, several go one under the other
//     Column, Row            gap=none|tight|normal|loose; a Row's centered
//     Grid                   columns=content,fill,12 and rows=content,flex (each content, fill, flex or cells), gap;
//                            its children fill it a row at a time
//     Split                  vertical (one over the other rather than side by side), ratio=0.4 (the first's share);
//                            its two widgets
//     Dock                   (the code fills it, saying where each widget goes)
//     Tabs                   fixed (the tabs cannot be closed); Tab "title" in it, each holding a widget or none
//     Label "text"           buddy=id (the field Alt and its letter go to; with none the & does not show, and && is
//                            an & written), bold, heading, wrap, background
//     LineEdit               text="...", placeholder="...", background
//     TextEdit               read_only, wrap
//     MarkdownView           background, foreground
//     CheckButton "text"     checked, background
//     Button "text"          default, compact, flat (without its shadow)
//     ComboBox               (its options are the code's to give)
//     VSpacer, HSpacer       height=rows, width=cells; 1 unless given
//     Slot                   a place the code fills with a widget of its own, through put
//
// Every widget takes id=name, hidden, and tooltip="..." for what a pointer resting on it is told. A colour,
// background=... or foreground=..., is one of the theme's, by the name it has in Theme: background=table_background.
// Texts, a widget's own, a placeholder and a tooltip, are translated under
// the context the root names, context=Name; a text that shows as it is written, as a name or an arrow, says
// untranslated.

// What is wrong with a description and where, from 1; what() says all three: "2:3: There is no widget called Slider."
class UiError : public std::runtime_error
{
  public:
    UiError(std::size_t line, std::size_t column, const std::string& message);

    [[nodiscard]] std::size_t line() const;
    [[nodiscard]] std::size_t column() const;
    [[nodiscard]] const std::string& message() const;

  private:
    std::size_t m_line;
    std::size_t m_column;
    std::string m_message;
};

// A text a description shows, to translate, and where it is written.
struct UiText
{
    std::string context;
    std::string text;
    std::size_t line;
    std::size_t column;
};

// The texts a description shows, in order, under the context its root names, for a translation's catalog to cover.
[[nodiscard]] std::vector<UiText> ui_texts(const DeclarativeNode& root);

class Ui
{
  public:
    Ui(std::unique_ptr<Widget> root, std::vector<std::pair<std::string, Widget*>> named);

    [[nodiscard]] Widget& root() const;
    // The root, to put where it shows: in a window, or shown as one.
    [[nodiscard]] std::unique_ptr<Widget> take();
    // The root, a Window, to show; none when the root is another widget.
    [[nodiscard]] std::unique_ptr<Window> take_window();

    // The widget with the id, as what it is. Asking for one there is not, or as what it is not, is a mistake in the
    // code, and throws std::logic_error.
    template <typename Kind> [[nodiscard]] Kind& get(std::string_view id) const
    {
        auto* const found{dynamic_cast<Kind*>(&named(id))};
        if(found == nullptr)
        {
            throw std::logic_error{"The widget " + std::string{id} + " is not of the kind asked for."};
        }
        return *found;
    }

    // Puts the widget in the Slot with the id, which it then answers to. One that is not a Slot is a mistake in the
    // code, and throws std::logic_error.
    template <typename Kind> Kind& put(std::string_view id, std::unique_ptr<Kind> widget)
    {
        auto& result{*widget};
        fill(id, std::move(widget));
        return result;
    }

    // Whether the widget with the id is a Slot, still to be filled.
    [[nodiscard]] bool is_slot(std::string_view id) const;

    // The widget with the id out of the description, for the code to put somewhere of its own; get still finds it.
    [[nodiscard]] std::unique_ptr<Widget> take(std::string_view id);

    // The widgets with an id, in the order they are written.
    [[nodiscard]] const std::vector<std::pair<std::string, Widget*>>& widgets() const;

  private:
    [[nodiscard]] Widget& named(std::string_view id) const;
    void fill(std::string_view id, std::unique_ptr<Widget> widget);

    std::unique_ptr<Widget> m_root;
    Widget* m_root_reference{};
    std::vector<std::pair<std::string, Widget*>> m_named;
};

// What fits where the cursor is in a description, for an editor to suggest: what each is called, what it is, and
// what goes in, "$0" where the cursor goes after.
struct UiSuggestion
{
    std::string label;
    std::string detail;
    std::string text;
};

// The suggestions at `cursor`, and where what they replace starts: at a line's start the widgets; after a widget and
// its text, the properties it takes and has not been given; after name=, its values, from a list, the theme's colours
// or the ids the description gives. Those that start as what is written there does, whatever the case; nothing in a
// text or a comment.
struct UiSuggestions
{
    std::size_t start{};
    std::vector<UiSuggestion> items;
};
[[nodiscard]] UiSuggestions ui_suggestions(std::string_view description, std::size_t cursor);

// Whether a description's texts are translated under its context, or show as they are written, as a script's do; then
// it needs no context.
enum class UiTexts
{
    translated,
    as_written,
};

// Builds what the description says, its texts as `texts` says. A description that is not one throws UiError, which
// says where and what is wrong: "2:3: There is no widget called Slider."
[[nodiscard]] Ui build_ui(std::string_view description, UiTexts texts = UiTexts::translated);

} // namespace olagarro::oui
