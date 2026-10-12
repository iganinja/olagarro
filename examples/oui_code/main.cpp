// OUI Code: a text editor for the terminal, a window, a console without a desktop or OUI's own desktop. This reads
// the command line, picks where OUI Code shows, and hands over to the Workbench, which is OUI Code's window and
// everything in it.
#include "resources.h"
#include "self_tests.h"
#include "session.h"
#include "workbench.h"

#include <oui/oui.h>
#include <oui/settings.h>
#ifdef OLAGARRO_OUI_CODE_WINDOW
#include <oui/graphics/window_backend.h>
#endif
#ifdef OLAGARRO_OUI_CODE_DRM
#include <oui/graphics/drm_backend.h>
#endif
#ifdef OLAGARRO_OUI_CODE_DESKTOP
#include <oui/desktop.h>
#endif
#ifdef OLAGARRO_OUI_CODE_DESKTOP_PIXELS
#include <oui/graphics/desktop.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

using namespace olagarro;

int main(int argument_count, char* const* const arguments)
{
    oui::CrashReporterOptions crash_options;
    crash_options.product_name = "OUI Code";
    oui::install_crash_reporter(crash_options);
    const oui::CommandLine command_line{oui::program_arguments(argument_count, arguments)};
    if(command_line.has_option("help"))
    {
        std::fputs("OUI Code\n\n"
                   "Usage: oui_code [options] [folder] [file...]\n"
                   "       oui_code [options] --diff left-file right-file\n\n"
                   "Options:\n"
                   "  --help          Show this help text.\n"
                   "  --no-menu       Start with the menu bar hidden.\n"
                   "  --no-explorer   Start with the explorer hidden.\n"
                   "  --diff          Compare two files side by side, as git difftool can ask.\n"
#ifdef OLAGARRO_OUI_CODE_WINDOW
                   "  --window        Open in a desktop window instead of the terminal, as it\n"
                   "                  does when not started from a terminal.\n"
#endif
#ifdef OLAGARRO_OUI_CODE_DRM
                   "  --drm           Take the whole screen, with no desktop: for a console.\n"
#endif
#ifdef OLAGARRO_OUI_CODE_DESKTOP
                   "  --desktop       Show in a window on OUI's own desktop.\n"
#endif
                   "  --legacy-render Use the classic Win32 console API instead of VT escape\n"
                   "                  sequences (bold degrades to bright colors).\n"
                   "  --smoke-test    Start, render once, and exit.\n"
                   "  --stress-test   Exercise editor dialogs and commands, then exit.\n"
                   "  --input-test    Type dead-key accent sequences into a new document,\n"
                   "                  print the resulting text, and exit.\n"
                   "  --key-test      Feed dead-key keystrokes to the real terminal backend,\n"
                   "                  type them into a new document, print the resulting\n"
                   "                  text, and exit (interactive console required).\n",
                   stdout);
        return 0;
    }
    const auto smoke_test{command_line.has_option("smoke-test")};
    const auto stress_test{command_line.has_option("stress-test")};
    const auto input_test{command_line.has_option("input-test")};
    const auto key_test{command_line.has_option("key-test")};
    const auto legacy_render{command_line.has_option("legacy-render")};
#ifdef OLAGARRO_OUI_CODE_DRM
    const auto on_the_screen{command_line.has_option("drm")};
#endif
#ifdef OLAGARRO_OUI_CODE_DESKTOP
    const auto on_the_desktop{command_line.has_option("desktop")};
#ifdef OLAGARRO_OUI_CODE_DESKTOP_PIXELS
    auto desktop_shows_pixels{false};
#endif
#endif
    const auto headless_test{smoke_test || stress_test || input_test || key_test};
#ifdef OLAGARRO_OUI_CODE_WINDOW
    // Started from a desktop's menu or an icon, with no terminal to show in, it shows in a window unless told where.
    const auto in_window{command_line.has_option("window") ||
                         (!headless_test && !command_line.has_option("drm") && !command_line.has_option("desktop") &&
                          !oui::started_from_terminal())};
#endif
    // The folder and the files to open, whole, so what is kept of them still holds when OUI Code starts somewhere else.
    std::optional<std::filesystem::path> folder_argument;
    std::vector<std::filesystem::path> file_arguments;
    // With --diff, the two files to compare instead.
    std::optional<std::pair<std::filesystem::path, std::filesystem::path>> compare_arguments;
    if(command_line.has_option("diff"))
    {
        const auto left{command_line.positional(0)};
        const auto right{command_line.positional(1)};
        if(!left || !right)
        {
            std::fputs("oui_code: --diff takes two files\n", stderr);
            return 2;
        }
        compare_arguments =
            std::pair{oui::whole_path(oui::path_from_utf8(*left)), oui::whole_path(oui::path_from_utf8(*right))};
    }
    else
    {
        for(std::size_t index{}; const auto given{command_line.positional(index)}; ++ index)
        {
            auto path{oui::whole_path(oui::path_from_utf8(*given))};
            std::error_code unreadable;
            if(!std::filesystem::is_directory(path, unreadable))
            {
                file_arguments.push_back(std::move(path));
            }
            else if(folder_argument)
            {
                std::fputs("oui_code: one folder at a time\n", stderr);
                return 2;
            }
            else
            {
                folder_argument = std::move(path);
            }
        }
    }
    oui::Settings settings{oui::SettingsLocation::portable, "oui_code"};
    const auto settings_path{settings.file_path()};
    const auto smoke_backup_path{oui::path_from_utf8(oui::utf8_of(settings_path) + ".smoke-backup")};
    std::error_code settings_missing;
    const auto had_settings_file{std::filesystem::exists(settings_path, settings_missing)};
    if(smoke_test && had_settings_file)
    {
        std::error_code ec;
        oui::copy_entry(settings_path, smoke_backup_path, ec);
    }
    static_cast<void>(settings.load());
    // Before anything shows a text, or makes one to show later, as the application's own commands do. The tests run in
    // English, as the sources write it.
    std::string interface_language{"en"};
    if(!headless_test)
    {
        const std::string language{settings.value("language", "auto")};
        interface_language = language == "auto" ? oui::system_language() : language;
        oui::install_language(interface_language, oui_code_translations());
    }
    std::unique_ptr<oui::Backend> backend;
    oui::ScriptedBackend* scripted_backend{};
#ifdef OLAGARRO_OUI_CODE_WINDOW
    // The window OUI Code shows in, when it does.
    oui::DesktopWindow* window_screen{};
#endif
    if(key_test)
    {
        return run_key_test();
    }
    if(input_test)
    {
        auto scripted{oui::scripted_backend(input_test_events(), oui::Size{100, 30})};
        scripted_backend = scripted.get();
        backend = std::move(scripted);
    }
    else if(stress_test)
    {
        auto scripted{oui::scripted_backend(stress_test_events(), oui::Size{100, 30})};
        scripted_backend = scripted.get();
        backend = std::move(scripted);
    }
    else if(smoke_test)
    {
        backend = oui::scripted_backend(std::vector<oui::Event>{}, oui::Size{100, 30});
    }
#ifdef OLAGARRO_OUI_CODE_WINDOW
    else if(in_window)
    {
        oui::WindowOptions window_options;
        window_options.title = "OUI Code";
        // Turbo C's blue, as OUI Code is in the terminal.
        window_options.icon = oui::text_icon("OC", {0, 0, 170}, {255, 255, 255});
        // Where the window was left.
        window_options.geometry = window_geometry_from(settings);
        auto screen{oui::window_backend(window_options)};
        window_screen = screen.get();
        backend = std::move(screen);
    }
#endif
#ifdef OLAGARRO_OUI_CODE_DRM
    else if(on_the_screen)
    {
        // The keyboard's layout as a desktop names it, as XKB_DEFAULT_LAYOUT does.
        oui::DrmOptions options;
        if(const auto* const layout{std::getenv("OUI_KEYBOARD")})
        {
            options.keyboard = layout;
        }
        backend = oui::drm_backend(options);
    }
#endif
#ifdef OLAGARRO_OUI_CODE_DESKTOP
    else if(on_the_desktop)
    {
        oui::DesktopClientOptions options;
        options.title = "OUI Code";
#ifdef OLAGARRO_OUI_CODE_DESKTOP_PIXELS
        backend = oui::desktop_backend(options, oui::classic_style());
        desktop_shows_pixels = dynamic_cast<oui::PixelBackend*>(backend.get()) != nullptr;
#else
        backend = oui::desktop_backend(options);
#endif
    }
#endif
    else
    {
        oui::TerminalOptions terminal_options;
        terminal_options.force_legacy = legacy_render;
        backend = oui::terminal_backend(terminal_options);
    }

    oui::Application application{std::move(backend)};
#ifdef OLAGARRO_DEBUG_BUILD
    std::unique_ptr<std::FILE, int (*)(std::FILE*)> debug_event_log{std::fopen("oui_code_events.log", "w"),
                                                                    &std::fclose};
#endif
    auto theme{oui::TurboCTheme::make()};
    // Characters, unless what shows OUI Code draws in pixels.
    auto in_cells{true};
#ifdef OLAGARRO_OUI_CODE_WINDOW
    if(in_window)
    {
        theme = oui::classic_theme();
        in_cells = false;
    }
#endif
#ifdef OLAGARRO_OUI_CODE_DRM
    if(on_the_screen)
    {
        theme = oui::classic_theme();
        in_cells = false;
    }
#endif
#ifdef OLAGARRO_OUI_CODE_DESKTOP_PIXELS
    // The graphical look on a desktop of pixels; on one in a terminal, the terminal's.
    if(on_the_desktop && desktop_shows_pixels)
    {
        theme = oui::classic_theme();
        in_cells = false;
    }
#endif
    application.set_theme(theme);
    WorkbenchOptions options;
    options.headless = headless_test;
    options.no_menu = command_line.has_option("no-menu");
    options.no_explorer = command_line.has_option("no-explorer");
    options.folder = folder_argument;
    options.files = file_arguments;
    options.compare = compare_arguments;
    options.interface_language = interface_language;
    options.in_cells = in_cells;
    options.legacy_render = legacy_render;
#ifdef OLAGARRO_OUI_CODE_WINDOW
    options.window_screen = window_screen;
#endif
#ifdef OLAGARRO_DEBUG_BUILD
    options.event_log = debug_event_log.get();
#endif
    Workbench workbench{application, settings, std::move(options)};
    if(scripted_backend != nullptr)
    {
        if(input_test)
        {
            // The script opens a new tab first, so the text must land in the editor the user sees.
            scripted_backend->set_on_drained(
                [&application, &workbench]
                {
                    const auto* const visible{workbench.shown_document()};
                    std::fputs(("INPUT_TEST_RESULT " + std::string{visible ? visible->text() : ""} + "\n").c_str(),
                               stdout);
                    std::fflush(stdout);
                    application.quit();
                });
        }
        else
        {
            scripted_backend->set_on_drained([&application] { application.quit(); });
        }
    }
    if(smoke_test)
    {
        application.post([&application] { application.quit(); });
    }
    const auto result{workbench.run()};
    if(smoke_test)
    {
        std::error_code ec;
        if(had_settings_file)
        {
            oui::copy_entry(smoke_backup_path, settings_path, ec);
        }
        else
        {
            static_cast<void>(std::filesystem::remove(settings_path, ec));
        }
        static_cast<void>(std::filesystem::remove(smoke_backup_path, ec));
    }
    return result;
}
