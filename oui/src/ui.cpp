#include <oui/ui.h>

#include <oui/button.h>
#include <oui/check_button.h>
#include <oui/combo_box.h>
#include <oui/command.h>
#include <oui/dock_layout.h>
#include <oui/grid.h>
#include <oui/label.h>
#include <oui/layout.h>
#include <oui/line_edit.h>
#include <oui/progress_bar.h>
#include <oui/markdown_view.h>
#include <oui/spacer.h>
#include <oui/split_view.h>
#include <oui/tab_view.h>
#include <oui/text_edit.h>
#include <oui/theme.h>
#include <oui/translation.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>

namespace olagarro::oui
{

namespace
{

// The properties each widget takes, besides those every one does: id and hidden, untranslated when it has a text,
// and context on the root.
struct Kind
{
    std::string_view type;
    std::vector<std::string_view> properties;
    // Whether it shows a text of its own, the one in quotes.
    bool has_text;
};

[[nodiscard]] const std::vector<Kind>& kinds()
{
    static const std::vector<Kind> all{
        {"Window",
         {"width", "max_width", "min_width", "min_height", "height_share", "resizable", "border", "placement",
          "keep_on_escape"},
         true},
        {"Column", {"gap"}, false},
        {"Row", {"gap", "centered"}, false},
        {"Grid", {"columns", "rows", "gap"}, false},
        {"Split", {"vertical", "ratio"}, false},
        {"Dock", {}, false},
        {"Tabs", {"fixed"}, false},
        {"Tab", {}, true},
        {"Label", {"buddy", "bold", "heading", "wrap", "background"}, true},
        {"LineEdit", {"text", "placeholder", "background", "password"}, false},
        {"ProgressBar", {"value", "text"}, false},
        {"TextEdit", {"read_only", "wrap"}, false},
        {"MarkdownView", {"background", "foreground"}, false},
        {"CheckButton", {"checked", "background"}, true},
        {"Button", {"default", "compact", "flat"}, true},
        {"ComboBox", {}, false},
        {"VSpacer", {"height"}, false},
        {"HSpacer", {"width"}, false},
        {"Slot", {}, false},
    };
    return all;
}

// The theme's colours by their names in Theme.
[[nodiscard]] const std::vector<std::pair<std::string_view, Color Theme::*>>& theme_colors()
{
    static const std::vector<std::pair<std::string_view, Color Theme::*>> colors{
        {"background", &Theme::background},
        {"text", &Theme::text},
        {"muted_text", &Theme::muted_text},
        {"control_background", &Theme::control_background},
        {"panel_background", &Theme::panel_background},
        {"panel_foreground", &Theme::panel_foreground},
        {"shadow", &Theme::shadow},
        {"shadow_foreground", &Theme::shadow_foreground},
        {"focus_background", &Theme::focus_background},
        {"focus_text", &Theme::focus_text},
        {"dialog_background", &Theme::dialog_background},
        {"dialog_foreground", &Theme::dialog_foreground},
        {"dialog_border", &Theme::dialog_border},
        {"dialog_heading_foreground", &Theme::dialog_heading_foreground},
        {"dialog_icon_foreground", &Theme::dialog_icon_foreground},
        {"dialog_border_placing", &Theme::dialog_border_placing},
        {"table_header_background", &Theme::table_header_background},
        {"table_header_foreground", &Theme::table_header_foreground},
        {"table_background", &Theme::table_background},
        {"table_foreground", &Theme::table_foreground},
        {"list_background", &Theme::list_background},
        {"list_foreground", &Theme::list_foreground},
        {"item_selected_background", &Theme::item_selected_background},
        {"item_selected_foreground", &Theme::item_selected_foreground},
        {"cluster_background", &Theme::cluster_background},
        {"cluster_foreground", &Theme::cluster_foreground},
        {"cluster_focused_foreground", &Theme::cluster_focused_foreground},
        {"cluster_shortcut_foreground", &Theme::cluster_shortcut_foreground},
        {"menu_background", &Theme::menu_background},
        {"menu_foreground", &Theme::menu_foreground},
        {"menu_border", &Theme::menu_border},
        {"menu_selected_background", &Theme::menu_selected_background},
        {"menu_selected_foreground", &Theme::menu_selected_foreground},
        {"status_background", &Theme::status_background},
        {"status_foreground", &Theme::status_foreground},
        {"button_background", &Theme::button_background},
        {"button_foreground", &Theme::button_foreground},
        {"button_default_foreground", &Theme::button_default_foreground},
        {"button_shortcut_foreground", &Theme::button_shortcut_foreground},
        {"label_shortcut_foreground", &Theme::label_shortcut_foreground},
        {"button_focused_background", &Theme::button_focused_background},
        {"button_focused_foreground", &Theme::button_focused_foreground},
        {"button_pressed_background", &Theme::button_pressed_background},
        {"button_pressed_foreground", &Theme::button_pressed_foreground},
        {"tab_active_background", &Theme::tab_active_background},
        {"tab_active_foreground", &Theme::tab_active_foreground},
        {"tab_inactive_background", &Theme::tab_inactive_background},
        {"tab_inactive_foreground", &Theme::tab_inactive_foreground},
        {"selection_background", &Theme::selection_background},
        {"selection_foreground", &Theme::selection_foreground},
        {"search_match_background", &Theme::search_match_background},
        {"search_match_foreground", &Theme::search_match_foreground},
        {"occurrence_background", &Theme::occurrence_background},
        {"occurrence_foreground", &Theme::occurrence_foreground},
        {"bracket_match_background", &Theme::bracket_match_background},
        {"bracket_match_foreground", &Theme::bracket_match_foreground},
        {"diff_removed_background", &Theme::diff_removed_background},
        {"diff_removed_foreground", &Theme::diff_removed_foreground},
        {"diff_added_background", &Theme::diff_added_background},
        {"diff_added_foreground", &Theme::diff_added_foreground},
        {"diff_changed_background", &Theme::diff_changed_background},
        {"diff_changed_foreground", &Theme::diff_changed_foreground},
        {"diff_filler_background", &Theme::diff_filler_background},
        {"scrollbar_background", &Theme::scrollbar_background},
        {"scrollbar_foreground", &Theme::scrollbar_foreground},
        {"editor_background", &Theme::editor_background},
        {"editor_foreground", &Theme::editor_foreground},
        {"line_number_foreground", &Theme::line_number_foreground},
        {"reading_background", &Theme::reading_background},
        {"reading_foreground", &Theme::reading_foreground},
        {"side_panel_background", &Theme::side_panel_background},
        {"syntax_keyword", &Theme::syntax_keyword},
        {"syntax_type", &Theme::syntax_type},
        {"syntax_function", &Theme::syntax_function},
        {"syntax_string", &Theme::syntax_string},
        {"syntax_number", &Theme::syntax_number},
        {"syntax_comment", &Theme::syntax_comment},
        {"syntax_preprocessor", &Theme::syntax_preprocessor},
        {"syntax_operator", &Theme::syntax_operator},
        {"heading_foreground", &Theme::heading_foreground},
        {"code_foreground", &Theme::code_foreground},
        {"code_background", &Theme::code_background},
        {"link_foreground", &Theme::link_foreground},
        {"input_background", &Theme::input_background},
        {"input_foreground", &Theme::input_foreground},
        {"disabled_foreground", &Theme::disabled_foreground},
        {"hotkey_foreground", &Theme::hotkey_foreground},
        {"error_foreground", &Theme::error_foreground},
        {"warning_foreground", &Theme::warning_foreground},
        {"info_foreground", &Theme::info_foreground},
    };
    return colors;
}

[[nodiscard]] std::optional<Color Theme::*> theme_color(std::string_view name)
{
    const auto& colors{theme_colors()};
    const auto found{
        std::find_if(colors.begin(), colors.end(), [name](const auto& color) { return color.first == name; })};
    return found == colors.end() ? std::nullopt : std::optional<Color Theme::*>{found->second};
}

// What a property's value is, for an editor to suggest it and write it as it goes in.
enum class Value
{
    // A name alone, with no value.
    flag,
    number,
    // In quotes.
    text,
    // A word, as an id.
    word,
    choice,
    lengths,
    colour,
    // The id of another widget in the description.
    id,
};

struct Property
{
    std::string_view name;
    Value value;
    // What it is for, in the interface's language: for a suggestion.
    std::string (*detail)();
    std::vector<std::string_view> choices{};
};

[[nodiscard]] const std::vector<Property>& properties()
{
    static const std::vector<Property> all{
        {"id", Value::word, [] { return translate("DescriptionProperty", "how the code finds it"); }},
        {"hidden", Value::flag, [] { return translate("DescriptionProperty", "not shown until the code shows it"); }},
        {"tooltip", Value::text,
         [] { return translate("DescriptionProperty", "what a pointer resting on it is told"); }},
        {"untranslated", Value::flag, [] { return translate("DescriptionProperty", "its text shows as written"); }},
        {"context", Value::word,
         [] { return translate("DescriptionProperty", "what its texts are translated under"); }},
        {"width", Value::number, [] { return translate("DescriptionProperty", "cells wide"); }},
        {"max_width", Value::number, [] { return translate("DescriptionProperty", "cells wide at most"); }},
        {"min_width", Value::number, [] { return translate("DescriptionProperty", "cells wide at least"); }},
        {"min_height", Value::number, [] { return translate("DescriptionProperty", "rows high at least"); }},
        {"height", Value::number, [] { return translate("DescriptionProperty", "rows high"); }},
        {"height_share", Value::number,
         [] { return translate("DescriptionProperty", "of the screen's height, as 0.9"); }},
        {"resizable", Value::flag, [] { return translate("DescriptionProperty", "the mouse resizes it"); }},
        {"border",
         Value::choice,
         [] { return translate("DescriptionProperty", "none, single or double"); },
         {"none", "single", "double"}},
        {"placement", Value::word, [] { return translate("DescriptionProperty", "where it was is remembered under"); }},
        {"keep_on_escape", Value::flag, [] { return translate("DescriptionProperty", "Escape does not close it"); }},
        {"gap",
         Value::choice,
         [] { return translate("DescriptionProperty", "none, tight, normal or loose"); },
         {"none", "tight", "normal", "loose"}},
        {"centered", Value::flag, [] { return translate("DescriptionProperty", "its widgets in the middle"); }},
        {"columns", Value::lengths,
         [] { return translate("DescriptionProperty", "content, fill, flex or cells, each"); }},
        {"rows", Value::lengths, [] { return translate("DescriptionProperty", "content, fill, flex or cells, each"); }},
        {"vertical", Value::flag, [] { return translate("DescriptionProperty", "one over the other"); }},
        {"ratio", Value::number, [] { return translate("DescriptionProperty", "the first's share, as 0.4"); }},
        {"value", Value::number,
         [] { return translate("DescriptionProperty", "how much is done, from 0 to 1, as 0.25"); }},
        {"fixed", Value::flag, [] { return translate("DescriptionProperty", "its tabs cannot be closed"); }},
        {"buddy", Value::id, [] { return translate("DescriptionProperty", "the field its letter goes to"); }},
        {"bold", Value::flag, [] { return translate("DescriptionProperty", "in bold"); }},
        {"heading", Value::flag, [] { return translate("DescriptionProperty", "a heading over a part"); }},
        {"wrap", Value::flag, [] { return translate("DescriptionProperty", "its text wraps"); }},
        {"background", Value::colour, [] { return translate("DescriptionProperty", "a colour of the theme"); }},
        {"foreground", Value::colour, [] { return translate("DescriptionProperty", "a colour of the theme"); }},
        {"text", Value::text, [] { return translate("DescriptionProperty", "what it starts with"); }},
        {"placeholder", Value::text, [] { return translate("DescriptionProperty", "what it says while empty"); }},
        {"read_only", Value::flag, [] { return translate("DescriptionProperty", "it only shows"); }},
        {"password", Value::flag,
         [] { return translate("DescriptionProperty", "a dot shows for each character typed"); }},
        {"checked", Value::flag, [] { return translate("DescriptionProperty", "ticked"); }},
        {"default", Value::flag, [] { return translate("DescriptionProperty", "Enter presses it"); }},
        {"compact", Value::flag, [] { return translate("DescriptionProperty", "one row"); }},
        {"flat", Value::flag, [] { return translate("DescriptionProperty", "without its shadow"); }},
    };
    return all;
}

[[nodiscard]] const Property* property_named(std::string_view name)
{
    const auto found{std::find_if(properties().begin(), properties().end(),
                                  [name](const Property& each) { return each.name == name; })};
    return found == properties().end() ? nullptr : &*found;
}

// Whether `text` starts as `prefix`, whatever the case of its letters.
[[nodiscard]] bool starts_as(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() &&
           std::equal(
               prefix.begin(), prefix.end(), text.begin(), [](char a, char b)
               { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); });
}

// The ids the description gives, in order.
[[nodiscard]] std::vector<std::string> ids_in(std::string_view description)
{
    std::vector<std::string> ids;
    for(auto at{description.find("id=")}; at != std::string_view::npos; at = description.find("id=", at + 3))
    {
        if(at > 0 && description[at - 1] != ' ')
        {
            continue;
        }
        const auto start{at + 3};
        const auto end{std::min(description.find_first_of(" \n\r", start), description.size())};
        if(end > start)
        {
            ids.emplace_back(description.substr(start, end - start));
        }
    }
    return ids;
}

// Property values that are texts to translate.
[[nodiscard]] bool is_translated(std::string_view property)
{
    return property == "placeholder" || property == "text" || property == "tooltip";
}

[[noreturn]] void fail(const DeclarativeNode& node, const std::string& message, std::size_t column = 0)
{
    throw UiError{node.line, column == 0 ? node.column : column, message};
}

// The column a property is written at, for saying where it is wrong: found in what the node was read from would take
// the line again, so a property's place is counted as its name's place after the type and the text.
[[nodiscard]] std::size_t property_column(const DeclarativeNode& node, std::string_view name)
{
    // Where the reader found it; the node's own column for a node made otherwise.
    for(std::size_t index{}; index < node.properties.size() && index < node.property_columns.size(); ++ index)
    {
        if(node.properties[index].first == name)
        {
            return node.property_columns[index];
        }
    }
    return node.column;
}

[[nodiscard]] Spacing spacing_of(const DeclarativeNode& node)
{
    const auto gap{node.property("gap").value_or("none")};
    if(gap == "none")
    {
        return Spacing::none;
    }
    if(gap == "tight")
    {
        return Spacing::tight;
    }
    if(gap == "normal")
    {
        return Spacing::normal;
    }
    if(gap == "loose")
    {
        return Spacing::loose;
    }
    fail(node, format_text(translate("Description", "A gap is none, tight, normal or loose, not {0}."), {gap}),
         property_column(node, "gap"));
}

[[nodiscard]] int number_of(const DeclarativeNode& node, std::string_view property, int otherwise)
{
    const auto text{node.property(property)};
    if(!text)
    {
        return otherwise;
    }
    int value{};
    const auto [end, error]{std::from_chars(text->data(), text->data() + text->size(), value)};
    if(error != std::errc{} || end != text->data() + text->size() || value < 0)
    {
        fail(node, format_text(translate("Description", "The {0} is a whole number, not {1}."), {property, *text}),
             property_column(node, property));
    }
    return value;
}

// A share between 0 and 1, as 0.4: read here rather than with the C library, whose decimal point is the locale's.
[[nodiscard]] std::optional<double> share_of(const DeclarativeNode& node, std::string_view property)
{
    const auto text{node.property(property)};
    if(!text)
    {
        return std::nullopt;
    }
    const auto point{text->find('.')};
    const auto whole{text->substr(0, point)};
    const auto fraction{point == std::string_view::npos ? std::string_view{} : text->substr(point + 1)};
    auto value{whole == "1" ? 1.0 : 0.0};
    auto digit{0.1};
    auto valid{(whole == "0" || whole == "1") && (point == std::string_view::npos || !fraction.empty())};
    for(const auto character : fraction)
    {
        valid = valid && character >= '0' && character <= '9';
        value += (character - '0') * digit;
        digit /= 10;
    }
    if(!valid || value > 1.0)
    {
        fail(node,
             format_text(translate("Description", "The {0} is a share between 0 and 1, as 0.4, not {1}."),
                         {property, *text}),
             property_column(node, property));
    }
    return value;
}

// The columns or the rows of a grid: content, fill, flex or a number of cells each.
[[nodiscard]] std::vector<Length> lengths_of(const DeclarativeNode& node, std::string_view property,
                                             std::string_view each)
{
    std::vector<Length> lengths;
    const auto text{*node.property(property)};
    std::size_t start{};
    while(start <= text.size())
    {
        const auto end{std::min(text.find(',', start), text.size())};
        const auto part{text.substr(start, end - start)};
        int cells{};
        const auto [stop, error]{std::from_chars(part.data(), part.data() + part.size(), cells)};
        if(part == "content")
        {
            lengths.push_back(Length::content());
        }
        else if(part == "fill")
        {
            lengths.push_back(Length::fill());
        }
        else if(part == "flex")
        {
            lengths.push_back(Length::flex());
        }
        else if(error == std::errc{} && stop == part.data() + part.size() && cells >= 0)
        {
            lengths.push_back(Length::cells(cells));
        }
        else
        {
            fail(node, each == "column"
                           ? format_text(translate("Description",
                                                   "A column is content, fill, flex or a number of cells, not {0}."),
                                         {part})
                           : format_text(translate("Description",
                                                   "A row is content, fill, flex or a number of cells, not {0}."),
                                         {part}));
        }
        start = end + 1;
    }
    return lengths;
}

[[nodiscard]] std::optional<ColorRef> color_of(const DeclarativeNode& node, std::string_view property)
{
    const auto name{node.property(property)};
    if(!name)
    {
        return std::nullopt;
    }
    const auto color{theme_color(*name)};
    if(!color)
    {
        fail(node, format_text(translate("Description", "There is no colour {0} in the theme."), {*name}),
             property_column(node, property));
    }
    return ColorRef{*color};
}

// A place in a description the code fills, through Ui::put; it takes no room until then.
class Slot final : public Widget
{
  public:
    [[nodiscard]] Size measure(const Constraints& constraints) const override { return constraints.constrain({}); }
};

// Builds a description's widgets, keeping those with an id and the labels with a buddy to name once all are made.
class Builder
{
  public:
    Builder(std::string context, const DeclarativeNode& root, UiTexts texts)
        : m_context{std::move(context)}, m_root{root}, m_texts{texts}
    {
    }

