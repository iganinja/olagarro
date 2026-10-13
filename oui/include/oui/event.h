#pragma once

#include <oui/geometry.h>

#include <optional>
#include <variant>

namespace olagarro::oui
{

enum class Key
{
    tab,
    shift_tab,
    enter,
    space,
    backspace,
    delete_,
    escape,
    left,
    right,
    up,
    down,
    home,
    end,
    page_up,
    page_down,
    insert_,
    f1,
    f2,
    f3,
    f4,
    f5,
    f6,
    f7,
    f8,
    f9,
    f10,
    f11,
    f12
};

struct KeyEvent
{
    Key key;
    bool control{false};
    bool alt{false};
    bool shift{false};
};

struct ResizeEvent
{
    int width;
    int height;
};

struct TextEvent
{
    char32_t codepoint;
    bool control{false};
    bool alt{false};
    bool shift{false};
};

// What the mouse pointer looks like, as a widget under it asks: an arrow, a line it can drag, between columns or
// between rows, or what a canvas shows for what it does there. A terminal has the one it has.
enum class Pointer
{
    arrow,
    resize_columns,
    resize_rows,
    crosshair,
    // An open hand, over something that can be grabbed and moved, and the hand closed while it moves it.
    hand,
    grabbing,
    // Arrows four ways, over something that moves when dragged.
    move,
    text,
    // Nothing: the widget draws its own.
    none,
};

struct MouseEvent
{
    int x;
    int y;
    bool pressed;
    int wheel_delta{0};
    bool dragging{false};
    bool released{false};
    bool hover{false};
    bool right{false};
    bool shift{false};
    bool control{false};
    bool alt{false};
    // On a press: 1 for a single click, 2 for a double click, 3 for a triple click. The application
    // counts them, so every backend gets double clicks without knowing about them.
    int clicks{1};
    // Where in its cell the pointer is, from the cell's left, in the units text_metrics() measures; 0 in a
    // terminal, where a cell is the finest place.
    int cell_offset{0};
    // Where the pointer is in pixels, from the top left of the application's area, where the backend draws in
    // pixels; none in a terminal. A widget finds its own area in pixels through Painter::pixel_bounds.
    std::optional<Point> pixel{};
    // The middle button, on a press or a release. Only a widget that takes the raw pointer hears it.
    bool middle{false};
    // On a move: the right or the middle button is held, as a canvas paints or pans with them. `dragging` is the
    // left button's. Only a widget that takes the raw pointer hears these moves as more than the pointer passing.
    bool right_dragging{false};
    bool middle_dragging{false};
};

// The terminal window gained or lost the keyboard focus, for instance when the user switched away.
struct FocusEvent
{
    bool focused;
};

// The terminal went away, as when its window closes or an SSH connection drops, or the system asked the
// program to stop. Nobody is left to answer a question.
struct ShutdownEvent
{
    enum class Reason
    {
        terminal_closed,
        terminated
    };
    Reason reason;
};

// The person asked to close the program from outside it, as with the close button of its window. Unlike a
// ShutdownEvent, the program can still ask about what is unsaved, or decline.
struct CloseRequestEvent
{
};

// The modifier keys held changed, as when Ctrl is let go. Only a backend that can tell sends it; a terminal cannot.
struct ModifiersEvent
{
    bool control{false};
    bool alt{false};
    bool shift{false};
};

using Event = std::variant<KeyEvent, ResizeEvent, TextEvent, MouseEvent, FocusEvent, ShutdownEvent, CloseRequestEvent,
                           ModifiersEvent>;

} // namespace olagarro::oui
