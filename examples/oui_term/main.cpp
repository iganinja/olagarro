// oui_term: a terminal, the user's shell or the program given, in OUI: in a desktop window, on the whole screen
// with no desktop, or in the terminal it is started from.
#include <oui/oui.h>
#include <oui/terminal_view.h>

#ifdef OLAGARRO_OUI_TERM_WINDOW
#include <oui/graphics/window_backend.h>
#endif
#ifdef OLAGARRO_OUI_TERM_DRM
#include <oui/graphics/drm_backend.h>
#endif
#ifdef OLAGARRO_OUI_TERM_DESKTOP
#include <oui/desktop.h>
#endif
#ifdef OLAGARRO_OUI_TERM_DESKTOP_PIXELS
#include <oui/graphics/desktop.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace olagarro;

int main(int argument_count, char* const* const arguments)
{
    // Its own options first; whatever follows is the program to run and its arguments, dashes and all.
    auto words{oui::program_arguments(argument_count, arguments)};
    auto in_window{false};
    auto on_the_screen{false};
    auto on_the_desktop{false};
    std::size_t first_command_word{};
    for(; first_command_word < words.size(); ++ first_command_word)
    {
        const std::string_view word{words[first_command_word]};
        if(word == "--help")
        {
            std::fputs("OUI Term\n\n"
                       "Usage: oui_term [options] [program [arguments]]\n\n"
                       "Runs the program, or the user's shell, in a terminal.\n\n"
                       "Options:\n"
                       "  --help          Show this help text.\n"
#ifdef OLAGARRO_OUI_TERM_WINDOW
                       "  --window        Open in a desktop window instead of the terminal.\n"
#endif
#ifdef OLAGARRO_OUI_TERM_DRM
                       "  --drm           Take the whole screen, with no desktop: for a console.\n"
#endif
#ifdef OLAGARRO_OUI_TERM_DESKTOP
                       "  --desktop       Show in a window on OUI's own desktop.\n"
#endif
                       ,
                       stdout);
            return 0;
        }
        if(word == "--window")
        {
            in_window = true;
        }
        else if(word == "--drm")
        {
            on_the_screen = true;
        }
        else if(word == "--desktop")
        {
            on_the_desktop = true;
        }
        else if(word == "--")
        {
            ++ first_command_word;
            break;
        }
        else
        {
            break;
        }
    }
    oui::PtyOptions options;
    options.command.assign(words.begin() + static_cast<std::ptrdiff_t>(first_command_word), words.end());

    // OUI's own texts, in the user's language; the program has none of its own.
    oui::install_language(oui::system_language(), {});
    std::unique_ptr<oui::Backend> backend;
    oui::Theme theme{};
    if(in_window)
    {
#ifdef OLAGARRO_OUI_TERM_WINDOW
        oui::WindowOptions window_options;
        window_options.title = "OUI Term";
        window_options.geometry.size = {960, 600};
        window_options.icon = oui::text_icon(">_", {0, 0, 0}, {170, 170, 170});
        // Black too round the cells, where the window is not a whole number of them.
        window_options.style.desktop = {0, 0, 0};
        backend = oui::window_backend(window_options);
        theme = oui::classic_theme();
#else
        std::fputs("oui_term: this build cannot open windows\n", stderr);
        return 2;
#endif
    }
    else if(on_the_screen)
    {
#ifdef OLAGARRO_OUI_TERM_DRM
        oui::DrmOptions drm_options;
        drm_options.style.desktop = {0, 0, 0};
        if(const auto* const layout{std::getenv("OUI_KEYBOARD")})
        {
            drm_options.keyboard = layout;
        }
        backend = oui::drm_backend(drm_options);
        theme = oui::classic_theme();
#else
        std::fputs("oui_term: this build cannot take the screen\n", stderr);
        return 2;
#endif
    }
    else if(on_the_desktop)
    {
#ifdef OLAGARRO_OUI_TERM_DESKTOP
        oui::DesktopClientOptions desktop_options;
        desktop_options.title = "Terminal";
#ifdef OLAGARRO_OUI_TERM_DESKTOP_PIXELS
        auto style{oui::classic_style()};
        style.desktop = {0, 0, 0};
        backend = oui::desktop_backend(desktop_options, style);
        // The graphical look on a desktop of pixels; on one in a terminal, a terminal's.
        if(dynamic_cast<oui::PixelBackend*>(backend.get()) != nullptr)
        {
            theme = oui::classic_theme();
        }
#else
        backend = oui::desktop_backend(desktop_options);
#endif
#else
        std::fputs("oui_term: this build cannot show on the desktop\n", stderr);
        return 2;
#endif
    }
    else
    {
        backend = oui::terminal_backend();
    }
    oui::Application application{std::move(backend)};
    // A terminal's own colours: light grey on black, whatever look the rest has.
    theme.background = {0, 0, 0};
    theme.editor_background = {0, 0, 0};
    theme.editor_foreground = {170, 170, 170};
    application.set_theme(theme);

    auto window{oui::window("OUI Term")};
    window->set_border(oui::WindowBorder::none);
    std::unique_ptr<oui::TerminalView> view;
    try
    {
        view = std::make_unique<oui::TerminalView>(application, std::move(options));
    }
    catch(const std::exception& error)
    {
        application.quit();
        std::fprintf(stderr, "oui_term: %s\n", error.what());
        return 1;
    }
    auto& view_reference{*view};
    auto exit_code{0};
    // The terminal goes when its program does, with its code.
    view->on_finished(
        [&application, &exit_code](int code)
        {
            exit_code = code;
            application.quit();
        });
    // The window is named after what runs in it, as sh or top, or what the program calls it.
    view->on_title_changed([&application](const std::string& title)
                           { application.set_title(title.empty() ? "Terminal" : title); });
    window->add(std::move(view));
    application.set_window(std::move(window));
    application.focus(view_reference);
    application.exec();
    return exit_code;
}
