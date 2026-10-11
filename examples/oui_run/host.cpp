#include "host.h"

#include <oui/oui.h>
#include <oui/script/script_highlighter.h>
#include <oui/terminal_themes.h>
#include <oui/terminal_view.h>
#ifdef OLAGARRO_OUI_RUN_WINDOW
#include <oui/graphics/graphical_painter.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>

using namespace olagarro;

namespace
{

// The places of `target` among `nodes` and under them, from the top: [1, 0] the first under the second.
bool path_to(const std::list<oui::TreeNode>& nodes, const oui::TreeNode& target, std::vector<std::size_t>& path)
{
    std::size_t index{};
    for(const auto& node : nodes)
    {
        path.push_back(index++);
        if(&node == &target || path_to(node.children, target, path))
        {
            return true;
        }
        path.pop_back();
    }
    return false;
}

// The node at `path`, if there is one.
const oui::TreeNode* node_at(const std::list<oui::TreeNode>& nodes, const std::vector<std::size_t>& path,
                             std::size_t depth = 0)
{
    if(depth >= path.size() || path[depth] >= nodes.size())
    {
        return nullptr;
    }
    const auto& node{*std::next(nodes.begin(), static_cast<std::ptrdiff_t>(path[depth]))};
    return depth + 1 == path.size() ? &node : node_at(node.children, path, depth + 1);
}

// A length as a script writes one: a number of cells, "content", "fill", "flex", or "40%".
oui::Length length_of(const oui::script::Value& value)
{
    if(value.is_number())
    {
        return oui::Length::cells(static_cast<int>(value.to_number()));
    }
    const auto text{value.to_string()};
    if(text == "content")
    {
        return oui::Length::content();
    }
    if(text == "fill")
    {
        return oui::Length::fill();
    }
    if(text == "flex")
    {
        return oui::Length::flex();
    }
    if(!text.empty() && text.back() == '%')
    {
        return oui::Length::percent(static_cast<float>(std::atof(text.c_str())));
    }
    throw oui::script::ScriptError{"a length is a number of cells, \"content\", \"fill\", \"flex\" or \"40%\", not " +
                                   text};
}

// A key as a script reads it, in English whatever the language: "Ctrl+S", "Enter", "Shift+F3", "A".
std::string key_name(const oui::Event& event)
{
    static const oui::ShortcutNames english{
        "Ctrl+", "Alt+", "Shift+", {"Tab",  "Shift+Tab", "Enter", "Space", "Backspace", "Del", "Esc",
                                    "Left", "Right",     "Up",    "Down",  "Home",      "End", "PgUp",
                                    "PgDn", "Ins",       "F1",    "F2",    "F3",        "F4",  "F5",
                                    "F6",   "F7",        "F8",    "F9",    "F10",       "F11", "F12"}};
    if(const auto* const key{std::get_if<oui::KeyEvent>(&event)})
    {
        return oui::format_shortcut({{}, key->control, key->alt, key->shift, key->key}, english);
    }
    if(const auto* const text{std::get_if<oui::TextEvent>(&event)})
    {
        return oui::format_shortcut({text->codepoint, text->control, text->alt, text->shift}, english);
    }
    return {};
}

// A part of the window: what it holds, the whole of its room, and nothing of its own drawn.
class Part final : public oui::Container
{
  public:
    [[nodiscard]] oui::Size measure(const oui::Constraints& constraints) const override
    {
        return child_count() == 0 ? oui::Size{} : child_at(0).measure(constraints);
    }

    void arrange(const oui::Rect& bounds) override
    {
        Widget::arrange(bounds);
        if(child_count() != 0)
        {
            child_at(0).arrange(bounds);
        }
    }
};

// What a script draws on, in cells: what it drew stays, painted again whenever the canvas is, until it is cleared.
// Of its own size when given one, of all the room it has when not.
class Canvas final : public oui::Widget
{
  public:
    struct Stroke
    {
        oui::Rect area;
        // Text written from the area's corner; with none the area is filled.
        std::string text;
        std::optional<oui::ColorRef> foreground;
        std::optional<oui::ColorRef> background;
        std::uint8_t attributes{};
    };

    void set_size(oui::Size size) { m_size = size; }

    void clear(std::optional<oui::ColorRef> background)
    {
        m_strokes.clear();
        m_background = background;
    }

    void add(Stroke stroke) { m_strokes.push_back(std::move(stroke)); }

    // Where the pointer is, from the canvas's corner, with what it did.
    void on_mouse(std::function<void(const oui::MouseEvent&, int x, int y)> handler)
    {
        m_on_mouse = std::move(handler);
    }

    // Its new width and height, each time they change.
    void on_resize(std::function<void(oui::Size)> handler) { m_on_resize = std::move(handler); }

    [[nodiscard]] oui::Size measure(const oui::Constraints& constraints) const override
    {
        return constraints.constrain({m_size.width > 0 ? m_size.width : constraints.max_width,
                                      m_size.height > 0 ? m_size.height : constraints.max_height});
    }

    void arrange(const oui::Rect& bounds) override
    {
        const auto before{this->bounds()};
        Widget::arrange(bounds);
        if((before.width != bounds.width || before.height != bounds.height) && m_on_resize)
        {
            m_on_resize({bounds.width, bounds.height});
        }
    }

    [[nodiscard]] bool is_focusable() const override { return true; }

    bool handle_event(const oui::Event& event) override
    {
        const auto* const mouse{std::get_if<oui::MouseEvent>(&event)};
        const auto& area{bounds()};
        if(mouse == nullptr || !m_on_mouse || mouse->x < area.x || mouse->y < area.y ||
           mouse->x >= area.x + area.width || mouse->y >= area.y + area.height)
        {
            return false;
        }
        m_on_mouse(*mouse, mouse->x - area.x, mouse->y - area.y);
        return true;
    }

    void paint(oui::Painter& painter, const oui::Theme& theme) const override
    {
        const auto& area{bounds()};
        const auto background{oui::resolve(m_background, theme, theme.background)};
        painter.push_clip(area);
        painter.fill_rect(area, background);
        for(const auto& stroke : m_strokes)
        {
            const oui::Rect at{area.x + stroke.area.x, area.y + stroke.area.y, stroke.area.width, stroke.area.height};
            if(stroke.text.empty())
            {
                painter.fill_rect(at, oui::resolve(stroke.background, theme, background));
                continue;
            }
            painter.draw_text({at.x, at.y}, stroke.text,
                              {oui::resolve(stroke.foreground, theme, theme.text),
                               oui::resolve(stroke.background, theme, background), stroke.attributes});
        }
        painter.pop_clip();
    }

  private:
    oui::Size m_size;
    std::optional<oui::ColorRef> m_background;
    std::vector<Stroke> m_strokes;
    std::function<void(const oui::MouseEvent&, int, int)> m_on_mouse;
    std::function<void(oui::Size)> m_on_resize;
};

} // namespace

// Defined in the source CMake generates from examples/oui_run/prelude.js.
std::vector<std::string_view> embedded_oui_run_prelude();

namespace
{

using oui::script::ScriptEngine;
using oui::script::ScriptError;
using oui::script::ScriptStop;
using oui::script::SystemApi;
using oui::script::Value;
using Arguments = std::vector<Value>;

[[nodiscard]] Value argument(const Arguments& arguments, std::size_t index)
{
    return index < arguments.size() ? arguments[index] : Value{};
}
[[nodiscard]] int id_of(const Arguments& arguments, std::size_t index)
{
    return static_cast<int>(argument(arguments, index).to_number());
}
[[nodiscard]] std::string text_of(const Value& value)
{
    return value.is_undefined() || value.is_null() ? std::string{} : value.to_string();
}

// A place in UTF-8 text, given in bytes, as JavaScript counts places in a string: in UTF-16 units.
[[nodiscard]] double js_index_of(std::string_view text, std::size_t byte)
{
    double units{};
    for(std::size_t index{}; index < byte && index < text.size();)
    {
        units += oui::decode_utf8(text, index) >= 0x10000 ? 2 : 1;
    }
    return units;
}

// A place JavaScript gives in UTF-16 units, as a byte of the UTF-8 text; past the end is the end.
[[nodiscard]] std::size_t byte_of(std::string_view text, const Value& js_index)
{
    const auto wanted{std::max(0.0, js_index.to_number())};
    std::size_t index{};
    for(double units{}; index < text.size() && units < wanted;)
    {
        units += oui::decode_utf8(text, index) >= 0x10000 ? 2 : 1;
    }
    return index;
}

// How to look for text, as a script says it: {matchCase, wholeWord, regex}, matching case unless told not to.
[[nodiscard]] oui::SearchOptions search_options_of(const Value& options)
{
    oui::SearchOptions search;
    if(options.is_object())
    {
        search.match_case = options.get("matchCase").is_undefined() || options.get("matchCase").to_bool();
        search.whole_word = options.get("wholeWord").to_bool();
        search.regex = options.get("regex").to_bool();
    }
    return search;
}

// A colour as a script names it: one of the theme's, "table_background", or a fixed one, "#20a0ff".
[[nodiscard]] oui::ColorRef color_of(const Value& value)
{
    const auto name{text_of(value)};
    if(name.size() == 7 && name.front() == '#' &&
       name.find_first_not_of("0123456789abcdefABCDEF", 1) == std::string::npos)
    {
        const auto part{[&name](std::size_t at)
                        { return static_cast<std::uint8_t>(std::stoi(name.substr(at, 2), nullptr, 16)); }};
        return oui::Color{part(1), part(3), part(5)};
    }
    if(const auto field{oui::named_theme_color(name)})
    {
        return *field;
    }
    throw ScriptError{"there is no colour " + name + " in the theme, nor is it one as \"#20a0ff\""};
}

// A key of the script's settings, as the file it is kept in can hold it: a line before an =.
[[nodiscard]] std::string setting_key(const Value& value)
{
    const auto key{text_of(value)};
    if(key.empty() || key.find_first_of("=\r\n") != std::string::npos || key.front() == '#' || key.front() == ' ' ||
       key.back() == ' ' || key.front() == '\t' || key.back() == '\t')
    {
        throw ScriptError{
            "\"" + key + "\" cannot be a setting's name: one with no =, no line break, and no blank or # at its start"};
    }
    return key;
}

// What a widget is, as prelude.js knows the kinds: the one it makes the widget's object of.
[[nodiscard]] std::string kind_of(const oui::Widget& widget)
{
    if(dynamic_cast<const oui::Label*>(&widget) != nullptr)
    {
        return "Label";
    }
    if(dynamic_cast<const oui::Button*>(&widget) != nullptr)
    {
        return "Button";
    }
    if(dynamic_cast<const oui::CheckButton*>(&widget) != nullptr)
    {
        return "CheckButton";
    }
    if(dynamic_cast<const oui::LineEdit*>(&widget) != nullptr)
    {
        return "LineEdit";
    }
    if(dynamic_cast<const oui::TextEdit*>(&widget) != nullptr)
    {
        return "TextEdit";
    }
    if(dynamic_cast<const oui::ComboBox*>(&widget) != nullptr)
    {
        return "ComboBox";
    }
    if(dynamic_cast<const oui::Window*>(&widget) != nullptr)
    {
        return "Window";
    }
    if(dynamic_cast<const oui::TabView*>(&widget) != nullptr)
    {
        return "Tabs";
    }
    if(dynamic_cast<const oui::SplitView*>(&widget) != nullptr)
    {
        return "Split";
    }
    if(dynamic_cast<const oui::MarkdownView*>(&widget) != nullptr)
    {
        return "MarkdownView";
    }
    if(dynamic_cast<const oui::Grid*>(&widget) != nullptr)
    {
        return "Grid";
    }
    if(dynamic_cast<const oui::Column*>(&widget) != nullptr)
    {
        return "Column";
    }
    if(dynamic_cast<const oui::Row*>(&widget) != nullptr)
    {
        return "Row";
    }
    return "Widget";
}

} // namespace