    [[nodiscard]] std::unique_ptr<Widget> build(const DeclarativeNode& node)
    {
        check(node);
        if(node.type == "Tab")
        {
            fail(node, translate("Description", "A Tab goes in a Tabs."));
        }
        auto widget{make(node)};
        if(node.has("hidden"))
        {
            widget->set_visible(false);
        }
        if(const auto tooltip{node.property("tooltip")})
        {
            widget->set_tooltip(shown(*tooltip));
        }
        name(node, *widget);
        return widget;
    }

    // Each label's buddy, now that every widget is there to be named.
    void name_buddies()
    {
        for(const auto& [label, node] : m_buddies)
        {
            const auto id{*node->property("buddy")};
            const auto found{
                std::find_if(m_named.begin(), m_named.end(), [id](const auto& named) { return named.first == id; })};
            if(found == m_named.end())
            {
                fail(*node, format_text(translate("Description", "No widget has the id {0}."), {id}));
            }
            label->set_buddy(*found->second);
        }
    }

    [[nodiscard]] std::vector<std::pair<std::string, Widget*>> named() { return std::move(m_named); }

  private:
    // That the widget is one there is, with properties it takes.
    void check(const DeclarativeNode& node) const
    {
        const auto kind{
            std::find_if(kinds().begin(), kinds().end(), [&node](const Kind& each) { return each.type == node.type; })};
        if(kind == kinds().end())
        {
            fail(node, format_text(translate("Description", "There is no widget called {0}."), {node.type}));
        }
        for(const auto& [property, value] : node.properties)
        {
            if(property == "context" && &node != &m_root)
            {
                fail(node, translate("Description", "Only the root names a context."), property_column(node, property));
            }
            if(property != "id" && property != "hidden" && property != "tooltip" && property != "context" &&
               !(property == "untranslated" && kind->has_text) &&
               std::find(kind->properties.begin(), kind->properties.end(), property) == kind->properties.end())
            {
                fail(node, format_text(translate("Description", "A {0} has no property {1}."), {node.type, property}),
                     property_column(node, property));
            }
        }
        if(node.text && !kind->has_text)
        {
            fail(node, format_text(translate("Description", "A {0} has no text of its own."), {node.type}));
        }
        // A tab's title is always there, to choose it by.
        if(node.type == "Tab" && node.has("hidden"))
        {
            fail(node, translate("Description", "A Tab cannot be hidden."));
        }
    }

