#pragma once

#include <oui/cell_buffer.h>
#include <oui/event.h>

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace olagarro::oui
{

class Backend
{
  public:
    virtual ~Backend() = default;

    [[nodiscard]] virtual Size size() const = 0;
    virtual std::optional<Event> poll_event() = 0;
    virtual void present(const CellBuffer& buffer) = 0;
    // Paints a frame: `paint` draws everything on the painter it is given, in cells. A backend of cells, as a
    // terminal is, paints into a CellBuffer and presents it; a graphical one paints pixels with its own style.
    virtual void render(const std::function<void(Painter&)>& paint);
    virtual void reset() {}
    // Blocks until input may be ready, wake() is called, `deadline` passes, or the backend's own deadline
    // does (such as resolving a lone Escape). The application calls it only when it has nothing left to
    // do, so an idle program sleeps instead of polling; `deadline` is when its next timer is due, or none.
    // Backends that never block, like test doubles, return at once.
    virtual void wait_for_input(std::optional<std::chrono::steady_clock::time_point> deadline)
    {
        static_cast<void>(deadline);
    }
    // Whether it sends ModifiersEvent, so a program can act when a modifier is let go.
    [[nodiscard]] virtual bool reports_modifiers() const { return false; }
    // Whether `key` is held down now, where the backend can tell, as a window can, for what a key does while held,
    // such as Space dragging a canvas; a terminal cannot, and says no.
    [[nodiscard]] virtual bool is_key_held(Key key) const
    {
        static_cast<void>(key);
        return false;
    }
    // Ends a wait_for_input in progress, or the next one; safe to call from any thread.
    virtual void wake() {}
    // Offers copied text to whatever shows the application, where that can take it: a terminal backend
    // sends it as OSC 52. Others ignore it.
    virtual void set_clipboard_text(std::string_view text) { static_cast<void>(text); }
    // Whether it keeps the clipboard itself, for every program it shows, as OUI's desktop does: then the clipboard
    // is its, through set_clipboard_text and clipboard_text, and not the system's.
    [[nodiscard]] virtual bool keeps_clipboard() const { return false; }
    [[nodiscard]] virtual std::string clipboard_text() const { return {}; }
    // What the application is called where that shows, as over its window and in a task bar; most show nothing.
    virtual void set_title(std::string_view title) { static_cast<void>(title); }
    // Whether text and cells can be made bigger or smaller: in a window or on a screen of pixels, not in a terminal,
    // which has its own keys for it.
    [[nodiscard]] virtual bool zooms() const { return false; }
    // Makes them bigger, a step up, or smaller, down, as Ctrl and the wheel or plus and minus ask in every program;
    // false where they cannot be.
    virtual bool zoom_by(int step)
    {
        static_cast<void>(step);
        return false;
    }
    // Back to the size they start at.
    virtual bool zoom_reset() { return false; }
    // Shows the pointer as `pointer`, in a window or on a screen of pixels; a terminal keeps its own.
    virtual void set_pointer(Pointer pointer) { static_cast<void>(pointer); }
};

} // namespace olagarro::oui
