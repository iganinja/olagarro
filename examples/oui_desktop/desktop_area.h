#pragma once

#include <oui/color.h>
#include <oui/desktop.h>
#include <oui/widget.h>

#include <signalslot/signalslot.h>

#include <functional>
#include <vector>

// What fills the screen round the windows, unless told otherwise.
inline constexpr olagarro::oui::Color desktop_colour{0, 128, 128};

// The area the windows are in. Each window is a title row and, under it, the program's cells; the title drags it, its
// bottom right corner sizes it, and its buttons leave it only in the task bar, make it fill the desktop (as a double
// click on the title does) and ask the program to close. The window on top has the keys.
//
// F12 and then another key work everywhere, over SSH too, where the other desktop keeps Alt+Tab and the like: Tab the
// next window, t a terminal, x close, m maximize, n minimize, q end the desktop, and F12 again an F12 for the window.
// Alt+Tab goes to the next window, Alt+F4 closes the one on top, Alt+F9 leaves it in the task bar, Alt+F10 maximizes
// it or puts it back, Ctrl+Alt+T opens a terminal and Ctrl+Alt+Backspace ends the desktop.
class DesktopArea final : public olagarro::oui::Widget
{
  public:
    // `cells` for a desktop in a terminal, whose windows are framed in characters.
    DesktopArea(olagarro::oui::DesktopServer& server, bool cells);

    // The windows from the bottom one to the top one, which has the keys.
    [[nodiscard]] std::vector<int> windows() const;
    // The window with the keys, the top one; 0 when there is none, or all are only in the task bar.
    [[nodiscard]] int focused() const;
    // Puts a window on top, with the keys, back from the task bar if it was only there.
    void raise(int id);
    // Leaves a window only in the task bar, under all the others, and the keys to the one on top.
    void minimize(int id);
    // Makes a window fill the desktop, or puts it back where it was.
    void toggle_maximized(int id);
    void close_focused();
    // The next window down comes to the top, as Alt+Tab does.
    void cycle();
    // Whether F12 was pressed and the desktop waits for what it is to do.
    [[nodiscard]] bool waiting_for_desktop_key() const;

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override;
    void arrange(const olagarro::oui::Rect& bounds) override;
    void paint(olagarro::oui::Painter& painter, const olagarro::oui::Theme& theme) const override;
    [[nodiscard]] bool is_focusable() const override;
    // Every key goes to the window on top, but the desktop's own.
    [[nodiscard]] bool claims_key(const olagarro::oui::Event& event) const override;
    bool handle_event(const olagarro::oui::Event& event) override;
    // Over a program's window, or dragging in it, the pointer the program asked for.
    [[nodiscard]] olagarro::oui::Pointer pointer_at(olagarro::oui::Point point) const override;

    // What fills the screen round the windows.
    olagarro::oui::Color background{desktop_colour};
    // What Ctrl+Alt+T and Ctrl+Alt+Backspace do.
    std::function<void()> open_terminal;
    std::function<void()> quit;

  private:
    struct Frame
    {
        int id;
        olagarro::oui::Rect bounds;
        bool minimized{false};
        bool maximized{false};
        // Where a maximized window goes back to.
        olagarro::oui::Rect restored{};
    };

    enum class Drag
    {
        none,
        move,
        resize,
        content
    };

    enum class TitleButton
    {
        none,
        minimize,
        maximize,
        close
    };

    static constexpr double minimum_width{20.0};
    static constexpr double minimum_height{5.0};

    // The desktop's own keys, which no window gets.
    bool own_key(const olagarro::oui::Event& event);
    // Where the program's cells are: under the title row, and in characters also inside the frame.
    [[nodiscard]] olagarro::oui::Rect content(const olagarro::oui::Rect& frame) const;
    // The button of the title row a click falls on: from the right, close, maximize and minimize, two cells each in
    // pixels and three ("[■]") before the corner in characters.
    [[nodiscard]] TitleButton title_button_at(const olagarro::oui::Rect& frame, double x) const;
    [[nodiscard]] std::vector<Frame>::iterator find(int id);
    // Runs `change` and, when it changes which window has the keys, tells both.
    template <typename Change> void telling_focus(Change change);
    // The window under a cell, the topmost.
    [[nodiscard]] const Frame* frame_at(double x, double y) const;
    // Within the desktop, and no bigger: its title row always there to drag it back by.
    [[nodiscard]] olagarro::oui::Rect keep_inside(olagarro::oui::Rect frame) const;
    void configure(int id);
    // A new window, on top: as big as most programs want, each a little lower and to the right of the one before.
    void add(int id);
    void remove(int id);
    void forward(int id, olagarro::oui::MouseEvent mouse);
    bool handle_mouse(const olagarro::oui::MouseEvent& mouse);

    olagarro::oui::DesktopServer& m_server;
    bool m_cells;
    bool m_desktop_key{false};
    std::vector<olagarro::signal_slot::ScopedConnection> m_connections;
    std::vector<Frame> m_frames;
    Drag m_drag{Drag::none};
    int m_drag_id{};
    double m_grab_x{};
    double m_grab_y{};
    olagarro::oui::Rect m_start{};
};
