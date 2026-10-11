#pragma once

#include <string_view>

namespace olagarro::oui
{

// The mark before a checkable entry in menus and lists.
[[nodiscard]] inline std::string_view check_marker(bool checked)
{
    return checked ? "[■] " : "[ ] ";
}

} // namespace olagarro::oui
