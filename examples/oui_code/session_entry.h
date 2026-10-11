#pragma once

#include <string>
#include <string_view>
#include <vector>

// A tab as the session keeps it, one entry of a list in the settings: a file's path, as it is; or what the tab is,
// as "calculator:", then what it needs to come back, apart by tabs. A list of the settings is split at ';' and a
// setting ends at its line, so the parts are written with those, '%' and tabs as %3B, %0A, %25, %09 and the rest.

struct SessionTab
{
    // "" for a file, whose path is its only part; otherwise the mark before the colon, as "calculator".
    std::string kind;
    std::vector<std::string> parts;
};

[[nodiscard]] std::string session_entry(const SessionTab& tab);
[[nodiscard]] SessionTab session_tab(std::string_view entry);
// The kinds of tab a session keeps besides files.
[[nodiscard]] const std::vector<std::string_view>& session_kinds();