    void name(const DeclarativeNode& node, Widget& widget)
    {
        if(const auto id{node.property("id")})
        {
            if(std::any_of(m_named.begin(), m_named.end(), [&id](const auto& named) { return named.first == *id; }))
            {
                fail(node, format_text(translate("Description", "The id {0} is given twice."), {*id}));
            }
            m_named.emplace_back(std::string{*id}, &widget);
        }
    }

    // A text as it shows: translated, unless the description's texts are as written.
    [[nodiscard]] std::string shown(std::string_view text) const
    {
        return m_texts == UiTexts::as_written ? std::string{text} : translate(m_context, text);
    }

    // The widget's own text, translated unless it says otherwise.
    [[nodiscard]] std::string own_text(const DeclarativeNode& node) const
    {
        const auto text{node.text.value_or("")};
        return node.has("untranslated") ? text : shown(text);
    }

    void add_children(Container& container, const DeclarativeNode& node)
    {
        for(const auto& child : node.children)
        {
            container.add(build(child));
        }
    }

    [[nodiscard]] std::unique_ptr<Widget> make(const DeclarativeNode& node)
    {
        const auto& type{node.type};
        if(type == "Window")
        {
            return make_window(node);
        }
        if(type == "Column")
        {
            auto made{column()};
            made->set_gap(spacing_of(node));
            add_children(*made, node);
            return made;
        }
        if(type == "Row")
        {
            auto made{row()};
            made->set_gap(spacing_of(node));
            made->set_centered(node.has("centered"));
            add_children(*made, node);
            return made;
        }
        if(type == "Grid")
        {
            auto made{grid()};
            made->set_columns(node.has("columns") ? lengths_of(node, "columns", "column")
                                                  : std::vector<Length>{Length::fill()});
            if(node.has("rows"))
            {
                made->set_rows(lengths_of(node, "rows", "row"));
            }
            made->set_gap(spacing_of(node));
            add_children(*made, node);
            return made;
        }
        if(type == "Split")
        {
            auto made{split_view(node.has("vertical") ? SplitView::Orientation::vertical
                                                      : SplitView::Orientation::horizontal)};
            if(node.children.size() > 2)
            {
                fail(node.children[2], translate("Description", "A Split holds two widgets."));
            }
            add_children(*made, node);
            if(const auto ratio{share_of(node, "ratio")})
            {
                made->set_ratio(*ratio);
            }
            return made;
        }
        if(type == "Dock")
        {
            if(!node.children.empty())
            {
                fail(node.children.front(),
                     translate("Description", "A Dock is filled by the code, which says where each widget goes."));
            }
            return dock_layout();
        }
        if(type == "Tabs")
        {
            return make_tabs(node);
        }
        if(node.type == "Slot" && &node == &m_root)
        {
            fail(node, translate("Description", "A Slot goes inside another widget."));
        }
        if(!node.children.empty())
        {
            fail(node.children.front(),
                 format_text(translate("Description", "A {0} holds no other widgets."), {node.type}));
        }
        if(type == "Label")
        {
            // Its & marks the letter Alt takes to its buddy; with none it goes, and && is an & written.
            auto made{label(node.has("buddy") ? own_text(node) : strip_mnemonic(own_text(node)))};
            made->set_wrap(node.has("wrap"));
            if(node.has("heading"))
            {
                made->set_bold(true);
                made->set_color(&Theme::dialog_heading_foreground);
            }
            else if(node.has("bold"))
            {
                made->set_bold(true);
            }
            if(const auto background{color_of(node, "background")})
            {
                made->set_background(*background);
            }
            if(node.has("buddy"))
            {
                m_buddies.emplace_back(made.get(), &node);
            }
            return made;
        }
        if(type == "LineEdit")
        {
            auto made{line_edit()};
            if(const auto text{node.property("text")})
            {
                made->set_text(shown(*text));
            }
            if(const auto placeholder{node.property("placeholder")})
            {
                made->set_placeholder(shown(*placeholder));
            }
            if(const auto background{color_of(node, "background")})
            {
                made->set_background(*background);
            }
            made->set_password(node.has("password"));
            return made;
        }
        if(type == "ProgressBar")
        {
            auto made{progress_bar()};
            if(const auto value{share_of(node, "value")})
            {
                made->set_value(*value);
            }
            if(const auto text{node.property("text")})
            {
                made->set_text(shown(*text));
            }
            return made;
        }
        if(type == "TextEdit")
        {
            auto made{text_edit()};
            made->set_read_only(node.has("read_only"));
            made->set_word_wrap(node.has("wrap"));
            return made;
        }
        if(type == "MarkdownView")
        {
            auto made{markdown_view()};
            if(const auto background{color_of(node, "background")})
            {
                made->set_background(*background);
            }
            if(const auto foreground{color_of(node, "foreground")})
            {
                made->set_foreground(*foreground);
            }
            return made;
        }
        if(type == "CheckButton")
        {
            auto made{check_button(own_text(node))};
            made->set_checked(node.has("checked"));
            if(const auto background{color_of(node, "background")})
            {
                made->set_background(*background);
            }
            return made;
        }
        if(type == "Button")
        {
            auto made{std::make_unique<Button>(own_text(node))};
            made->set_default(node.has("default"));
            made->set_compact(node.has("compact"));
            made->set_shadow_visible(!node.has("flat"));
            return made;
        }
        if(type == "ComboBox")
        {
            return combo_box();
        }
        if(type == "VSpacer")
        {
            return vspacer(number_of(node, "height", 1));
        }
        if(type == "HSpacer")
        {
            return hspacer(number_of(node, "width", 1));
        }
        if(!node.has("id"))
        {
            fail(node, translate("Description", "A Slot needs an id, for the code to fill it."));
        }
        return std::make_unique<Slot>();
    }