void write_to(std::FILE* stream, std::string_view text)
{
    std::fwrite(text.data(), 1, text.size(), stream);
    std::fflush(stream);
}

Host::Host(oui::Application& application, std::vector<std::string> arguments, bool desktop_window)
    : m_application{application}, m_arguments{std::move(arguments)}
{
#ifdef OLAGARRO_OUI_RUN_WINDOW
    m_desktop_window = desktop_window;
#else
    static_cast<void>(desktop_window);
#endif
    // A script is a program: it runs as long as it needs, not against the clock that guards callbacks.
    m_engine.set_time_limit(std::chrono::hours{24 * 365});
    m_engine.set_print([this](std::string_view text) { print(std::string{text} + "\n"); });
    m_engine.set_unhandled_rejection([this](std::string_view error) { report(error); });
    m_system = std::make_unique<SystemApi>(m_engine, m_application,
                                           SystemApi::Hooks{[this](std::string_view text, bool error)
                                                            { print_output(text, error); },
                                                            [this](std::string_view error) { report(error); },
                                                            [this] { return m_window != nullptr; }, "oui_run"});
    define_everything();
    m_engine.run(embedded_oui_run_prelude().front(), "prelude.js");
    // A right click on a widget with a menu of its own opens it.
    m_application.set_event_observer([this](const oui::Event& event) { open_context_menu(event); });
}

void Host::run(const std::filesystem::path& path)
{
    m_script_folder = oui::whole_path(path).parent_path();
    m_script_name = oui::utf8_of(path.stem());
    const auto source{oui::read_file(path)};
    if(!source)
    {
        report("cannot read " + oui::utf8_of(path));
        return;
    }
    try
    {
        m_engine.run_module(*source, oui::whole_path(path));
    }
    catch(const ScriptStop&)
    {
        // Ended by itself, on purpose: nothing to tell.
    }
    catch(const ScriptError& error)
    {
        report(error.what());
    }
}

bool Host::keeps_running() const
{
    return !m_quit && (m_window != nullptr || m_system->pending() > 0);
}

int Host::exit_code() const
{
    return m_exit_code;
}

std::string Host::take_held_output()
{
    return std::exchange(m_held_output, {});
}

bool Host::holds_terminal() const
{
    return m_window != nullptr && !m_in_desktop_window;
}

void Host::print(const std::string& text)
{
    if(holds_terminal())
    {
        m_held_output += text;
        return;
    }
    write_to(stdout, text);
}

void Host::print_output(std::string_view text, bool error)
{
    if(holds_terminal())
    {
        m_held_output += text;
        return;
    }
    write_to(error ? stderr : stdout, text);
}

void Host::report(std::string_view error)
{
    m_exit_code = 1;
    const auto text{"oui_run: " + std::string{error} + "\n"};
    if(m_window == nullptr)
    {
        write_to(stderr, text);
        return;
    }
    if(holds_terminal())
    {
        m_held_output += text;
    }
    else
    {
        write_to(stderr, text);
    }
    const std::string message{error};
    m_application.post([this, message] { static_cast<void>(oui::message_box(m_application, "oui_run", message)); });
}

void Host::call(const Value& function, const Arguments& arguments)
{
    try
    {
        static_cast<void>(function.call(arguments));
    }
    catch(const ScriptStop&)
    {
        // Ended by itself, on purpose: nothing to tell.
    }
    catch(const ScriptError& error)
    {
        report(error.what());
    }
}

template <typename Kind> int Host::adopt(std::unique_ptr<Kind> widget)
{
    const auto id{++ m_next_id};
    m_widgets[id] = widget.get();
    m_loose[id] = std::move(widget);
    return id;
}

template <typename Kind> Kind& Host::widget(int id, std::string_view what)
{
    const auto found{m_widgets.find(id)};
    auto* const result{found == m_widgets.end() ? nullptr : dynamic_cast<Kind*>(found->second)};
    if(result == nullptr)
    {
        throw ScriptError{"not a " + std::string{what}};
    }
    return *result;
}

std::unique_ptr<oui::Widget> Host::take(int id)
{
    const auto found{m_loose.find(id)};
    if(found == m_loose.end())
    {
        throw ScriptError{m_widgets.count(id) != 0 ? "that widget is in place already" : "not a widget"};
    }
    auto taken{std::move(found->second)};
    m_loose.erase(found);
    return taken;
}

oui::Settings& Host::settings()
{
    if(m_settings == nullptr)
    {
        // Each script's own file, beside the others' in oui_run's folder.
        const oui::Settings folder{oui::SettingsLocation::app_data, "oui_run"};
        m_settings = std::make_unique<oui::Settings>(folder.file_path().parent_path() /
                                                     oui::path_from_utf8(m_script_name + ".ini"));
        static_cast<void>(m_settings->load());
    }
    return *m_settings;
}

void Host::on(int id, const std::string& event, const Value& function)
{
    if(!function.is_function())
    {
        throw ScriptError{"on " + event + ": not a function"};
    }
    const auto key{std::make_pair(id, event)};
    const auto first{m_handlers.count(key) == 0};
    m_handlers[key] = function;
    if(!first)
    {
        return;
    }
    const auto handler{[this, key](const Arguments& arguments)
                       {
                           const auto found{m_handlers.find(key)};
                           if(found != m_handlers.end())
                           {
                               call(found->second, arguments);
                           }
                       }};
    if(event == "click")
    {
        widget<oui::Button>(id, "button").on_clicked([handler] { handler({}); });
    }
    else if(event == "link")
    {
        widget<oui::MarkdownView>(id, "Markdown view")
            .on_link([this, handler](const std::string& target) { handler({m_engine.string(target)}); });
    }
    else if(event == "cursor")
    {
        widget<oui::TextEdit>(id, "text edit").on_cursor_moved([handler] { handler({}); });
    }
    else if(event == "close")
    {
        widget<oui::TabView>(id, "tabs")
            .on_close_requested([this, handler](std::size_t index)
                                { handler({m_engine.number(static_cast<double>(index))}); });
    }
    else if(event == "resize" && dynamic_cast<Canvas*>(m_widgets[id]) != nullptr)
    {
        // After the layout, which a script's widgets must not change under it.
        widget<Canvas>(id, "canvas")
            .on_resize(
                [this, handler](oui::Size size)
                {
                    m_application.post([this, handler, size]
                                       { handler({m_engine.number(size.width), m_engine.number(size.height)}); });
                });
    }
    else if(event == "resize")
    {
        widget<oui::Table>(id, "table")
            .on_column_resized([this, handler](std::size_t column, int width)
                               { handler({m_engine.number(static_cast<double>(column)), m_engine.number(width)}); });
    }
    else if(event == "mouse")
    {
        widget<Canvas>(id, "canvas")
            .on_mouse(
                [this, handler](const oui::MouseEvent& mouse, int x, int y)
                {
                    const auto* const kind{mouse.wheel_delta != 0 ? "wheel"
                                           : mouse.released       ? "release"
                                           : mouse.dragging       ? "drag"
                                           : mouse.pressed        ? "press"
                                                                  : "move"};
                    handler({m_engine.number(x), m_engine.number(y), m_engine.string(kind),
                             m_engine.boolean(mouse.right), m_engine.number(mouse.clicks),
                             m_engine.number(mouse.wheel_delta), m_engine.boolean(mouse.shift),
                             m_engine.boolean(mouse.control), m_engine.boolean(mouse.alt)});
                });
    }
    else if(event == "change" && dynamic_cast<oui::LineEdit*>(m_widgets[id]) != nullptr)
    {
        widget<oui::LineEdit>(id, "field")
            .on_changed([this, handler](std::string_view text) { handler({m_engine.string(text)}); });
    }
    else if(event == "change")
    {
        widget<oui::TextEdit>(id, "text edit").on_changed([handler] { handler({}); });
    }
    else if(event == "submit")
    {
        widget<oui::LineEdit>(id, "field")
            .on_submitted([this, handler](std::string_view text) { handler({m_engine.string(text)}); });
    }
    else if(event == "toggle" && dynamic_cast<oui::CheckList*>(m_widgets[id]) != nullptr)
    {
        widget<oui::CheckList>(id, "list")
            .on_toggled([this, handler](std::size_t row, bool checked)
                        { handler({m_engine.number(static_cast<double>(row)), m_engine.boolean(checked)}); });
    }
    else if(event == "select" && dynamic_cast<oui::TreeView*>(m_widgets[id]) != nullptr)
    {
        auto& tree{widget<oui::TreeView>(id, "tree")};
        tree.on_selection_changed([this, handler, &tree] { handler({tree_path(tree)}); });
    }
    else if(event == "activate" && dynamic_cast<oui::TreeView*>(m_widgets[id]) != nullptr)
    {
        auto& tree{widget<oui::TreeView>(id, "tree")};
        tree.on_activated([this, handler, &tree](std::size_t) { handler({tree_path(tree)}); });
    }
    else if(event == "activate" && dynamic_cast<oui::FolderView*>(m_widgets[id]) != nullptr)
    {
        widget<oui::FolderView>(id, "folder view")
            .on_activated([this, handler](const std::filesystem::path& path)
                          { handler({m_engine.string(oui::utf8_of(path))}); });
    }
    else if(event == "finished")
    {
        widget<oui::TerminalView>(id, "terminal")
            .on_finished([this, handler](int code) { handler({m_engine.number(static_cast<double>(code))}); });
    }
    else if(event == "title")
    {
        widget<oui::TerminalView>(id, "terminal")
            .on_title_changed([this, handler](const std::string& title) { handler({m_engine.string(title)}); });
    }
    else if(event == "exit")
    {
        widget<oui::CalculatorView>(id, "calculator").on_exit([handler] { handler({}); });
    }
    else if(event == "toggle")
    {
        widget<oui::CheckButton>(id, "check button")
            .on_toggled([this, handler](bool checked) { handler({m_engine.boolean(checked)}); });
    }
    else if(event == "select" && dynamic_cast<oui::ComboBox*>(m_widgets[id]) != nullptr)
    {
        widget<oui::ComboBox>(id, "combo box")
            .on_selected([this, handler](std::size_t index)
                         { handler({m_engine.number(static_cast<double>(index))}); });
    }
    else if(event == "select" && dynamic_cast<oui::TabView*>(m_widgets[id]) != nullptr)
    {
        widget<oui::TabView>(id, "tabs")
            .on_selection_changed([this, handler](std::size_t index)
                                  { handler({m_engine.number(static_cast<double>(index))}); });
    }
    else if(event == "select")
    {
        widget<oui::Table>(id, "list")
            .on_selection_changed([this, handler](std::size_t row)
                                  { handler({m_engine.number(static_cast<double>(row))}); });
    }
    else if(event == "activate")
    {
        widget<oui::Table>(id, "list")
            .on_row_activated([this, handler](std::size_t row)
                              { handler({m_engine.number(static_cast<double>(row))}); });
    }
    else if(event == "sort")
    {
        widget<oui::Table>(id, "table")
            .on_column_click(
                [this, handler](std::size_t column, oui::SortOrder order)
                {
                    handler({m_engine.number(static_cast<double>(column)),
                             m_engine.string(order == oui::SortOrder::ascending    ? "ascending"
                                             : order == oui::SortOrder::descending ? "descending"
                                                                                   : "none")});
                });
    }
    else
    {
        throw ScriptError{"no event " + event};
    }
}

