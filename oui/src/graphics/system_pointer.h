#pragma once

namespace olagarro::oui
{

// The size of the system's pointer in pixels: XCURSOR_SIZE where X's cursors are, Windows' own, 24 when nothing says.
[[nodiscard]] int system_pointer_size();

} // namespace olagarro::oui