    [[nodiscard]] std::unique_ptr<Widget> make_window(const DeclarativeNode& node)
    {
        auto made{window(own_text(node))};
        if(const auto width{number_of(node, "width", 0)}; width > 0)
        {
            made->set_preferred_size({static_cast<double>(width), 0});
        }
        if(node.has("max_width"))
        {
            made->set_maximum_size({static_cast<double>(number_of(node, "max_width", 0)), 1 << 15});
        }
        if(node.has("min_width") || node.has("min_height"))
        {
            made->set_minimum_size({static_cast<double>(number_of(node, "min_width", 0)),
                                    static_cast<double>(number_of(node, "min_height", 0))});
        }
        if(const auto share{share_of(node, "height_share")})
        {
            made->set_height_share(*share);
        }
        made->set_resizable(node.has("resizable"));
        if(const auto border{node.property("border")})
        {
            if(*border == "none")
            {
                made->set_border(WindowBorder::none);
            }
            else if(*border == "single")
            {
                made->set_border(WindowBorder::single);
            }
            else if(*border == "double")
            {
                made->set_border(WindowBorder::double_);
            }
            else
            {
                fail(node,
                     format_text(translate("Description", "A border is none, single or double, not {0}."), {*border}),
                     property_column(node, "border"));
            }
        }
        if(const auto placement{node.property("placement")})
        {
            made->set_placement_key(std::string{*placement});
        }
        made->set_close_on_escape(!node.has("keep_on_escape"));
        // A widget alone fills the window; several go one under the other.
        if(node.children.size() == 1)
        {
            made->add(build(node.children.front()));
        }
        else if(!node.children.empty())
        {
            auto content{column()};
            add_children(*content, node);
            made->add(std::move(content));
        }
        return made;
    }

