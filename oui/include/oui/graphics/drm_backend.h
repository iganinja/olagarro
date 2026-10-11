#pragma once

#include <oui/graphics/graphical_painter.h>
#include <oui/graphics/pixel_backend.h>

#include <memory>
#include <string>

namespace olagarro::oui
{

struct DrmOptions
{
    // The graphics card, as the kernel names it.
    std::string card{"/dev/dri/card0"};
    // The keyboard's layout, which is not something a keyboard tells: "us" or "es".
    std::string keyboard{"us"};
    GraphicalStyle style{classic_style()};
};

// The whole screen, straight through the kernel's mode setting (DRM/KMS), with no display server and no graphics
// library: OUI paints each frame in memory, as it always does, and the kernel shows it; the keyboard and the mouse
// come from the kernel's event devices. For a system with no desktop of its own. Linux only; it throws when it
// cannot take the screen, as when another program holds it.
[[nodiscard]] std::unique_ptr<PixelBackend> drm_backend(DrmOptions options = {});

} // namespace olagarro::oui
