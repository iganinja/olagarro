#pragma once

#include <oui/event.h>
#include <oui/geometry.h>

#include <cstdlib>
#include <vector>

namespace olagarro::oui
{

// What a turn of the wheel by `notches` over a cell is, up when positive: three lines a notch, as a desktop scrolls;
// with Ctrl one step a notch, which makes text bigger or smaller a size at a time.
[[nodiscard]] inline std::vector<MouseEvent> wheel_events(Point cell, int notches, bool control, bool alt, bool shift)
{
    std::vector<MouseEvent> events;
    const auto steps{std::abs(notches) * (control ? 1 : 3)};
    for(int step{}; step < steps; ++ step)
    {
        MouseEvent event{cell.x, cell.y, false, notches > 0 ? 1 : -1};
        event.control = control;
        event.alt = alt;
        event.shift = shift;
        events.push_back(event);
    }
    return events;
}

} // namespace olagarro::oui