    [[nodiscard]] std::unique_ptr<Widget> make_tabs(const DeclarativeNode& node)
    {
        auto made{tab_view()};
        made->set_closable(!node.has("fixed"));
        // A tab's tooltip is told by its title, whichever place the code moves the tab to.
        std::vector<std::pair<const Widget*, std::string>> tooltips;
        for(const auto& tab : node.children)
        {
            if(tab.type != "Tab")
            {
                fail(tab, format_text(translate("Description", "A Tabs holds Tab, not {0}."), {tab.type}));
            }
            check(tab);
            if(tab.children.size() > 1)
            {
                fail(tab.children[1], translate("Description", "A Tab holds one widget."));
            }
            // A tab with nothing of its own only chooses, as one over a list the code fills.
            auto& content{tab.children.empty() ? made->add_tab(own_text(tab), vspacer(0))
                                               : made->add_tab(own_text(tab), build(tab.children.front()))};
            name(tab, content);
            if(const auto tooltip{tab.property("tooltip")})
            {
                tooltips.emplace_back(&content, shown(*tooltip));
            }
        }
        if(!tooltips.empty())
        {
            made->set_tooltip_provider(
                [tabs{made.get()}, tooltips](std::size_t index) -> std::optional<std::string>
                {
                    const auto* const content{tabs->content_at<Widget>(index)};
                    const auto found{std::find_if(tooltips.begin(), tooltips.end(),
                                                  [content](const auto& each) { return each.first == content; })};
                    return found == tooltips.end() ? std::nullopt : std::optional{found->second};
                });
        }
        return made;
    }