void Host::open_window(const std::string& title)
{
    if(m_window != nullptr)
    {
        throw ScriptError{"oui.window: the window is open already"};
    }
#ifdef OLAGARRO_OUI_RUN_WINDOW
    if(m_desktop_window)
    {
        oui::WindowOptions options;
        options.title = title;
        // The colours JavaScript is known by.
        options.icon = oui::text_icon("JS", {240, 219, 79}, {0, 0, 0});
        auto desktop{oui::window_backend(options)};
        m_screen = desktop.get();
        m_application.set_backend(std::move(desktop));
        if(!m_theme_chosen)
        {
            m_application.set_theme(oui::classic_theme());
        }
        m_in_desktop_window = true;
    }
    else
#endif
    {
        m_application.set_backend(oui::terminal_backend());
        // The terminal look OUI Code has unless told otherwise.
        if(!m_theme_chosen)
        {
            m_application.set_theme(oui::TurboCTheme::make());
        }
    }
    auto window{oui::window(title)};
    window->set_border(oui::WindowBorder::none);
    window->set_close_on_escape(false);
    m_window = window.get();
    // Its parts, each in place from the start, so whatever comes first takes its own: the menu bar on top, the
    // status bar at the bottom, and what the script shows in the room left between them.
    auto frame{std::make_unique<oui::DockLayout>()};
    m_top = &frame->add(oui::Dock::top, std::make_unique<Part>());
    m_bottom = &frame->add(oui::Dock::bottom, std::make_unique<Part>());
    m_middle = &frame->add(oui::Dock::fill, std::make_unique<Part>());
    m_window->add(std::move(frame));
    place_bars();
    m_application.set_window(std::move(window));
    m_application.on_close_requested([this] { quit(); });
    // A terminal that goes away, or a system that shuts down, ends the script with it.
    m_application.on_shutdown([this](oui::ShutdownEvent::Reason) { quit(); });
    m_application.commands().register_command("oui_run.quit", oui::translate("Runner", "Quit"), [this] { quit(); });
    m_application.commands().bind("oui_run.quit", {U'q', true});
}

void Host::set_window_content(int id)
{
    if(m_window == nullptr)
    {
        throw ScriptError{"oui.window: open the window first"};
    }
    auto& content{m_middle->add(take(id))};
    fit_window();
    // The focus starts on the first thing that takes it.
    std::vector<oui::Widget*> focusable;
    content.collect_focusable(focusable);
    if(!focusable.empty())
    {
        m_application.focus(*focusable.front());
    }
}

Host::Value Host::tree_path(const oui::TreeView& tree)
{
    std::vector<std::size_t> path;
    const auto* const chosen{tree.selected()};
    if(chosen == nullptr || !path_to(tree.nodes(), *chosen, path))
    {
        return m_engine.null();
    }
    std::vector<Value> places;
    for(const auto place : path)
    {
        places.push_back(m_engine.number(static_cast<double>(place)));
    }
    return m_engine.array(places);
}

oui::MenuBar& Host::menu_bar()
{
    if(m_menu_bar == nullptr)
    {
        auto bar{oui::menu_bar(m_application.commands())};
        m_menu_bar = bar.get();
        m_pending_menu = std::move(bar);
        place_bars();
    }
    return *m_menu_bar;
}

std::vector<oui::MenuItem> Host::menu_items(const Value& entries)
{
    std::vector<oui::MenuItem> items;
    for(std::size_t entry{}; entries.is_array() && entry < entries.length(); ++ entry)
    {
        const auto item{entries.at(entry)};
        if(item.is_string())
        {
            items.push_back(oui::menu_separator());
            continue;
        }
        const auto run{item.get("run")};
        const auto keys{item.get("keys")};
        const auto shortcut{keys.is_string() ? oui::parse_shortcut(keys.to_string()) : std::nullopt};
        if(!run.is_function() || (keys.is_string() && !shortcut))
        {
            throw ScriptError{"a menu's entry is {title, keys?, run}, keys as \"Ctrl+S\", or \"-\" for a line"};
        }
        const auto id{"oui_run.menu." + std::to_string(++ m_next_id)};
        m_application.commands().register_command(id, text_of(item.get("title")), [this, run] { call(run); });
        if(shortcut)
        {
            m_application.commands().bind(id, *shortcut);
        }
        items.emplace_back(id);
    }
    return items;
}

void Host::open_context_menu(const oui::Event& event)
{
    const auto* const mouse{std::get_if<oui::MouseEvent>(&event)};
    if(mouse == nullptr || !mouse->pressed || !mouse->right || m_window == nullptr || m_context_menus.empty())
    {
        return;
    }
    // The nearest widget with a menu, from the one under the pointer out.
    for(const auto* widget{m_window->hit_test(mouse->x, mouse->y)}; widget != nullptr; widget = widget->parent())
    {
        for(const auto& [id, items] : m_context_menus)
        {
            if(m_widgets.count(id) != 0 && m_widgets[id] == widget)
            {
                // Once what the click does to the widget is done, as choosing the row it falls on.
                m_application.post([this, items{items}, at{oui::Point{mouse->x, mouse->y}}]
                                   { menu_bar().open_context_menu(items, at); });
                return;
            }
        }
    }
}

void Host::place_bars()
{
    if(m_window == nullptr)
    {
        return;
    }
    if(m_pending_menu)
    {
        m_top->add(std::move(m_pending_menu));
    }
    if(m_pending_status)
    {
        m_bottom->add(std::move(m_pending_status));
    }
    fit_window();
}

void Host::fit_window()
{
#ifdef OLAGARRO_OUI_RUN_WINDOW
    if(m_screen == nullptr || m_middle == nullptr || m_middle->child_count() == 0)
    {
        return;
    }
    // As wide as a described window says, or as a terminal is; as tall as what it shows is at that width, with room
    // for a list to show some rows, and not a whole screen of them. A row that centres what it holds, or a bar,
    // asks for the whole width it is given, so the width cannot come from what is measured.
    constexpr oui::Size least{30, 10};
    constexpr oui::Size most{120, 40};
    const auto width{std::clamp(m_described_size.width > 0 ? m_described_size.width : 80, least.width, most.width)};
    // Each part on its own: the frame round them takes whatever it is offered.
    auto height{0};
    for(const auto* const part : {m_top, m_middle, m_bottom})
    {
        height += part->measure({width, most.height}).height;
    }
    m_screen->fit_content({width, std::clamp(std::max(height, m_described_size.height), least.height, most.height)});
#endif
}

void Host::open_described_window(int id)
{
    auto taken{take(id)};
    auto* const window{dynamic_cast<oui::Window*>(taken.get())};
    if(window == nullptr)
    {
        throw ScriptError{"oui.window: not a described Window"};
    }
    m_widgets.erase(id);
    m_described_size = window->preferred_size();
    open_window(std::string{window->title()});
    if(window->child_count() != 0)
    {
        set_window_content(adopt(window->remove_at(0)));
    }
}

Value Host::describe(std::string_view description, const std::string& source)
{
    std::optional<oui::Ui> built;
    try
    {
        built.emplace(oui::build_ui(description, oui::UiTexts::as_written));
    }
    catch(const oui::UiError& error)
    {
        throw ScriptError{source + error.what()};
    }
    Described described{std::move(*built), {}};
    // The description's number, its root's and what the root is, then each widget with an id: its id, its number
    // and what it is.
    const auto root_kind{kind_of(described.ui.root())};
    const auto root_id{adopt(described.ui.take())};
    std::vector<Value> parts{m_engine.number(root_id), m_engine.string(root_kind)};
    for(const auto& [name, widget] : described.ui.widgets())
    {
        const auto id{++ m_next_id};
        m_widgets[id] = widget;
        described.ids[name] = id;
        parts.push_back(
            m_engine.array({m_engine.string(name), m_engine.number(id), m_engine.string(kind_of(*widget))}));
    }
    const auto described_id{++ m_next_id};
    m_described.emplace(described_id, std::move(described));
    parts.insert(parts.begin(), m_engine.number(described_id));
    return m_engine.array(parts);
}

