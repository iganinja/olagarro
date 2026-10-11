#pragma once

#include <string>
#include <string_view>

// An oui_run script for the user interface description saved as `file`, its name, to start one from: the file loaded
// from beside the script; each widget with an id wired to what it is usually wanted for, a button's click, a field's
// change, a slot filled; and the window opened. "" for a description that does not read.
[[nodiscard]] std::string script_skeleton(std::string_view description, std::string_view file);

// The same in C++, to put in a program of OUI's: the file built into it by olagarro_embed_files, as the comment over
// the code says, and a function named after it that builds the window, wires its widgets and shows it.
[[nodiscard]] std::string cpp_skeleton(std::string_view description, std::string_view file);

// What a description's code is generated in.
enum class SkeletonLanguage
{
    javascript,
    cpp,
};
