#pragma once

#include <oui/application.h>
#include <oui/layout.h>
#include <oui/menu_bar.h>
#include <oui/ui.h>
#include <oui/script/script_engine.h>
#include <oui/status_bar.h>
#include <oui/tree_view.h>
#include <oui/script/system_api.h>
#include <oui/settings.h>
#include <oui/widget.h>
#include <oui/window.h>
#ifdef OLAGARRO_OUI_RUN_WINDOW
#include <oui/graphics/window_backend.h>
#endif

#include <cstdio>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Writes `text` to `stream` at once, as a program's output goes.
void write_to(std::FILE* stream, std::string_view text);

// What a script runs with: the engine, the widgets it made, and what it started and still waits on. It defines what
// the script reaches as `oui`, the system's calls and the widgets of prelude.js, and opens the one window when the
// script asks for it, in the terminal or, with --window, on the desktop.
class Host
{
  public:
    Host(olagarro::oui::Application& application, std::vector<std::string> arguments, bool desktop_window);

    // Runs the script at `path`; one that fails says so on the terminal and makes the exit code 1.
    void run(const std::filesystem::path& path);
    // Whether there is still something to wait for: a window, or work the script started.
    [[nodiscard]] bool keeps_running() const;
    [[nodiscard]] int exit_code() const;
    // What was printed while a window held the terminal, to show once it is given back.
    [[nodiscard]] std::string take_held_output();

  private:
    using Value = olagarro::oui::script::Value;
    using Arguments = std::vector<Value>;

    // While a window holds the terminal, what is printed waits until it closes.
    [[nodiscard]] bool holds_terminal() const;
    void print(const std::string& text);
    // A program's output: on the terminal, or held while a window holds it.
    void print_output(std::string_view text, bool error);
    void report(std::string_view error);
    // Calls a script's function, saying so when it throws.
    void call(const Value& function, const Arguments& arguments = {});
    // The script's settings, read from its file the first time.
    [[nodiscard]] olagarro::oui::Settings& settings();

    // The widgets scripts make, by the number they know each by.
    template <typename Kind> int adopt(std::unique_ptr<Kind> widget);
    template <typename Kind> Kind& widget(int id, std::string_view what);
    // Hands a widget over to whatever it goes in; each goes in one place only.
    std::unique_ptr<olagarro::oui::Widget> take(int id);
    // The function called when `event` happens to the widget, connected the first time it is asked for.
    void on(int id, const std::string& event, const Value& function);

    // Opens the one window, in the terminal or on the desktop, the first time a script asks.
    void open_window(const std::string& title);
#ifdef OLAGARRO_OUI_RUN_WINDOW
#endif
    void set_window_content(int id);
    // Puts the menu bar or the status bar where it goes in the window, at once or once the window opens.
    void place_bars();
    // On the desktop, the window as big as what it shows asks, its bars and a described window's width with it.
    void fit_window();
    // The menu bar the window has, the script's or one with no menus of its own that shows nothing, for the menus a
    // right click opens.
    olagarro::oui::MenuBar& menu_bar();
    // The entries of a menu as a script gives them, each a command of the application.
    std::vector<olagarro::oui::MenuItem> menu_items(const Value& entries);
    // A right click on a widget with a menu of its own, or in one: that menu, where the pointer is.
    void open_context_menu(const olagarro::oui::Event& event);
    // Where the tree's chosen node is, as the places from the top, [1, 0]; null with none.
    [[nodiscard]] Value tree_path(const olagarro::oui::TreeView& tree);
    // Opens the window as a description's Window says: its title, and what it holds.
    void open_described_window(int id);
    // A description built, its texts as written: what prelude.js makes oui.ui of. Where it came from starts what is
    // said when it is wrong.
    [[nodiscard]] Value describe(std::string_view description, const std::string& source);
    void quit();

    // What prelude.js builds `oui` on, as `__oui`.
    void define(std::string_view name, olagarro::oui::script::ScriptEngine::Function function);
    void define_everything();

    struct Box
    {
        bool vertical{true};
        std::vector<olagarro::oui::Length> lengths;
    };

    olagarro::oui::Application& m_application;
    std::vector<std::string> m_arguments;
    bool m_in_desktop_window{false};
    // Whether the script chose the colours, which the window then keeps rather than its own.
    bool m_theme_chosen{false};
    // The theme the script chose, by its id; nothing until it chooses.
    std::string m_theme_name;
#ifdef OLAGARRO_OUI_RUN_WINDOW
    // Whether a window opens on the desktop, as --window asks: only the build that can open one has it.
    bool m_desktop_window{false};
    olagarro::oui::DesktopWindow* m_screen{};
#endif
    olagarro::oui::Window* m_window{};
    // The window's parts, one over the other: the menu bar, what the script shows, the status bar.
    olagarro::oui::Container* m_top{};
    olagarro::oui::Container* m_middle{};
    olagarro::oui::Container* m_bottom{};
    // The bars the script asked for, until the window is there to put them in.
    std::unique_ptr<olagarro::oui::Widget> m_pending_menu;
    std::unique_ptr<olagarro::oui::Widget> m_pending_status;
    olagarro::oui::StatusBar* m_status{};
    olagarro::oui::MenuBar* m_menu_bar{};
    // The menus a right click on a widget opens, by the widget's number.
    std::map<int, std::vector<olagarro::oui::MenuItem>> m_context_menus;
    // The size a described Window asked for, if any.
    olagarro::oui::Size m_described_size{};
    std::map<int, olagarro::oui::Widget*> m_widgets;
    // Made and not yet placed in anything.
    std::map<int, std::unique_ptr<olagarro::oui::Widget>> m_loose;
    // The dialogs open, by their ids.
    std::map<int, olagarro::oui::Window*> m_dialogs;
    std::map<int, Box> m_boxes;
    // The descriptions built, by the number the script knows each by, with the numbers of their widgets by id.
    struct Described
    {
        olagarro::oui::Ui ui;
        std::map<std::string, int> ids;
    };
    std::map<int, Described> m_described;
    // The script's folder, which the descriptions it reads are found from.
    std::filesystem::path m_script_folder;
    // The script's name without its extension, which its settings are kept under.
    std::string m_script_name;
    // What the script keeps from one run to the next, read when it first asks.
    std::unique_ptr<olagarro::oui::Settings> m_settings;
    std::map<std::pair<int, std::string>, Value> m_handlers;
    int m_next_id{};
    bool m_quit{false};
    int m_exit_code{};
    std::string m_held_output;
    // Last, so they go first: what scripts hold dies before the widgets they reach, and the work they started
    // before the engine its callbacks run in.
    olagarro::oui::script::ScriptEngine m_engine;
    std::unique_ptr<olagarro::oui::script::SystemApi> m_system;
};
