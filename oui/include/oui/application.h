#pragma once

#include <oui/backend.h>
#include <oui/clipboard.h>
#include <oui/command.h>
#include <oui/widget.h>
#include <oui/window.h>

#include <signalslot/signalslot.h>

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace olagarro::oui
{

class ModalLayer final : public Container
{
  public:
    ModalLayer(std::unique_ptr<Widget>&& background, std::unique_ptr<Window>&& dialog);
    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void arrange(const Rect& bounds) override;
    void paint(Painter& painter, const Theme& theme) const override;
    bool handle_event(const Event& event) override;
    [[nodiscard]] Widget* hit_test(int x, int y) override;
    [[nodiscard]] bool is_overlay() const override;
    [[nodiscard]] Widget* keyboard_target() override;
    void collect_focusable(std::vector<Widget*>& widgets) override;
    [[nodiscard]] std::unique_ptr<Widget> release_background();
    [[nodiscard]] Window& dialog();
    [[nodiscard]] std::unique_ptr<Window> release_dialog();

  private:
    Window* m_dialog{};
};

class Application;

// A callback the application runs every interval, between events, until the timer is stopped or
// destroyed. Move-only: whoever holds it decides how long the callback runs.
class Timer
{
  public:
    Timer() = default;
    ~Timer();
    Timer(Timer&& other) noexcept = default;
    Timer& operator=(Timer&& other) noexcept;
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    void stop();
    [[nodiscard]] bool is_active() const;

  private:
    friend class Application;
    struct State
    {
        std::chrono::milliseconds interval;
        std::chrono::steady_clock::time_point due;
        std::function<void()> callback;
    };
    explicit Timer(std::shared_ptr<State> state);

    std::shared_ptr<State> m_state;
};

// What a key mode does after a key: goes on, ends, or ends and lets the key do what it does outside the mode,
// such as a letter typed into the document once the mode is left.
enum class KeyModeAnswer
{
    keep,
    end,
    end_and_pass
};

class Application
{
  public:
    // An application on the terminal it runs in.
    Application();
    explicit Application(std::unique_ptr<Backend> backend, Theme theme = {});
    // Takes its widgets down first, while the rest of it is whole: one may still post work or reach the
    // application as it goes, as a terminal's thread does when its program ends.
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    // Shows the application on another backend from now on, such as a program that runs without a screen
    // until it opens a window. Safe while other threads post work.
    void set_backend(std::unique_ptr<Backend> backend);
    void set_content(std::unique_ptr<Widget>&& content);
    void set_window(std::unique_ptr<Window>&& window);
    [[nodiscard]] DialogResult show_modal(std::unique_ptr<Window>&& window);
    // As show_modal, and the window is back in `window` once it closes, to be shown again with what it holds as it
    // was left.
    [[nodiscard]] DialogResult show_modal_and_keep(std::unique_ptr<Window>& window);
    using PlacementLoader = std::function<std::optional<WindowPlacement>(std::string_view key)>;
    using PlacementSaver = std::function<void(std::string_view key, const WindowPlacement& placement)>;
    // Lets modal windows with a placement key reopen where the user left them.
    void set_window_placement_store(PlacementLoader load, PlacementSaver save);
    // The key that starts placing the dialog on top from the keyboard: the arrows move it, Ctrl and the arrows
    // resize it, Enter keeps it and Escape puts it back, as a line at the bottom of the screen says. Ctrl+L
    // unless set; terminals send it as a byte of its own, so it reaches the program everywhere.
    void set_placement_shortcut(Shortcut shortcut);
    // A mode that takes the keyboard for a while, such as placing a window or picking a side: every key goes to
    // `handler` until it answers that the mode ends, and `hint`, on the bottom line of the screen over whatever
    // is there, says what the keys do. Mouse events are dropped meanwhile.
    void start_key_mode(std::string hint, std::function<KeyModeAnswer(const Event& key)> handler);
    [[nodiscard]] bool in_key_mode() const;
    // A new hint for the key mode in progress, when what the keys can do changes along the way.
    void set_key_mode_hint(std::string hint);
    void push_overlay(std::unique_ptr<Window>&& window);
    void pop_overlay();
    // A popup over everything, such as suggestions at the cursor, that leaves the focus where it is: it sees the
    // keys and the mouse first and keeps those it handles, the rest going on as if it were not there; a press
    // outside it closes it. It is arranged in the whole screen and places itself in it. One at a time: showing
    // another closes it, and so does a dialog. `closed` is told when it goes, however it goes.
    void show_popup(std::unique_ptr<Widget> popup, std::function<void()> closed = {});
    // Closes the popup, if there is one; safe from inside its own handling of an event.
    void close_popup();
    [[nodiscard]] Widget* popup() const;
    void set_theme(Theme theme);
    void reset_focus();
    void focus(Widget& widget);
    // Gives the focus to what takes it nearest on the side of what has it that (dx, dy) points to, (1, 0) for the
    // right, as across panels and the parts of a tab; in a dialog, within it. False, and the focus stays, when nothing
    // is there.
    bool focus_toward(int dx, int dy);
    [[nodiscard]] const Theme& theme() const;
    [[nodiscard]] CommandRegistry& commands();
    // The system clipboard, which also offers copied text to the terminal through the backend.
    [[nodiscard]] Clipboard& clipboard();
    // What the application is called where the backend shows that, as over its window on a desktop.
    void set_title(std::string_view title);
    [[nodiscard]] const CommandRegistry& commands() const;
    [[nodiscard]] Widget* focused() const;
    void set_event_observer(std::function<void(const Event&)> observer);
    void post(std::function<void()> callback);
    // Runs `callback` on the application's thread every `interval`, first one interval from now, until the
    // returned timer is stopped or destroyed. A timer due while the application is busy runs once when it
    // is free, not once per interval missed.
    [[nodiscard]] Timer start_timer(std::chrono::milliseconds interval, std::function<void()> callback);
    // Fires when the terminal window gains or loses the keyboard focus, where the backend can tell.
    template <typename Callback> olagarro::signal_slot::Connection on_terminal_focus_changed(Callback&& callback)
    {
        return m_terminal_focus_changed.connect(std::forward<Callback>(callback));
    }
    // Fires once when a ShutdownEvent arrives: the terminal went away or the system asked the program to
    // stop. There is nobody to ask anything, so this is where an application keeps what must not be lost,
    // without dialogs. Every loop then ends: an open dialog returns DialogResult::cancelled, and exec()
    // returns.
    template <typename Callback> olagarro::signal_slot::Connection on_shutdown(Callback&& callback)
    {
        return m_shutdown.connect(std::forward<Callback>(callback));
    }
    [[nodiscard]] bool is_shutting_down() const;
    // Fires when the modifier keys held change, where the backend can tell: see reports_modifiers().
    template <typename Callback> olagarro::signal_slot::Connection on_modifiers_changed(Callback&& callback)
    {
        return m_modifiers_changed.connect(std::forward<Callback>(callback));
    }
    [[nodiscard]] bool reports_modifiers() const;
    // Says what a CloseRequestEvent does, such as asking about unsaved work before quitting; without it the
    // application quits.
    void on_close_requested(std::function<void()> callback);
    // After Ctrl and the wheel made everything bigger or smaller, for a program that keeps the size it was left at.
    void on_zoomed(std::function<void()> callback);
    // Whether Ctrl and the wheel make everything bigger or smaller; yes unless set. A desktop leaves them to the
    // program under the pointer.
    void set_wheel_zooms(bool zooms);
    // Makes everything bigger, a step up, smaller, down, or with 0 the size it starts at, where the backend can, as the
    // commands zoom.in, zoom.out and zoom.reset do, on Ctrl and plus, minus or 0; false where it cannot.
    bool zoom(int step);
    void dispatch_event(const Event& event);
    void run_frame();
    // Asks again what the pointer should look like where it is, as when what is under it changed without its moving:
    // a program on the desktop answers after the pointer came over its window.
    void update_pointer();
    int exec();
    void quit();
    void render();

  private:
    // Shows `window` modally until it closes, and gives it back.
    [[nodiscard]] std::unique_ptr<Window> run_modal(std::unique_ptr<Window>&& window, DialogResult& result);
    // Offers zoom.in, zoom.out and zoom.reset, on their keys, once the backend can make everything bigger or smaller.
    void offer_zoom();
    void paint_key_mode_hint(Painter& painter, Size size) const;
    void paint_tooltip(Painter& painter, Size size) const;
    // A pointer that moves, presses or leaves for the keyboard hides the tooltip; one that rests shows it.
    void follow_pointer(const MouseEvent& mouse);
    // Whether anyone is to hear `event`: a move within the cell the pointer was already in, the middle button and
    // drags with the right or the middle button are only for a widget that takes the raw pointer. It notes the moves
    // it lets through, to tell the next one by.
    [[nodiscard]] bool is_wanted(const Event& event);
    void deliver_event(const Event& event);
    void show_pointer(const Widget* target);
    void hide_tooltip();
    bool edit_focused_field(const Event& event);
    void ensure_focus_is_valid();
    [[nodiscard]] bool has_posted();
    void drop_mouse_capture();
    [[nodiscard]] bool dispatch_to_focused(const Event& event);
    bool dispatch_to_popup(const Event& event);
    // Alt and a letter in `root`, or the letter alone when `plain` and nothing took it: the button it marks is
    // pressed, the check box it marks turned, or what the label it marks names takes the focus.
    bool activate_mnemonic(Widget& root, const Event& event, bool plain);
    // The dialog shown, whose letters come before what has the focus; none outside one.
    [[nodiscard]] Widget* dialog_shown();
    [[nodiscard]] bool process_posted();
    // Runs the timers that are due; true when any ran.
    [[nodiscard]] bool process_timers();
    [[nodiscard]] std::optional<std::chrono::steady_clock::time_point> next_timer_due();
    void focus_next();
    void focus_previous();
    void set_focus(Widget* widget);
    // Tab wraps around the window; an arrow stops at the first or last field, so holding it does not
    // cycle through the dialog.
    void focus_by_offset(int offset, bool wrap = true);

    std::unique_ptr<Backend> m_backend;
    Clipboard m_clipboard{m_backend.get()};
    // Before the content, so it outlives it: widgets such as a menu bar unregister their commands when they
    // are destroyed.
    CommandRegistry m_commands;
    std::unique_ptr<Widget> m_content;
    Widget* m_focused{};
    std::weak_ptr<void> m_focused_lifetime;
    Widget* m_mouse_capture{};
    std::unique_ptr<Widget> m_popup;
    std::function<void()> m_popup_closed;
    // Popups closed while an event was being handled, perhaps their own, kept until the next frame.
    std::vector<std::unique_ptr<Widget>> m_closed_popups;
    std::weak_ptr<void> m_mouse_capture_lifetime;
    // Set when the widget holding a click goes away mid-click; the rest of that click is dropped.
    bool m_discard_mouse_until_release{false};
    // The last press, to tell double and triple clicks from separate ones.
    struct LastPress
    {
        int x{-1};
        int y{-1};
        bool right{false};
        int clicks{};
        std::chrono::steady_clock::time_point time{};
    };
    LastPress m_last_press;
    std::function<void(const Event&)> m_event_observer;
    olagarro::signal_slot::Signal<bool> m_terminal_focus_changed;
    olagarro::signal_slot::Signal<ShutdownEvent::Reason> m_shutdown;
    std::function<void()> m_close_requested;
    std::function<void()> m_zoomed;
    bool m_wheel_zooms{true};
    olagarro::signal_slot::Signal<ModifiersEvent> m_modifiers_changed;
    bool m_shutting_down{false};
    PlacementLoader m_load_placement;
    PlacementSaver m_save_placement;
    Shortcut m_placement_shortcut{U'l', true};
    struct KeyMode
    {
        std::string hint;
        std::function<KeyModeAnswer(const Event& key)> handler;
    };
    std::optional<KeyMode> m_key_mode;
    // Told apart, so a handler that starts another mode does not end it on returning.
    std::size_t m_key_mode_count{};
    // Hands the event to the key mode, or starts placing the dialog on top; true when it took the event.
    [[nodiscard]] bool handle_key_mode(const Event& event);
    void start_placing();
    std::mutex m_post_mutex;
    std::vector<std::function<void()>> m_posted;
    std::vector<std::weak_ptr<Timer::State>> m_timers;
    bool m_is_running{false};
    // The pointer as last shown, so the backend hears only of changes.
    Pointer m_pointer{Pointer::arrow};
    // Where the pointer was last, to ask again what it should look like there.
    std::optional<Point> m_pointer_position;
    // The cell, and the place in it, of the last move let through.
    struct Motion
    {
        Point cell;
        int offset{};
    };
    std::optional<Motion> m_last_motion;
    bool m_needs_render{true};
    std::size_t m_modal_depth{};
    Theme m_theme{};
    std::unique_ptr<Widget> m_previous_content;
    Widget* m_previous_focus{};
    std::weak_ptr<void> m_previous_focus_lifetime;
    struct Tooltip
    {
        std::string text;
        Point at;
        // The columns the pointer covers from its tip's, which the tooltip starts after.
        int pointer_columns{2};
    };
    std::optional<Tooltip> m_tooltip;
    Timer m_tooltip_timer;
};

} // namespace olagarro::oui
