#pragma once

#include <oui/ui.h>

#include <string_view>
#include <vector>

// OUI Code's windows and panels, described in examples/oui_code/ui, in the order CMake embeds them.
enum class OuiCodeUi
{
    find_in_files,
    preferences,
    replace,
    filter_running,
    about,
    markdown,
    help,
    find_bar,
    tasks,
    script_output,
    interactive,
    main_window,
    key_test,
    interactive_ui,
};

// Defined in the source CMake generates from examples/oui_code/ui, in the order OuiCodeUi names them.
std::vector<std::string_view> embedded_oui_code_ui();

// The window or panel, built from its description and translated.
[[nodiscard]] olagarro::oui::Ui build_oui_code_ui(OuiCodeUi which);
