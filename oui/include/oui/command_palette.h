#pragma once

#include <oui/application.h>
#include <oui/translation.h>

#include <string>

namespace olagarro::oui
{

struct CommandPaletteOptions
{
    std::string title{translate("CommandPalette", "Command Palette")};
    std::string placeholder{translate("CommandPalette", "Filter commands")};
    // What follows the title of a command that can be checked, by its state.
    std::string checked{translate("CommandPalette", "(on)")};
    std::string unchecked{translate("CommandPalette", "(off)")};
    // The share of the screen's height the palette opens with; a long list scrolls inside it.
    double height_share{0.8};
    // Remembers where the user left the palette, through Application::set_window_placement_store.
    std::string placement_key;
};

// The application's commands in a quick pick, filtered fuzzily by title; the chosen one runs once the palette
// closes. Each shows its shortcut and, for one that can be checked, its state.
void show_command_palette(Application& application, const CommandPaletteOptions& options = {});

} // namespace olagarro::oui