void Host::quit()
{
    m_quit = true;
    m_application.quit();
}

void Host::define(std::string_view name, ScriptEngine::Function function)
{
    m_engine.define("__oui." + std::string{name}, std::move(function));
}

void Host::define_everything()
{
    using oui::Length;
    define("args",
           [this](const Arguments&)
           {
               std::vector<Value> values;
               for(const auto& text : m_arguments)
               {
                   values.push_back(m_engine.string(text));
               }
               return m_engine.array(values);
           });
    define("label", [this](const Arguments& a) { return m_engine.number(adopt(oui::label(text_of(argument(a, 0))))); });
    define("button",
           [this](const Arguments& a) { return m_engine.number(adopt(oui::button(text_of(argument(a, 0))))); });
    define("setDefault",
           [this](const Arguments& a)
           {
               widget<oui::Button>(id_of(a, 0), "button").set_default(argument(a, 1).to_bool());
               return Value{};
           });
    define("lineEdit", [this](const Arguments&) { return m_engine.number(adopt(oui::line_edit())); });
    define("textEdit", [this](const Arguments&) { return m_engine.number(adopt(oui::text_edit())); });
    define("list", [this](const Arguments&) { return m_engine.number(adopt(oui::list())); });
    define("checkList", [this](const Arguments&) { return m_engine.number(adopt(oui::check_list())); });
    define("getChecks",
           [this](const Arguments& a)
           {
               const auto& list{widget<oui::CheckList>(id_of(a, 0), "list with boxes")};
               std::vector<Value> checks;
               for(std::size_t row{}; row < list.row_count(); ++ row)
               {
                   checks.push_back(m_engine.boolean(list.is_checked(row)));
               }
               return m_engine.array(checks);
           });
    define("setChecks",
           [this](const Arguments& a)
           {
               auto& list{widget<oui::CheckList>(id_of(a, 0), "list with boxes")};
               const auto checks{argument(a, 1)};
               for(std::size_t row{}; row < list.row_count(); ++ row)
               {
                   list.set_checked(row, row < checks.length() && checks.at(row).to_bool());
               }
               return Value{};
           });
    define("tree", [this](const Arguments&) { return m_engine.number(adopt(oui::tree_view())); });
    // Nodes as {text, children, expanded}, the children as nodes too.
    define("setTreeItems",
           [this](const Arguments& a)
           {
               auto& tree{widget<oui::TreeView>(id_of(a, 0), "tree")};
               tree.clear();
               std::function<void(oui::TreeNode&, const Value&)> fill{
                   [&fill](oui::TreeNode& node, const Value& item)
                   {
                       node.expanded = item.get("expanded").to_bool();
                       const auto children{item.get("children")};
                       for(std::size_t index{}; children.is_array() && index < children.length(); ++ index)
                       {
                           fill(node.add(text_of(children.at(index).get("text"))), children.at(index));
                       }
                   }};
               const auto items{argument(a, 1)};
               for(std::size_t index{}; index < items.length(); ++ index)
               {
                   fill(tree.add(text_of(items.at(index).get("text"))), items.at(index));
               }
               return Value{};
           });
    define("getTreePath", [this](const Arguments& a) { return tree_path(widget<oui::TreeView>(id_of(a, 0), "tree")); });
    define("setTreePath",
           [this](const Arguments& a)
           {
               auto& tree{widget<oui::TreeView>(id_of(a, 0), "tree")};
               const auto places{argument(a, 1)};
               std::vector<std::size_t> path;
               for(std::size_t index{}; places.is_array() && index < places.length(); ++ index)
               {
                   path.push_back(static_cast<std::size_t>(places.at(index).to_number()));
               }
               if(const auto* const node{node_at(tree.nodes(), path)})
               {
                   tree.select(*node);
               }
               return Value{};
           });
    define("getVisible", [this](const Arguments& a)
           { return m_engine.boolean(widget<oui::Widget>(id_of(a, 0), "widget").is_visible()); });
    define("setVisible",
           [this](const Arguments& a)
           {
               widget<oui::Widget>(id_of(a, 0), "widget").set_visible(argument(a, 1).to_bool());
               return Value{};
           });
    define("getEnabled", [this](const Arguments& a)
           { return m_engine.boolean(widget<oui::Widget>(id_of(a, 0), "widget").is_enabled()); });
    define("setEnabled",
           [this](const Arguments& a)
           {
               widget<oui::Widget>(id_of(a, 0), "widget").set_enabled(argument(a, 1).to_bool());
               return Value{};
           });
    // The menus, each a title and its entries: an entry a title, its keys and what it runs, or "-" for a line.
    define("menuBar",
           [this](const Arguments& a)
           {
               auto& bar{menu_bar()};
               // Asked again, the new menus take the old ones' place.
               bar.clear();
               const auto menus{argument(a, 0)};
               for(std::size_t index{}; menus.is_array() && index < menus.length(); ++ index)
               {
                   const auto menu{menus.at(index)};
                   bar.add_menu(text_of(menu.get("title")), menu_items(menu.get("items")));
               }
               fit_window();
               return Value{};
           });
    // A grid of the columns and rows given, as a description's Grid: its widgets fill it a row at a time.
    define("grid",
           [this](const Arguments& a)
           {
               auto made{oui::grid()};
               const auto lengths{[](const Value& given)
                                  {
                                      std::vector<oui::Length> result;
                                      for(std::size_t index{}; given.is_array() && index < given.length(); ++ index)
                                      {
                                          result.push_back(length_of(given.at(index)));
                                      }
                                      return result;
                                  }};
               if(const auto columns{lengths(argument(a, 0))}; !columns.empty())
               {
                   made->set_columns(columns);
               }
               if(const auto rows{lengths(argument(a, 1))}; !rows.empty())
               {
                   made->set_rows(rows);
               }
               const auto gap{text_of(argument(a, 2))};
               made->set_gap(gap == "none"     ? oui::Spacing::none
                             : gap == "normal" ? oui::Spacing::normal
                             : gap == "loose"  ? oui::Spacing::loose
                                               : oui::Spacing::tight);
               return m_engine.number(adopt(std::move(made)));
           });
    // Puts a widget in a grid, a scroll view or a panel, after what is there.
    define("containerAdd",
           [this](const Arguments& a)
           {
               auto& container{widget<oui::Container>(id_of(a, 0), "container")};
               container.add(take(id_of(a, 1)));
               return Value{};
           });
    define("scrollView", [this](const Arguments&) { return m_engine.number(adopt(oui::scroll_view())); });
    define("panel",
           [this](const Arguments& a)
           {
               auto made{oui::panel()};
               const auto padding{argument(a, 0)};
               const auto side{[&padding](std::size_t index)
                               {
                                   return padding.is_array() ? static_cast<int>(padding.at(index).to_number())
                                                             : static_cast<int>(padding.to_number());
                               }};
               if(padding.is_number() || (padding.is_array() && padding.length() == 4))
               {
                   made->set_padding({side(0), side(1), side(2), side(3)});
               }
               return m_engine.number(adopt(std::move(made)));
           });
    define("dock", [this](const Arguments&) { return m_engine.number(adopt(std::make_unique<oui::DockLayout>())); });
    // A side "top", "bottom", "left", "right" or "fill", in the order given; a size in cells, and a side strip the
    // mouse can make wider, keeping at least `minimum`.
    define("dockAdd",
           [this](const Arguments& a)
           {
               auto& dock{widget<oui::DockLayout>(id_of(a, 0), "dock")};
               const auto side{text_of(argument(a, 2))};
               const auto where{side == "top"      ? oui::Dock::top
                                : side == "bottom" ? oui::Dock::bottom
                                : side == "left"   ? oui::Dock::left
                                : side == "right"  ? oui::Dock::right
                                : side == "fill"   ? oui::Dock::fill
                                                   : throw ScriptError{"a dock's side is top, bottom, left, right or "
                                                                       "fill, not " +
                                                                     side}};
               auto& added{dock.add(where, take(id_of(a, 1)), static_cast<int>(argument(a, 3).to_number()))};
               if(const auto minimum{static_cast<int>(argument(a, 4).to_number())}; minimum > 0)
               {
                   dock.set_resizable(added, minimum);
               }
               return Value{};
           });
    define("hspacer", [this](const Arguments& a)
           { return m_engine.number(adopt(oui::hspacer(static_cast<int>(argument(a, 0).to_number())))); });
    define("vspacer", [this](const Arguments& a)
           { return m_engine.number(adopt(oui::vspacer(static_cast<int>(argument(a, 0).to_number())))); });
    define("comparison",
           [this](const Arguments&) { return m_engine.number(adopt(std::make_unique<oui::DiffView>())); });
    define("setComparison",
           [this](const Arguments& a)
           {
               auto& view{widget<oui::DiffView>(id_of(a, 0), "comparison")};
               view.set_titles(text_of(argument(a, 3)), text_of(argument(a, 4)));
               view.set_texts(text_of(argument(a, 1)), text_of(argument(a, 2)));
               return Value{};
           });
    define("comparisonCount",
           [this](const Arguments& a) {
               return m_engine.number(
                   static_cast<double>(widget<oui::DiffView>(id_of(a, 0), "comparison").difference_count()));
           });
    define("comparisonStep",
           [this](const Arguments& a)
           {
               auto& view{widget<oui::DiffView>(id_of(a, 0), "comparison")};
               if(argument(a, 1).to_bool())
               {
                   view.next_difference();
               }
               else
               {
                   view.previous_difference();
               }
               return Value{};
           });
    define("folderView",
           [this](const Arguments& a)
           {
               auto made{oui::folder_view()};
               made->show_folder(oui::whole_path(oui::path_from_utf8(text_of(argument(a, 0)))));
               return m_engine.number(adopt(std::move(made)));
           });
    define("getFolder",
           [this](const Arguments& a) {
               return m_engine.string(oui::utf8_of(widget<oui::FolderView>(id_of(a, 0), "folder view").current_path()));
           });
    define("setFolder",
           [this](const Arguments& a)
           {
               widget<oui::FolderView>(id_of(a, 0), "folder view")
                   .show_folder(oui::whole_path(oui::path_from_utf8(text_of(argument(a, 1)))));
               return Value{};
           });
    define("getFolderSelected",
           [this](const Arguments& a)
           {
               const auto chosen{widget<oui::FolderView>(id_of(a, 0), "folder view").selected_path()};
               return chosen ? m_engine.string(oui::utf8_of(*chosen)) : m_engine.null();
           });
    // A program in a terminal of its own, the user's shell unless a command is given, in the folder given.
    define("terminal",
           [this](const Arguments& a)
           {
               oui::PtyOptions options;
               const auto command{argument(a, 0)};
               for(std::size_t index{}; command.is_array() && index < command.length(); ++ index)
               {
                   options.command.push_back(command.at(index).to_string());
               }
               options.working_directory = oui::path_from_utf8(text_of(argument(a, 1)));
               try
               {
                   return m_engine.number(
                       adopt(std::make_unique<oui::TerminalView>(m_application, std::move(options))));
               }
               catch(const std::system_error& error)
               {
                   throw ScriptError{std::string{"oui.Terminal: "} + error.what()};
               }
           });
    define("terminalPaste",
           [this](const Arguments& a)
           {
               widget<oui::TerminalView>(id_of(a, 0), "terminal").paste(text_of(argument(a, 1)));
               return Value{};
           });
    define("terminalRunning", [this](const Arguments& a)
           { return m_engine.boolean(widget<oui::TerminalView>(id_of(a, 0), "terminal").is_running()); });
    define("calculator", [this](const Arguments&)
           { return m_engine.number(adopt(std::make_unique<oui::CalculatorView>(m_application))); });
    define("setTooltip",
           [this](const Arguments& a)
           {
               widget<oui::Widget>(id_of(a, 0), "widget").set_tooltip(text_of(argument(a, 1)));
               return Value{};
           });
    // Takes a widget out of what it is in, to put it somewhere else, or nowhere.
    define("remove",
           [this](const Arguments& a)
           {
               const auto id{id_of(a, 0)};
               auto& target{widget<oui::Widget>(id, "widget")};
               auto* const parent{target.parent()};
               if(parent == nullptr || m_loose.count(id) != 0)
               {
                   return Value{};
               }
               if(dynamic_cast<oui::TabView*>(parent) != nullptr || dynamic_cast<oui::SplitView*>(parent) != nullptr)
               {
                   throw ScriptError{"a page of tabs, or a side of a split, stays where it is"};
               }
               // A column's or a row's child takes its length with it.
               for(auto& [box_id, box] : m_boxes)
               {
                   if(m_widgets.count(box_id) == 0 || m_widgets[box_id] != parent)
                   {
                       continue;
                   }
                   auto& grid{static_cast<oui::Grid&>(*parent)};
                   for(std::size_t index{}; index < grid.child_count(); ++ index)
                   {
                       if(&grid.child_at(index) == &target && index < box.lengths.size())
                       {
                           box.lengths.erase(box.lengths.begin() + static_cast<std::ptrdiff_t>(index));
                       }
                   }
                   if(box.vertical)
                   {
                       grid.set_rows(box.lengths);
                   }
                   else
                   {
                       grid.set_columns(box.lengths);
                   }
               }
               if(m_application.focused() != nullptr && target.contains(m_application.focused()))
               {
                   m_application.focus(*m_window);
               }
               m_loose[id] = parent->remove(target);
               return Value{};
           });
    define("contextMenu",
           [this](const Arguments& a)
           {
               const auto id{id_of(a, 0)};
               static_cast<void>(widget<oui::Widget>(id, "widget"));
               m_context_menus[id] = menu_items(argument(a, 1));
               return Value{};
           });
    // A function that hears a key on the widget before it does, while it or what it holds has the focus: the key as
    // shortcuts write it, and the text it types; true keeps it from going further.
    define(
        "onKey",
        [this](const Arguments& a)
        {
            const auto function{argument(a, 1)};
            if(!function.is_function())
            {
                throw ScriptError{"onKey: not a function"};
            }
            widget<oui::Widget>(id_of(a, 0), "widget")
                .set_key_handler(
                    [this, function](const oui::Event& event)
                    {
                        const auto* const text{std::get_if<oui::TextEvent>(&event)};
                        const auto typed{text != nullptr && !text->control && !text->alt
                                             ? oui::encode_utf8(text->codepoint)
                                             : std::string{}};
                        try
                        {
                            return function.call({m_engine.string(key_name(event)), m_engine.string(typed)}).to_bool();
                        }
                        catch(const ScriptStop&)
                        {
                            return true;
                        }
                        catch(const ScriptError& error)
                        {
                            report(error.what());
                            return false;
                        }
                    });
            return Value{};
        });
    // A widget's background, or its text's colour, by the name of the theme's colour.
    define("setColor",
           [this](const Arguments& a)
           {
               auto* const target{m_widgets.count(id_of(a, 0)) != 0 ? m_widgets[id_of(a, 0)] : nullptr};
               const auto which{text_of(argument(a, 1))};
               const auto ref{color_of(argument(a, 2))};
               const auto background{which == "background"};
               if(auto* const label{dynamic_cast<oui::Label*>(target)})
               {
                   background ? label->set_background(ref) : label->set_color(ref);
               }
               else if(auto* const view{dynamic_cast<oui::MarkdownView*>(target)})
               {
                   background ? view->set_background(ref) : view->set_foreground(ref);
               }
               else if(auto* const field{dynamic_cast<oui::LineEdit*>(target)}; field != nullptr && background)
               {
                   field->set_background(ref);
               }
               else if(auto* const check{dynamic_cast<oui::CheckButton*>(target)}; check != nullptr && background)
               {
                   check->set_background(ref);
               }
               else if(auto* const button{dynamic_cast<oui::Button*>(target)}; button != nullptr && background)
               {
                   button->set_background(ref);
               }
               else if(auto* const tree{dynamic_cast<oui::TreeView*>(target)}; tree != nullptr && background)
               {
                   tree->set_background(ref);
               }
               else
               {
                   throw ScriptError{"that widget does not take a " + which};
               }
               return Value{};
           });
    // The look: one of the terminal's themes by its id, or "classic", the graphical one.
    define("theme",
           [this](const Arguments& a)
           {
               const auto name{text_of(argument(a, 0))};
#ifdef OLAGARRO_OUI_RUN_WINDOW
               if(name == "classic")
               {
                   m_application.set_theme(oui::classic_theme());
                   m_theme_chosen = true;
                   return Value{};
               }
#endif
               std::vector<std::string> names;
               for(const auto& theme : oui::terminal_themes())
               {
                   if(theme.id == name)
                   {
                       m_application.set_theme(theme.theme);
                       m_theme_chosen = true;
                       return Value{};
                   }
                   names.push_back(theme.id);
               }
#ifdef OLAGARRO_OUI_RUN_WINDOW
               names.emplace_back("classic");
#endif
               // The themes this build has, as a list says them: "a, b or c".
               std::string listed;
               for(std::size_t index{}; index < names.size(); ++ index)
               {
                   listed += (index == 0 ? "" : index + 1 == names.size() ? " or " : ", ") + names[index];
               }
               throw ScriptError{"there is no theme " + name + ": " + listed};
           });
    define("status",
           [this](const Arguments& a)
           {
               if(m_status == nullptr)
               {
                   auto bar{oui::status_bar()};
                   m_status = bar.get();
                   m_pending_status = std::move(bar);
                   place_bars();
               }
               m_status->set_left(text_of(argument(a, 0)));
               m_status->set_right(text_of(argument(a, 1)));
               return Value{};
           });
    // Headings that sort when clicked, though only by saying so: the script orders its own rows. Each column the
    // width given, in cells, but the last, which takes the rest.
    define("table",
           [this](const Arguments& a)
           {
               const auto headings{argument(a, 0)};
               const auto widths{argument(a, 1)};
               std::vector<std::string> titles;
               for(std::size_t index{}; index < headings.length(); ++ index)
               {
                   titles.push_back(headings.at(index).to_string());
               }
               auto made{oui::table(titles)};
               made->set_headers_clickable(true);
               made->set_sorted_by_owner(true);
               for(std::size_t index{}; index < titles.size(); ++ index)
               {
                   const auto width{index < widths.length() ? widths.at(index).to_number() : 1.0};
                   made->set_column_width(index, std::max(1, static_cast<int>(width)));
               }
               if(!titles.empty())
               {
                   made->set_stretch_column(titles.size() - 1);
               }
               return m_engine.number(adopt(std::move(made)));
           });
    // The rows again, keeping the row chosen, by its place, and where the table was scrolled to, as a table filled
    // again every few seconds wants.
    define("setRows",
           [this](const Arguments& a)
           {
               auto& table{widget<oui::Table>(id_of(a, 0), "table")};
               const auto rows{argument(a, 1)};
               const auto chosen{table.selected_row()};
               const auto scrolled{table.scroll_offset()};
               table.clear();
               const auto columns{table.column_count()};
               for(std::size_t row{}; row < rows.length(); ++ row)
               {
                   const auto cells{rows.at(row)};
                   for(std::size_t column{}; column < columns; ++ column)
                   {
                       table.add(oui::label(column < cells.length() ? cells.at(column).to_string() : std::string{}));
                   }
               }
               table.scroll_to(scrolled);
               if(rows.length() > 0)
               {
                   table.select_row(std::min(chosen.value_or(0), rows.length() - 1));
               }
               return Value{};
           });
    // Marks the column the rows are in the order of, as a click on its heading would, and that one only.
    define("setTableSort",
           [this](const Arguments& a)
           {
               auto& table{widget<oui::Table>(id_of(a, 0), "table")};
               const auto marked{static_cast<std::size_t>(argument(a, 1).to_number())};
               const auto text{argument(a, 2).to_string()};
               const auto order{text == "ascending"    ? oui::SortOrder::ascending
                                : text == "descending" ? oui::SortOrder::descending
                                                       : oui::SortOrder::none};
               for(std::size_t column{}; column < table.column_count(); ++ column)
               {
                   table.set_column_sort(column, column == marked ? order : oui::SortOrder::none);
               }
               return Value{};
           });
    define("checkButton",
           [this](const Arguments& a) { return m_engine.number(adopt(oui::check_button(text_of(argument(a, 0))))); });
    define("comboBox", [this](const Arguments&) { return m_engine.number(adopt(oui::combo_box())); });
    define("getChecked", [this](const Arguments& a)
           { return m_engine.boolean(widget<oui::CheckButton>(id_of(a, 0), "check button").is_checked()); });
    define("setChecked",
           [this](const Arguments& a)
           {
               widget<oui::CheckButton>(id_of(a, 0), "check button").set_checked(argument(a, 1).to_bool());
               return Value{};
           });
    define("ui", [this](const Arguments& a) { return describe(text_of(argument(a, 0)), "oui.ui: "); });
    define("uiFromFile",
           [this](const Arguments& a)
           {
               // Found from the script's folder, wherever it is run from, as the files that come with it are.
               const auto name{text_of(argument(a, 0))};
               const auto given{oui::path_from_utf8(name)};
               const auto text{oui::read_file(given.is_absolute() ? given : m_script_folder / given)};
               if(!text)
               {
                   throw ScriptError{"oui.uiFromFile: cannot read " + name};
               }
               return describe(*text, name + ":");
           });
    define("uiPut",
           [this](const Arguments& a)
           {
               auto& described{m_described.at(id_of(a, 0))};
               const auto name{text_of(argument(a, 1))};
               try
               {
                   if(!described.ui.is_slot(name))
                   {
                       throw ScriptError{"put: " + name + " is not a Slot"};
                   }
               }
               catch(const std::logic_error& error)
               {
                   throw ScriptError{std::string{"put: "} + error.what()};
               }
               described.ui.put(name, take(id_of(a, 2)));
               // The Slot is gone; the id is the widget's put there.
               m_widgets.erase(described.ids.at(name));
               described.ids[name] = id_of(a, 2);
               return Value{};
           });
    define("windowDescribed",
           [this](const Arguments& a)
           {
               open_described_window(id_of(a, 0));
               return Value{};
           });
    define("box",
           [this](const Arguments& a)
           {
               auto grid{oui::grid()};
               const auto gap{text_of(argument(a, 1))};
               grid->set_gap(gap == "none"     ? oui::Spacing::none
                             : gap == "normal" ? oui::Spacing::normal
                             : gap == "loose"  ? oui::Spacing::loose
                                               : oui::Spacing::tight);
               const auto id{adopt(std::move(grid))};
               m_boxes[id] = {argument(a, 0).to_bool(), {}};
               return m_engine.number(id);
           });
    define("boxAdd",
           [this](const Arguments& a)
           {
               const auto id{id_of(a, 0)};
               auto& grid{widget<oui::Grid>(id, "column or row")};
               auto& box{m_boxes.at(id)};
               // A child that fills takes the room the others leave; the rest are as big as they need.
               box.lengths.push_back(argument(a, 2).to_bool() ? Length::flex() : Length::content());
               if(box.vertical)
               {
                   grid.set_columns({Length::fill()});
                   grid.set_rows(box.lengths);
               }
               else
               {
                   grid.set_columns(box.lengths);
                   grid.set_rows({Length::content()});
               }
               grid.add(take(id_of(a, 1)));
               return Value{};
           });
    define("split",
           [this](const Arguments& a)
           {
               return m_engine.number(
                   adopt(oui::split_view(argument(a, 0).to_bool() ? oui::SplitView::Orientation::vertical
                                                                  : oui::SplitView::Orientation::horizontal)));
           });
    define("splitAdd",
           [this](const Arguments& a)
           {
               widget<oui::SplitView>(id_of(a, 0), "split").add(take(id_of(a, 1)));
               return Value{};
           });
    define("splitRatio",
           [this](const Arguments& a)
           {
               widget<oui::SplitView>(id_of(a, 0), "split").set_ratio(argument(a, 1).to_number());
               return Value{};
           });
    define("getSplitRatio", [this](const Arguments& a)
           { return m_engine.number(widget<oui::SplitView>(id_of(a, 0), "split").ratio()); });
    define("tabs",
           [this](const Arguments& a)
           {
               auto tabs{oui::tab_view()};
               tabs->set_closable(!argument(a, 0).to_bool());
               return m_engine.number(adopt(std::move(tabs)));
           });
    define("tabsAdd",
           [this](const Arguments& a)
           {
               widget<oui::TabView>(id_of(a, 0), "tabs").add_tab(text_of(argument(a, 1)), take(id_of(a, 2)));
               return Value{};
           });
    define("tabsCount", [this](const Arguments& a)
           { return m_engine.number(static_cast<double>(widget<oui::TabView>(id_of(a, 0), "tabs").child_count())); });
    define("getTabsSelected",
           [this](const Arguments& a) {
               return m_engine.number(static_cast<double>(widget<oui::TabView>(id_of(a, 0), "tabs").selected_index()));
           });
    define("setTabsSelected",
           [this](const Arguments& a)
           {
               auto& tabs{widget<oui::TabView>(id_of(a, 0), "tabs")};
               const auto index{argument(a, 1).to_number()};
               if(index >= 0 && index < static_cast<double>(tabs.child_count()))
               {
                   tabs.select(static_cast<std::size_t>(index));
               }
               return Value{};
           });
    define("markdownView",
           [this](const Arguments& a) { return m_engine.number(adopt(oui::markdown_view(text_of(argument(a, 0))))); });
    define("markdownGoTo",
           [this](const Arguments& a)
           {
               return m_engine.boolean(
                   widget<oui::MarkdownView>(id_of(a, 0), "Markdown view").scroll_to_anchor(text_of(argument(a, 1))));
           });
    define("getMarkdown", [this](const Arguments& a)
           { return m_engine.string(widget<oui::MarkdownView>(id_of(a, 0), "Markdown view").markdown()); });
    define("setMarkdown",
           [this](const Arguments& a)
           {
               widget<oui::MarkdownView>(id_of(a, 0), "Markdown view").set_markdown(text_of(argument(a, 1)));
               return Value{};
           });
    define("getText",
           [this](const Arguments& a)
           {
               auto* const target{m_widgets.count(id_of(a, 0)) != 0 ? m_widgets[id_of(a, 0)] : nullptr};
               if(auto* const label{dynamic_cast<oui::Label*>(target)})
               {
                   return m_engine.string(label->text());
               }
               if(auto* const field{dynamic_cast<oui::LineEdit*>(target)})
               {
                   return m_engine.string(field->text());
               }
               if(auto* const button{dynamic_cast<oui::Button*>(target)})
               {
                   return m_engine.string(std::string{button->text()});
               }
               return m_engine.string(widget<oui::TextEdit>(id_of(a, 0), "text").text());
           });
    define("setText",
           [this](const Arguments& a)
           {
               auto* const target{m_widgets.count(id_of(a, 0)) != 0 ? m_widgets[id_of(a, 0)] : nullptr};
               const auto text{text_of(argument(a, 1))};
               if(auto* const label{dynamic_cast<oui::Label*>(target)})
               {
                   label->set_text(text);
               }
               else if(auto* const field{dynamic_cast<oui::LineEdit*>(target)})
               {
                   field->set_text(text);
               }
               else if(auto* const button{dynamic_cast<oui::Button*>(target)})
               {
                   button->set_text(text);
               }
               else
               {
                   widget<oui::TextEdit>(id_of(a, 0), "text").set_text(text);
               }
               return Value{};
           });
    define("setBold",
           [this](const Arguments& a)
           {
               widget<oui::Label>(id_of(a, 0), "label").set_bold(argument(a, 1).to_bool());
               return Value{};
           });
    define("setWrap",
           [this](const Arguments& a)
           {
               auto* const target{m_widgets.count(id_of(a, 0)) != 0 ? m_widgets[id_of(a, 0)] : nullptr};
               if(auto* const label{dynamic_cast<oui::Label*>(target)})
               {
                   label->set_wrap(argument(a, 1).to_bool());
               }
               else
               {
                   widget<oui::TextEdit>(id_of(a, 0), "text edit").set_word_wrap(argument(a, 1).to_bool());
               }
               return Value{};
           });
    define("setPlaceholder",
           [this](const Arguments& a)
           {
               widget<oui::LineEdit>(id_of(a, 0), "field").set_placeholder(text_of(argument(a, 1)));
               return Value{};
           });
    define("setReadOnly",
           [this](const Arguments& a)
           {
               widget<oui::TextEdit>(id_of(a, 0), "text edit").set_read_only(argument(a, 1).to_bool());
               return Value{};
           });
    define("setLineNumbers",
           [this](const Arguments& a)
           {
               widget<oui::TextEdit>(id_of(a, 0), "text edit").set_line_numbers_visible(argument(a, 1).to_bool());
               return Value{};
           });
    define("append",
           [this](const Arguments& a)
           {
               auto& edit{widget<oui::TextEdit>(id_of(a, 0), "text edit")};
               const auto read_only{edit.is_read_only()};
               edit.select_range(edit.text().size(), edit.text().size());
               edit.set_read_only(false);
               edit.paste(text_of(argument(a, 1)));
               edit.set_read_only(read_only);
               return Value{};
           });
    define("setItems",
           [this](const Arguments& a)
           {
               auto& list{widget<oui::List>(id_of(a, 0), "list")};
               const auto items{argument(a, 1)};
               list.clear();
               auto* const checks{dynamic_cast<oui::CheckList*>(&list)};
               for(std::size_t index{}; index < items.length(); ++ index)
               {
                   if(checks != nullptr)
                   {
                       checks->add_item(items.at(index).to_string(), false);
                   }
                   else
                   {
                       list.add_item(items.at(index).to_string());
                   }
               }
               if(items.length() > 0)
               {
                   list.select_row(0);
               }
               return Value{};
           });
    define("getSelected",
           [this](const Arguments& a)
           {
               const auto row{widget<oui::Table>(id_of(a, 0), "list").selected_row()};
               return m_engine.number(row ? static_cast<double>(*row) : -1.0);
           });
    define("setSelected",
           [this](const Arguments& a)
           {
               auto& list{widget<oui::Table>(id_of(a, 0), "list")};
               const auto row{argument(a, 1).to_number()};
               if(row >= 0 && row < static_cast<double>(list.row_count()))
               {
                   list.select_row(static_cast<std::size_t>(row));
               }
               return Value{};
           });
    define("setComboItems",
           [this](const Arguments& a)
           {
               const auto items{argument(a, 1)};
               std::vector<std::string> options;
               for(std::size_t index{}; index < items.length(); ++ index)
               {
                   options.push_back(items.at(index).to_string());
               }
               widget<oui::ComboBox>(id_of(a, 0), "combo box").set_options(std::move(options));
               return Value{};
           });
    define("getComboSelected",
           [this](const Arguments& a) {
               return m_engine.number(static_cast<double>(widget<oui::ComboBox>(id_of(a, 0), "combo box").selected()));
           });
    define("setComboSelected",
           [this](const Arguments& a)
           {
               auto& combo{widget<oui::ComboBox>(id_of(a, 0), "combo box")};
               const auto index{argument(a, 1).to_number()};
               if(index >= 0 && index < static_cast<double>(combo.options().size()))
               {
                   combo.set_selected(static_cast<std::size_t>(index));
               }
               return Value{};
           });
    define("focus",
           [this](const Arguments& a)
           {
               // What cannot have the focus passes it to the first of its widgets that can, and a calculator to
               // the line its expressions are written on, below its log.
               auto& chosen{widget<oui::Widget>(id_of(a, 0), "widget")};
               if(auto* const calculator{dynamic_cast<oui::CalculatorView*>(&chosen)})
               {
                   m_application.focus(calculator->prompt());
                   return Value{};
               }
               std::vector<oui::Widget*> focusable;
               chosen.collect_focusable(focusable);
               m_application.focus(chosen.is_focusable() || focusable.empty() ? chosen : *focusable.front());
               return Value{};
           });
    define("on",
           [this](const Arguments& a)
           {
               on(id_of(a, 0), text_of(argument(a, 1)), argument(a, 2));
               return Value{};
           });
    // Text edits and fields: places in their text in JavaScript's units, the selection, and what is done at it.
    define("getCursor",
           [this](const Arguments& a)
           {
               const auto& edit{widget<oui::TextEdit>(id_of(a, 0), "text edit")};
               return m_engine.number(js_index_of(edit.text(), edit.cursor_offset()));
           });
    define("getSelection",
           [this](const Arguments& a)
           {
               auto* const target{m_widgets.count(id_of(a, 0)) != 0 ? m_widgets[id_of(a, 0)] : nullptr};
               std::pair<std::size_t, std::size_t> range;
               std::string_view text;
               if(auto* const field{dynamic_cast<oui::LineEdit*>(target)})
               {
                   range = field->selection_range();
                   text = field->text();
               }
               else
               {
                   const auto& edit{widget<oui::TextEdit>(id_of(a, 0), "text edit or field")};
                   range = edit.selection_range();
                   text = edit.text();
               }
               return m_engine.array(
                   {m_engine.number(js_index_of(text, range.first)), m_engine.number(js_index_of(text, range.second))});
           });
    // Selects from start to end, the cursor at end; the same place twice only moves the cursor there.
    define("select",
           [this](const Arguments& a)
           {
               auto* const target{m_widgets.count(id_of(a, 0)) != 0 ? m_widgets[id_of(a, 0)] : nullptr};
               if(auto* const field{dynamic_cast<oui::LineEdit*>(target)})
               {
                   field->select_range(byte_of(field->text(), argument(a, 1)), byte_of(field->text(), argument(a, 2)));
               }
               else
               {
                   auto& edit{widget<oui::TextEdit>(id_of(a, 0), "text edit or field")};
                   edit.select_range(byte_of(edit.text(), argument(a, 1)), byte_of(edit.text(), argument(a, 2)));
               }
               return Value{};
           });
    define("selectAll",
           [this](const Arguments& a)
           {
               auto* const target{m_widgets.count(id_of(a, 0)) != 0 ? m_widgets[id_of(a, 0)] : nullptr};
               if(auto* const field{dynamic_cast<oui::LineEdit*>(target)})
               {
                   field->select_all();
               }
               else
               {
                   widget<oui::TextEdit>(id_of(a, 0), "text edit or field").select_all();
               }
               return Value{};
           });
    define("getSelectedText",
           [this](const Arguments& a)
           {
               auto* const target{m_widgets.count(id_of(a, 0)) != 0 ? m_widgets[id_of(a, 0)] : nullptr};
               if(auto* const field{dynamic_cast<oui::LineEdit*>(target)})
               {
                   return m_engine.string(field->selected_text());
               }
               return m_engine.string(widget<oui::TextEdit>(id_of(a, 0), "text edit or field").selected_text());
           });
    // Writes text in place of the selection, as typing would: one step to undo, and nothing when read only.
    define("insert",
           [this](const Arguments& a)
           {
               auto* const target{m_widgets.count(id_of(a, 0)) != 0 ? m_widgets[id_of(a, 0)] : nullptr};
               if(auto* const field{dynamic_cast<oui::LineEdit*>(target)})
               {
                   field->paste(text_of(argument(a, 1)));
               }
               else
               {
                   widget<oui::TextEdit>(id_of(a, 0), "text edit or field").paste(text_of(argument(a, 1)));
               }
               return Value{};
           });
    define("getModified", [this](const Arguments& a)
           { return m_engine.boolean(widget<oui::TextEdit>(id_of(a, 0), "text edit").is_modified()); });
    define("setModified",
           [this](const Arguments& a)
           {
               auto& edit{widget<oui::TextEdit>(id_of(a, 0), "text edit")};
               argument(a, 1).to_bool() ? edit.mark_modified() : edit.mark_saved();
               return Value{};
           });
    // Looks for text from the cursor, forwards or backwards, wrapping round, and selects what it finds.
    define("find",
           [this](const Arguments& a)
           {
               auto& edit{widget<oui::TextEdit>(id_of(a, 0), "text edit")};
               const auto needle{text_of(argument(a, 1))};
               const auto options{argument(a, 2)};
               const auto search{search_options_of(options)};
               return m_engine.boolean(options.is_object() && options.get("backward").to_bool()
                                           ? edit.find_previous(needle, search)
                                           : edit.find_next(needle, search));
           });
    define("replace",
           [this](const Arguments& a)
           {
               auto& edit{widget<oui::TextEdit>(id_of(a, 0), "text edit")};
               return m_engine.boolean(edit.replace_next(text_of(argument(a, 1)), text_of(argument(a, 2)),
                                                         search_options_of(argument(a, 3))));
           });
    define("replaceAll",
           [this](const Arguments& a)
           {
               auto& edit{widget<oui::TextEdit>(id_of(a, 0), "text edit")};
               return m_engine.number(static_cast<double>(edit.replace_all(
                   text_of(argument(a, 1)), text_of(argument(a, 2)), search_options_of(argument(a, 3)))));
           });
    // Shows every match until called with null; gives how many there are.
    define("showMatches",
           [this](const Arguments& a)
           {
               auto& edit{widget<oui::TextEdit>(id_of(a, 0), "text edit")};
               const auto needle{argument(a, 1)};
               if(needle.is_null() || needle.is_undefined())
               {
                   edit.clear_search_highlight();
                   return m_engine.number(0);
               }
               edit.set_search_highlight(needle.to_string(), search_options_of(argument(a, 2)));
               return m_engine.number(static_cast<double>(edit.search_match_count()));
           });
    define("goToLine",
           [this](const Arguments& a)
           {
               auto& edit{widget<oui::TextEdit>(id_of(a, 0), "text edit")};
               return m_engine.boolean(
                   edit.go_to_line(static_cast<std::size_t>(std::max(0.0, argument(a, 1).to_number()))));
           });
    define("undo", [this](const Arguments& a)
           { return m_engine.boolean(widget<oui::TextEdit>(id_of(a, 0), "text edit").undo()); });
    define("redo", [this](const Arguments& a)
           { return m_engine.boolean(widget<oui::TextEdit>(id_of(a, 0), "text edit").redo()); });
    // The lines the selection touches, or the cursor's: copied below, moved up or down, deleted, or commented with
    // a prefix.
    define("editLines",
           [this](const Arguments& a)
           {
               auto& edit{widget<oui::TextEdit>(id_of(a, 0), "text edit")};
               const auto what{text_of(argument(a, 1))};
               if(what == "duplicate")
               {
                   return m_engine.boolean(edit.duplicate_lines());
               }
               if(what == "up" || what == "down")
               {
                   return m_engine.boolean(edit.move_lines(what == "down"));
               }
               if(what == "delete")
               {
                   return m_engine.boolean(edit.delete_lines());
               }
               if(what == "comment")
               {
                   return m_engine.boolean(edit.toggle_line_comment(text_of(argument(a, 2))));
               }
               throw ScriptError{"lines are duplicated, moved up or down, deleted or commented, not " + what};
           });
    define("setEditOption",
           [this](const Arguments& a)
           {
               auto& edit{widget<oui::TextEdit>(id_of(a, 0), "text edit")};
               const auto name{text_of(argument(a, 1))};
               const auto value{argument(a, 2)};
               if(name == "tabSpaces")
               {
                   edit.set_tab_spaces(static_cast<int>(value.to_number()));
               }
               else if(name == "useSpaces")
               {
                   edit.set_use_spaces(value.to_bool());
               }
               else if(name == "autoIndent")
               {
                   edit.set_auto_indent(value.to_bool());
               }
               else if(name == "occurrences")
               {
                   edit.set_occurrences_shown(value.to_bool());
               }
               else if(name == "scrollbar")
               {
                   edit.set_scrollbar_visible(value.to_bool());
               }
               else
               {
                   throw ScriptError{"a text edit has no option " + name};
               }
               return Value{};
           });
    // Colours the text by the rules of states, as OUI Code's languages do; null leaves it all in one colour.
    define("setHighlight",
           [this](const Arguments& a)
           {
               auto& edit{widget<oui::TextEdit>(id_of(a, 0), "text edit")};
               const auto states{argument(a, 1)};
               edit.set_syntax_highlighter(states.is_null() || states.is_undefined()
                                               ? nullptr
                                               : oui::script::highlighter_of(states, "highlight"));
               return Value{};
           });
    // Tabs: the titles, taking a page out to go elsewhere, and moving one among the others.
    define("getTabTitle",
           [this](const Arguments& a)
           {
               const auto& tabs{widget<oui::TabView>(id_of(a, 0), "tabs")};
               const auto index{static_cast<std::size_t>(argument(a, 1).to_number())};
               if(index >= tabs.child_count())
               {
                   throw ScriptError{"there is no tab " + std::to_string(index)};
               }
               return m_engine.string(std::string{tabs.title(index)});
           });
    define("setTabTitle",
           [this](const Arguments& a)
           {
               auto& tabs{widget<oui::TabView>(id_of(a, 0), "tabs")};
               const auto index{static_cast<std::size_t>(argument(a, 1).to_number())};
               if(index >= tabs.child_count())
               {
                   throw ScriptError{"there is no tab " + std::to_string(index)};
               }
               tabs.set_title(index, text_of(argument(a, 2)));
               return Value{};
           });
    define("tabsRemove",
           [this](const Arguments& a)
           {
               auto& tabs{widget<oui::TabView>(id_of(a, 0), "tabs")};
               const auto index{static_cast<std::size_t>(argument(a, 1).to_number())};
               if(index >= tabs.child_count())
               {
                   throw ScriptError{"there is no tab " + std::to_string(index)};
               }
               auto* const page{&tabs.child_at(index)};
               if(m_application.focused() != nullptr && page->contains(m_application.focused()))
               {
                   m_application.focus(tabs);
               }
               const auto found{std::find_if(m_widgets.begin(), m_widgets.end(),
                                             [page](const auto& entry) { return entry.second == page; })};
               auto taken{tabs.take_tab(index)};
               if(found != m_widgets.end())
               {
                   m_loose[found->first] = std::move(taken);
               }
               return Value{};
           });
    define("tabsMove",
           [this](const Arguments& a)
           {
               auto& tabs{widget<oui::TabView>(id_of(a, 0), "tabs")};
               const auto from{static_cast<std::size_t>(argument(a, 1).to_number())};
               const auto to{static_cast<std::size_t>(argument(a, 2).to_number())};
               if(from >= tabs.child_count() || to >= tabs.child_count())
               {
                   throw ScriptError{"there is no tab there"};
               }
               tabs.move_tab(from, to);
               return Value{};
           });
    // Which tabs ask for attention, their close mark in the warning colour, as documents not saved.
    define("setTabsMarked",
           [this](const Arguments& a)
           {
               auto& tabs{widget<oui::TabView>(id_of(a, 0), "tabs")};
               const auto marked{argument(a, 1)};
               std::vector<bool> flags;
               for(std::size_t index{}; index < marked.length(); ++ index)
               {
                   flags.push_back(marked.at(index).to_bool());
               }
               tabs.set_highlight_predicate([flags](std::size_t index)
                                            { return index < flags.size() && flags[index]; });
               return Value{};
           });
    // Tables: the columns' widths, which one takes the room left, and what the user may change.
    define("setTableOption",
           [this](const Arguments& a)
           {
               auto& table{widget<oui::Table>(id_of(a, 0), "table")};
               const auto name{text_of(argument(a, 1))};
               const auto value{argument(a, 2)};
               if(name == "widths")
               {
                   for(std::size_t column{}; column < value.length(); ++ column)
                   {
                       table.set_column_width(column, std::max(1, static_cast<int>(value.at(column).to_number())));
                   }
               }
               else if(name == "stretch")
               {
                   table.set_stretch_column(
                       value.is_null() ? std::nullopt
                                       : std::optional<std::size_t>{static_cast<std::size_t>(value.to_number())});
               }
               else if(name == "resizable")
               {
                   table.set_columns_resizable(value.to_bool());
               }
               else if(name == "headers")
               {
                   table.set_headers_visible(value.to_bool());
               }
               else
               {
                   throw ScriptError{"a table has no option " + name};
               }
               return Value{};
           });
    define("getColumnWidths",
           [this](const Arguments& a)
           {
               const auto& table{widget<oui::Table>(id_of(a, 0), "table")};
               std::vector<Value> widths;
               for(std::size_t column{}; column < table.column_count(); ++ column)
               {
                   widths.push_back(m_engine.number(table.column_width(column)));
               }
               return m_engine.array(widths);
           });
    define("showRow",
           [this](const Arguments& a)
           {
               auto& table{widget<oui::Table>(id_of(a, 0), "table")};
               const auto row{argument(a, 1).to_number()};
               if(row >= 0 && row < static_cast<double>(table.row_count()))
               {
                   table.ensure_row_visible(static_cast<std::size_t>(row));
               }
               return Value{};
           });
    // A canvas: a size of its own, or none to take the room it has.
    define("canvas",
           [this](const Arguments& a)
           {
               auto canvas{std::make_unique<Canvas>()};
               canvas->set_size(
                   {static_cast<int>(argument(a, 0).to_number()), static_cast<int>(argument(a, 1).to_number())});
               return m_engine.number(adopt(std::move(canvas)));
           });
    define("canvasClear",
           [this](const Arguments& a)
           {
               const auto background{argument(a, 1)};
               widget<Canvas>(id_of(a, 0), "canvas")
                   .clear(background.is_undefined() || background.is_null() ? std::nullopt
                                                                            : std::optional{color_of(background)});
               return Value{};
           });
    // Text from a cell, or with none an area filled: {color, background, bold, underline}.
    define("canvasDraw",
           [this](const Arguments& a)
           {
               auto& canvas{widget<Canvas>(id_of(a, 0), "canvas")};
               const auto number{[&a](std::size_t index) { return static_cast<int>(argument(a, index).to_number()); }};
               const auto style{argument(a, 6)};
               Canvas::Stroke stroke{{number(1), number(2), number(3), number(4)}, text_of(argument(a, 5)), {}, {}, 0};
               if(style.is_object())
               {
                   if(const auto color{style.get("color")}; !color.is_undefined())
                   {
                       stroke.foreground = color_of(color);
                   }
                   if(const auto background{style.get("background")}; !background.is_undefined())
                   {
                       stroke.background = color_of(background);
                   }
                   stroke.attributes =
                       static_cast<std::uint8_t>((style.get("bold").to_bool() ? oui::text_bold : 0) |
                                                 (style.get("underline").to_bool() ? oui::text_underline : 0));
               }
               canvas.add(std::move(stroke));
               return Value{};
           });
    define("canvasSize",
           [this](const Arguments& a)
           {
               const auto& area{widget<Canvas>(id_of(a, 0), "canvas").bounds()};
               return m_engine.array({m_engine.number(area.width), m_engine.number(area.height)});
           });
    // What a script keeps from one run to the next, in <script>.ini in the user's configuration folder for oui_run:
    // ~/.config/oui_run on Linux. Each value as JSON text; saved as it is set.
    define("settingGet",
           [this](const Arguments& a)
           {
               const auto value{settings().get(setting_key(argument(a, 0)))};
               return value.empty() ? m_engine.null() : m_engine.string(value);
           });
    define("settingSet",
           [this](const Arguments& a)
           {
               auto& kept{settings()};
               const auto key{setting_key(argument(a, 0))};
               const auto value{argument(a, 1)};
               value.is_null() ? kept.remove(key) : kept.set(key, value.to_string());
               if(!kept.save())
               {
                   throw ScriptError{"the settings could not be saved in " + oui::utf8_of(kept.file_path())};
               }
               return Value{};
           });
    // A dialog of the script's: a window with a widget of the script's in it, under a title.
    define("dialog",
           [this](const Arguments& a)
           {
               const auto options{argument(a, 2)};
               const auto number{[&options](std::string_view name)
                                 {
                                     const auto value{options.get(name)};
                                     return value.is_undefined() ? 0 : static_cast<int>(value.to_number());
                                 }};
               auto window{oui::window(text_of(argument(a, 0)))};
               window->add(take(id_of(a, 1)));
               window->set_preferred_size({number("width"), number("height")});
               window->set_resizable(options.get("resizable").to_bool());
               return m_engine.number(adopt(std::move(window)));
           });
    // Shows a dialog over the window, kept whole as it closes to be shown again; the promise is whether it closed
    // accepted.
    define("dialogShow",
           [this](const Arguments& a)
           {
               const auto id{id_of(a, 0)};
               static_cast<void>(widget<oui::Window>(id, "dialog"));
               if(m_window == nullptr)
               {
                   throw ScriptError{"a dialog opens over the window: oui.window first"};
               }
               if(m_dialogs.count(id) != 0)
               {
                   throw ScriptError{"the dialog is open already"};
               }
               auto pending{m_engine.promise()};
               // Shown once this call has returned: the dialog waits on the user, and the script must not.
               m_application.post(
                   [this, id, pending]
                   {
                       auto taken{take(id)};
                       std::unique_ptr<oui::Window> window{static_cast<oui::Window*>(taken.release())};
                       m_dialogs[id] = window.get();
                       const auto result{m_application.show_modal_and_keep(window)};
                       m_dialogs.erase(id);
                       m_loose[id] = std::move(window);
                       call(pending.resolve, {m_engine.boolean(result == oui::DialogResult::accepted)});
                   });
               return pending.promise;
           });
    define("dialogClose",
           [this](const Arguments& a)
           {
               if(const auto found{m_dialogs.find(id_of(a, 0))}; found != m_dialogs.end())
               {
                   found->second->close(argument(a, 1).to_bool() ? oui::DialogResult::accepted
                                                                 : oui::DialogResult::cancelled);
               }
               return Value{};
           });
    define("window",
           [this](const Arguments& a)
           {
               open_window(text_of(argument(a, 0)));
               return Value{};
           });
    define("windowContent",
           [this](const Arguments& a)
           {
               set_window_content(id_of(a, 0));
               return Value{};
           });
    define("shortcut",
           [this](const Arguments& a)
           {
               const auto keys{text_of(argument(a, 0))};
               const auto shortcut{oui::parse_shortcut(keys)};
               const auto function{argument(a, 1)};
               if(!shortcut || !function.is_function())
               {
                   throw ScriptError{"oui.shortcut takes keys such as \"Ctrl+S\" and a function"};
               }
               const auto id{"oui_run.shortcut." + std::to_string(++ m_next_id)};
               m_application.commands().register_command(id, keys, [this, function] { call(function); });
               m_application.commands().bind(id, *shortcut);
               return Value{};
           });
    define("quit",
           [this](const Arguments&)
           {
               quit();
               return Value{};
           });
    define("exit",
           [this](const Arguments& a) -> Value
           {
               m_exit_code = static_cast<int>(argument(a, 0).to_number());
               quit();
               throw ScriptStop{};
           });
}
