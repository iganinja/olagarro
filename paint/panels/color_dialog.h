#pragma once

#include <paint/color.h>

#include <oui/application.h>

#include <functional>
#include <optional>
#include <string>

namespace olagarro::paint
{

// Asks for a colour, as Paint.NET's Colors window: a square of saturation and value and a bar of hue, numbers for red,
// green, blue, alpha, hue, saturation and value, hexadecimal, and grey as a 16-bit value, for heightmaps; none when
// cancelled. `changed`, when given, hears the colour each time it changes, so what uses it can show it. Unlike OUI's
// `oui::ask_color`, it has alpha, and keeps a colour deeper than 8 bits a channel.
[[nodiscard]] std::optional<Color> ask_color(oui::Application& application, const std::string& title,
                                             const Color& start, const std::function<void(const Color&)>& changed = {});

} // namespace olagarro::paint
