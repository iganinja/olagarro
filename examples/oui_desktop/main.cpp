// oui_desktop: OUI's own desktop, on the whole screen with nothing under it but Linux: the windows of the programs
// started in it, which it moves, sizes and gives the keys to, and a task bar along the bottom that starts them and
// goes from one to another.
#include "desktop_area.h"
#include "launcher.h"
#include "task_bar.h"

#include <oui/oui.h>
#include <oui/desktop.h>
#ifdef OLAGARRO_OUI_DESKTOP_PIXELS
#include <oui/graphics/graphical_painter.h>
#endif
#ifdef OLAGARRO_OUI_DESKTOP_DRM
#include <oui/graphics/drm_backend.h>
#endif
#ifdef OLAGARRO_OUI_DESKTOP_WINDOW
#include <oui/graphics/window_backend.h>
#endif

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <signal.h>
#include <unistd.h>

using namespace olagarro;

int main(int argument_count, char* const* const arguments)
{
    const auto words{oui::program_arguments(argument_count, arguments)};
    const auto has{[&words](std::string_view word)
                   { return std::find(words.begin(), words.end(), word) != words.end(); }};
    if(has("--help") || words.empty())
    {
        std::fputs("OUI Desktop\n\n"
                   "Usage: oui_desktop --drm | --window | --terminal\n\n"
                   "The windows of the programs started in it, and a task bar to start them and go from one to\n"
                   "another. Ctrl+Alt+T opens a terminal, Alt+Tab goes to the next window, Alt+F4 closes the one\n"
                   "with the keys, Alt+F9 leaves it only in the task bar, Alt+F10 maximizes it or puts it back,\n"
                   "and Ctrl+Alt+Backspace ends the desktop. F12 and then a key do the same where another desktop\n"
                   "keeps those keys, as over SSH: Tab, t, x, m, n and q; F12 twice is an F12 for the window.\n\n"
                   "Options:\n"
                   "  --help          Show this help text.\n"
                   "  --terminal      Show in the terminal it is started from, in characters: over SSH too.\n"
#ifdef OLAGARRO_OUI_DESKTOP_DRM
                   "  --drm           Take the whole screen, with no other desktop: for a console.\n"
#endif
#ifdef OLAGARRO_OUI_DESKTOP_WINDOW
                   "  --window        Show in a window of another desktop, to try it.\n"
#endif
                   ,
                   stdout);
        return words.empty() ? 2 : 0;
    }
    // OUI's own texts, in the user's language; the program has none of its own.
    oui::install_language(oui::system_language(), {});
    std::unique_ptr<oui::Backend> backend;
#ifdef OLAGARRO_OUI_DESKTOP_PIXELS
    oui::GraphicalStyle style{oui::classic_style()};
    // The desktop's colour goes on past its last whole cell, to the screen's edge.
    style.desktop = desktop_colour;
#endif
    const auto in_characters{has("--terminal")};
    try
    {
        if(in_characters)
        {
            backend = oui::terminal_backend();
        }
        else if(has("--window"))
        {
#ifdef OLAGARRO_OUI_DESKTOP_WINDOW
            oui::WindowOptions options;
            options.title = "OUI Desktop";
            options.style = style;
            backend = oui::window_backend(options);
#endif
        }
        else if(has("--drm"))
        {
#ifdef OLAGARRO_OUI_DESKTOP_DRM
            oui::DrmOptions options;
            options.style = style;
            if(const auto* const layout{std::getenv("OUI_KEYBOARD")})
            {
                options.keyboard = layout;
            }
            backend = oui::drm_backend(options);
#endif
        }
        if(!backend)
        {
            std::fputs("oui_desktop: this build cannot show that way\n", stderr);
            return 2;
        }
    }
    catch(const std::exception& error)
    {
        std::fprintf(stderr, "oui_desktop: %s\n", error.what());
        return 1;
    }
    // Ended programs go without anyone waiting for them.
    ::signal(SIGCHLD, SIG_IGN);
    oui::Application application{std::move(backend)};
#ifdef OLAGARRO_OUI_DESKTOP_PIXELS
    application.set_theme(in_characters ? oui::TurboCTheme::make() : oui::classic_theme());
#else
    application.set_theme(oui::TurboCTheme::make());
#endif
    // Ctrl and the wheel go to the program under the pointer, which makes itself bigger or smaller.
    application.set_wheel_zooms(false);

    const auto* const runtime{std::getenv("XDG_RUNTIME_DIR")};
    const std::filesystem::path socket{
        std::filesystem::path{runtime != nullptr && *runtime != '\0' ? runtime : "/tmp"} /
        ("oui-desktop-" + std::to_string(::getpid()))};
    std::unique_ptr<oui::DesktopServer> server;
    try
    {
        server = std::make_unique<oui::DesktopServer>(
            application, socket, in_characters ? oui::DesktopFrames::cells : oui::DesktopFrames::pixels);
    }
    catch(const std::exception& error)
    {
        application.quit();
        std::fprintf(stderr, "oui_desktop: %s\n", error.what());
        return 1;
    }
    std::error_code ignored;
    const auto folder{std::filesystem::read_symlink("/proc/self/exe", ignored).parent_path()};
    auto launchers{launchers_beside(folder)};
    const auto terminal{launchers.empty() || launchers.front().title != "Terminal" ? std::vector<std::string>{}
                                                                                   : launchers.front().command};

    auto window{oui::window("OUI Desktop")};
    window->set_border(oui::WindowBorder::none);
    auto layout{oui::dock_layout()};
    auto desktop{std::make_unique<DesktopArea>(*server, in_characters)};
    auto& desktop_reference{*desktop};
    layout->add(oui::Dock::bottom, std::make_unique<TaskBar>(desktop_reference, *server, std::move(launchers)));
    layout->add(oui::Dock::fill, std::move(desktop));
    window->add(std::move(layout));
    application.set_window(std::move(window));
    application.focus(desktop_reference);

    desktop_reference.quit = [&application] { application.quit(); };
    if(!terminal.empty())
    {
        desktop_reference.open_terminal = [&terminal, &socket] { launch(terminal, socket); };
    }
    // Every new frame and title shows.
    server->on_changed([](int) {});
    // A program answers after the pointer came over it: what it asks for shows then, without the pointer moving.
    server->on_pointer([&application](int) { application.update_pointer(); });
    application.exec();
    return 0;
}