    std::string m_context;
    const DeclarativeNode& m_root;
    UiTexts m_texts;
    std::vector<std::pair<std::string, Widget*>> m_named;
    std::vector<std::pair<Label*, const DeclarativeNode*>> m_buddies;
};

void collect_texts(const DeclarativeNode& node, const std::string& context, std::vector<UiText>& texts)
{
    if(node.text && !node.has("untranslated"))
    {
        texts.push_back({context, *node.text, node.line, node.column});
    }
    for(const auto& [property, value] : node.properties)
    {
        if(is_translated(property))
        {
            texts.push_back({context, value, node.line, property_column(node, property)});
        }
    }
    for(const auto& child : node.children)
    {
        collect_texts(child, context, texts);
    }
}

} // namespace

UiError::UiError(std::size_t line, std::size_t column, const std::string& message)
    : std::runtime_error{std::to_string(line) + ":" + std::to_string(column) + ": " + message}, m_line{line},
      m_column{column}, m_message{message}
{
}

std::size_t UiError::line() const
{
    return m_line;
}

std::size_t UiError::column() const
{
    return m_column;
}

const std::string& UiError::message() const
{
    return m_message;
}

std::vector<UiText> ui_texts(const DeclarativeNode& root)
{
    std::vector<UiText> texts;
    collect_texts(root, std::string{root.property("context").value_or("")}, texts);
    return texts;
}

