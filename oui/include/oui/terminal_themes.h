#pragma once

#include <oui/theme.h>

#include <string>
#include <vector>

namespace olagarro::oui
{

struct TerminalTheme
{
    // Stays the same in every language, to keep in settings.
    std::string id;
    std::string name;
    Theme theme;
};

// The looks of OUI in characters, the default first: Turbo C++'s blue, then a dark one, a light one, one without
// colours, those of two old monitors, amber and green phosphor, and a file manager's, Commander. They take their colours from the sixteen of a text console, so they read the same on the Linux console
// and over SSH.
[[nodiscard]] std::vector<TerminalTheme> terminal_themes();

} // namespace olagarro::oui
