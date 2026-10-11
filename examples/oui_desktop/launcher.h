#pragma once

#include <filesystem>
#include <string>
#include <vector>

// What the task bar starts: a name and a command.
struct Launcher
{
    std::string title;
    std::vector<std::string> command;
};

// Starts `command` as a program of the desktop, which OUI_DESKTOP tells where to show; it is on its own, in a
// session of its own, and nobody waits for it.
void launch(const std::vector<std::string>& command, const std::filesystem::path& socket);
// The programs beside this one that can show on the desktop.
[[nodiscard]] std::vector<Launcher> launchers_beside(const std::filesystem::path& folder);
