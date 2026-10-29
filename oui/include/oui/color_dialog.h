#pragma once

#include <oui/application.h>
#include <oui/color.h>

#include <functional>
#include <optional>
#include <string>

namespace olagarro::oui
{

// A modal asking for a colour: a square of saturation and value beside a bar of hue, the colour there was beside the
// one picked, and red, green and blue from 0 to 255, hue from 0 to 360, saturation and value from 0 to 100 and
// hexadecimal ("#rrggbb") to type. The square and the bar take a click, a drag, and the arrows while focused; the
// square starts focused. Returns the colour accepted with Enter or OK, or nothing when cancelled. `changed`, when
// given, hears the colour each time it changes, so what asked can show it as it goes.
[[nodiscard]] std::optional<Color> ask_color(Application& application, const std::string& title, Color start,
                                             const std::function<void(Color)>& changed = {});

} // namespace olagarro::oui
