// oui_run runs JavaScript, the way node or a shell runs a script: console.log writes to the terminal, and the
// program ends when the script and what it started are done. A script that wants more opens a window with
// oui.window() and builds it from OUI's widgets, in the terminal or, built with the window module and run with
// --window, in a desktop window.
//
//     oui_run script.js [arguments...]
//     oui_run_gui --window script.js [arguments...]
//
// Started with no terminal, from a desktop's menu or a file manager, the windows it opens are desktop windows too.
//
// Scripts are modules: `import {x} from "./tools.js"` reads tools.js from beside the importing file. What they
// can use, besides JavaScript itself, is in oui/oui.d.ts, which tools/extract_api.js makes from the comments marked
// "api" in prelude.js and the library.

#include "host.h"
#include "quiet_backend.h"

#include <oui/oui.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace olagarro;

// Defined in the source CMake generates from examples/oui_run/translations.
std::vector<std::string_view> embedded_oui_run_catalogs();

int main(int argc, char** argv)
{
    auto arguments{oui::program_arguments(argc, argv)};
    auto desktop_window{false};
#ifdef OLAGARRO_OUI_RUN_WINDOW
    if(!arguments.empty() && arguments.front() == "--window")
    {
        desktop_window = true;
        arguments.erase(arguments.begin());
    }
    else
    {
        desktop_window = !oui::started_from_terminal();
    }
#endif
    if(arguments.empty())
    {
#ifdef OLAGARRO_OUI_RUN_WINDOW
        write_to(stderr, "usage: oui_run [--window] script.js [arguments...]\n");
#else
        write_to(stderr, "usage: oui_run script.js [arguments...]\n");
#endif
        return 2;
    }
    const auto script{oui::path_from_utf8(arguments.front())};
    arguments.erase(arguments.begin());
    oui::install_language(oui::system_language(), oui::embedded_translations(embedded_oui_run_catalogs()));
    std::string held_output;
    int exit_code{};
    {
        // No screen until the script asks for one: a script that only prints and runs commands is a program
        // on the terminal like any other.
        oui::Application application{std::make_unique<QuietBackend>()};
        Host host{application, std::move(arguments), desktop_window};
        host.run(script);
        while(host.keeps_running())
        {
            application.run_frame();
        }
        held_output = host.take_held_output();
        exit_code = host.exit_code();
    }
    // Once the window has given the terminal back.
    write_to(stdout, held_output);
    return exit_code;
}