Ui::Ui(std::unique_ptr<Widget> root, std::vector<std::pair<std::string, Widget*>> named)
    : m_root{std::move(root)}, m_root_reference{m_root.get()}, m_named{std::move(named)}
{
}

Widget& Ui::root() const
{
    return *m_root_reference;
}

std::unique_ptr<Widget> Ui::take()
{
    return std::move(m_root);
}

std::unique_ptr<Window> Ui::take_window()
{
    if(dynamic_cast<Window*>(m_root.get()) == nullptr)
    {
        return nullptr;
    }
    return std::unique_ptr<Window>{static_cast<Window*>(m_root.release())};
}

std::unique_ptr<Widget> Ui::take(std::string_view id)
{
    auto& widget{named(id)};
    if(widget.parent() == nullptr)
    {
        throw std::logic_error{"The widget " + std::string{id} + " is in nothing to take it out of."};
    }
    return widget.parent()->remove(widget);
}

const std::vector<std::pair<std::string, Widget*>>& Ui::widgets() const
{
    return m_named;
}

Widget& Ui::named(std::string_view id) const
{
    const auto found{
        std::find_if(m_named.begin(), m_named.end(), [id](const auto& named) { return named.first == id; })};
    if(found == m_named.end())
    {
        throw std::logic_error{"No widget has the id " + std::string{id} + "."};
    }
    return *found->second;
}

bool Ui::is_slot(std::string_view id) const
{
    return dynamic_cast<const Slot*>(&named(id)) != nullptr;
}

void Ui::fill(std::string_view id, std::unique_ptr<Widget> widget)
{
    auto& slot{named(id)};
    if(dynamic_cast<Slot*>(&slot) == nullptr)
    {
        throw std::logic_error{"The widget " + std::string{id} + " is not a Slot."};
    }
    auto* const filled{widget.get()};
    static_cast<void>(slot.parent()->replace(slot, std::move(widget)));
    for(auto& [name, each] : m_named)
    {
        if(name == id)
        {
            each = filled;
        }
    }
}

