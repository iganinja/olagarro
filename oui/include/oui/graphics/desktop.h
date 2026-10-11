#pragma once

#include <oui/desktop.h>
#include <oui/graphics/graphical_painter.h>
#include <oui/graphics/pixel_backend.h>

#include <memory>

namespace olagarro::oui
{

// A window on OUI's desktop, whichever it shows: a PixelBackend in `style` on a desktop of pixels, a backend of
// cells on one in a terminal. It throws when there is no desktop to show in.
[[nodiscard]] std::unique_ptr<Backend> desktop_backend(DesktopClientOptions options, GraphicalStyle style);

} // namespace olagarro::oui
