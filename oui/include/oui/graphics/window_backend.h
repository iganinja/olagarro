#pragma once

#include <oui/graphics/graphical_painter.h>
#include <oui/graphics/icon.h>
#include <oui/graphics/pixel_backend.h>

#include <memory>
#include <optional>
#include <string>

namespace olagarro::oui
{

// Where a window is on the desktop and how big, in the desktop's coordinates. When it is maximized, the position
// and size are those it goes back to.
struct WindowGeometry
{
    std::optional<Point> position;
    Size size{1280, 800};
    bool maximized{false};
};

struct WindowOptions
{
    std::string title{"OUI"};
    // Where it opens, such as where it was left: without a position, or one on no display there is now, the
    // desktop places it. The person can move and resize it.
    WindowGeometry geometry;
    GraphicalStyle style{classic_style()};
    // What the desktop shows for the program, as in its task bar; without one, the desktop's own default.
    std::optional<WindowIcon> icon;
};

// A desktop window that shows the application in a graphical style, through SDL3.
class DesktopWindow : public PixelBackend
{
  public:
    using PixelBackend::PixelBackend;
    // Where it is now, to open there the next time.
    [[nodiscard]] virtual WindowGeometry geometry() const = 0;
    // Makes it as big as content of `cells` needs, its frame and title around it, as far as the display it is on
    // has room, and puts it in the middle of that display.
    virtual void fit_content(Size cells) = 0;
};

// Opens one; it throws when no window can be opened, as without a display.
[[nodiscard]] std::unique_ptr<DesktopWindow> window_backend(WindowOptions options = {});

} // namespace olagarro::oui