UiSuggestions ui_suggestions(std::string_view description, std::size_t cursor)
{
    UiSuggestions found;
    const auto line_start{cursor == 0 ? 0 : description.rfind('\n', cursor - 1) + 1};
    const auto line{description.substr(line_start, cursor - line_start)};
    const auto indent{std::min(line.find_first_not_of(' '), line.size())};
    const auto rest{line.substr(indent)};
    if(!rest.empty() && rest.front() == '#')
    {
        return found;
    }
    // The widget being written, at the line's start.
    const auto space{rest.find(' ')};
    if(space == std::string_view::npos)
    {
        found.start = line_start + indent;
        for(const auto& kind : kinds())
        {
            if(starts_as(kind.type, rest))
            {
                found.items.push_back(
                    {std::string{kind.type}, "widget", std::string{kind.type} + (kind.has_text ? " \"$0\"" : "")});
            }
        }
        return found;
    }
    const auto type{rest.substr(0, space)};
    const auto kind{
        std::find_if(kinds().begin(), kinds().end(), [type](const Kind& each) { return each.type == type; })};
    if(kind == kinds().end())
    {
        return found;
    }
    // The properties written so far, and where the one at the cursor starts; none inside the text in quotes.
    std::vector<std::string_view> given;
    auto quoted{false};
    std::size_t token{space + 1};
    for(std::size_t index{space}; index < rest.size(); ++ index)
    {
        if(rest[index] == '\\' && quoted)
        {
            ++ index;
        }
        else if(rest[index] == '"')
        {
            quoted = !quoted;
        }
        else if(rest[index] == ' ' && !quoted)
        {
            const auto word{rest.substr(token, index - token)};
            given.push_back(word.substr(0, word.find('=')));
            token = index + 1;
        }
    }
    const auto written{rest.substr(token)};
    if(quoted || (!written.empty() && written.front() == '"'))
    {
        return found;
    }
    if(const auto equals{written.find('=')}; equals != std::string_view::npos)
    {
        // A value.
        const auto* const property{property_named(written.substr(0, equals))};
        auto value{written.substr(equals + 1)};
        auto value_start{line_start + indent + token + equals + 1};
        if(property == nullptr)
        {
            return found;
        }
        std::vector<std::string> values;
        switch(property->value)
        {
        case Value::choice:
            values.assign(property->choices.begin(), property->choices.end());
            break;
        case Value::lengths:
            // The one after the last comma.
            if(const auto comma{value.rfind(',')}; comma != std::string_view::npos)
            {
                value_start += comma + 1;
                value.remove_prefix(comma + 1);
            }
            values = {"content", "fill", "flex"};
            break;
        case Value::colour:
            for(const auto& [name, field] : theme_colors())
            {
                values.emplace_back(name);
            }
            break;
        case Value::id:
            values = ids_in(description);
            break;
        default:
            break;
        }
        found.start = value_start;
        for(const auto& each : values)
        {
            if(starts_as(each, value))
            {
                found.items.push_back({each, property->detail(), each});
            }
        }
        return found;
    }
    // A property the widget takes and has not been given: its own first, then those every widget takes.
    found.start = line_start + indent + token;
    std::vector<std::string_view> takes{kind->properties.begin(), kind->properties.end()};
    takes.insert(takes.end(), {"id", "tooltip"});
    if(kind->type != "Tab")
    {
        takes.emplace_back("hidden");
    }
    if(kind->has_text)
    {
        takes.emplace_back("untranslated");
    }
    if(indent == 0)
    {
        takes.emplace_back("context");
    }
    for(const auto taken : takes)
    {
        const auto& property{*property_named(taken)};
        if(!starts_as(property.name, written) || std::find(given.begin(), given.end(), property.name) != given.end())
        {
            continue;
        }
        const std::string name{property.name};
        found.items.push_back({name, property.detail(),
                               property.value == Value::flag   ? name
                               : property.value == Value::text ? name + "=\"$0\""
                                                               : name + "=$0"});
    }
    return found;
}

Ui build_ui(std::string_view description, UiTexts texts)
{
    const auto parsed{parse_oui(description)};
    if(!parsed.succeeded())
    {
        const auto& first{parsed.diagnostics.front()};
        throw UiError{first.line, first.column, first.message};
    }
    const auto& root{*parsed.root};
    const auto context{root.property("context")};
    if(texts == UiTexts::translated && !context && !ui_texts(root).empty())
    {
        fail(root, translate("Description", "Texts to translate need context=, the place they belong to."));
    }
    Builder builder{std::string{context.value_or("")}, root, texts};
    auto widget{builder.build(root)};
    builder.name_buddies();
    return Ui{std::move(widget), builder.named()};
}

std::optional<Color Theme::*> named_theme_color(std::string_view name)
{
    return theme_color(name);
}

} // namespace olagarro::oui
