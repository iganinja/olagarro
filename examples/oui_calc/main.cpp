// oui_calc: a calculator as a prompt, in OUI: in the terminal it is started from, in a desktop window, on the whole
// screen with no desktop, or on OUI's own desktop.
#include <oui/calculator_view.h>
#include <oui/oui.h>

#ifdef OLAGARRO_OUI_CALC_WINDOW
#include <oui/graphics/window_backend.h>
#endif
#ifdef OLAGARRO_OUI_CALC_DRM
#include <oui/graphics/drm_backend.h>
#endif
#ifdef OLAGARRO_OUI_CALC_DESKTOP
#include <oui/desktop.h>
#endif
#ifdef OLAGARRO_OUI_CALC_DESKTOP_PIXELS
#include <oui/graphics/desktop.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace olagarro;

// Defined in the source CMake generates from examples/oui_calc/translations.
std::vector<std::string_view> embedded_oui_calc_catalogs();

int main(int argument_count, char* const* const arguments)
{
    const oui::CommandLine command_line{oui::program_arguments(argument_count, arguments)};
    if(command_line.has_option("help"))
    {
        std::fputs("OUI Calc\n\n"
                   "Usage: oui_calc [options]\n\n"
                   "A calculator: write an expression, as (800*0.16) - (0.32*500), and press Enter.\n\n"
                   "Options:\n"
                   "  --help          Show this help text.\n"
#ifdef OLAGARRO_OUI_CALC_WINDOW
                   "  --window        Open in a desktop window instead of the terminal, as it\n"
                   "                  does when not started from a terminal.\n"
#endif
#ifdef OLAGARRO_OUI_CALC_DRM
                   "  --drm           Take the whole screen, with no desktop: for a console.\n"
#endif
#ifdef OLAGARRO_OUI_CALC_DESKTOP
                   "  --desktop       Show in a window on OUI's own desktop.\n"
#endif
                   ,
                   stdout);
        return 0;
    }
    oui::install_language(oui::system_language(), oui::embedded_translations(embedded_oui_calc_catalogs()));

    std::unique_ptr<oui::Backend> backend;
    // In characters, Turbo's look, as OUI Code's; the graphical one where there are pixels.
    auto theme{oui::TurboCTheme::make()};
#ifdef OLAGARRO_OUI_CALC_WINDOW
    // Started from a desktop's menu or an icon, with no terminal to show in, it shows in a window unless told where.
    const auto in_window{command_line.has_option("window") ||
                         (!command_line.has_option("drm") && !command_line.has_option("desktop") &&
                          !oui::started_from_terminal())};
#else
    const auto in_window{command_line.has_option("window")};
#endif
    if(in_window)
    {
#ifdef OLAGARRO_OUI_CALC_WINDOW
        oui::WindowOptions window_options;
        window_options.title = "OUI Calc";
        window_options.geometry.size = {640, 480};
        window_options.icon = oui::text_icon("=", {255, 255, 255}, {0, 0, 170});
        backend = oui::window_backend(window_options);
        theme = oui::classic_theme();
#else
        std::fputs("oui_calc: this build cannot open windows\n", stderr);
        return 2;
#endif
    }
    else if(command_line.has_option("drm"))
    {
#ifdef OLAGARRO_OUI_CALC_DRM
        oui::DrmOptions drm_options;
        if(const auto* const layout{std::getenv("OUI_KEYBOARD")})
        {
            drm_options.keyboard = layout;
        }
        backend = oui::drm_backend(drm_options);
        theme = oui::classic_theme();
#else
        std::fputs("oui_calc: this build cannot take the screen\n", stderr);
        return 2;
#endif
    }
    else if(command_line.has_option("desktop"))
    {
#ifdef OLAGARRO_OUI_CALC_DESKTOP
        oui::DesktopClientOptions desktop_options;
        desktop_options.title = oui::translate("Calculator", "Calculator");
#ifdef OLAGARRO_OUI_CALC_DESKTOP_PIXELS
        backend = oui::desktop_backend(desktop_options, oui::classic_style());
        if(dynamic_cast<oui::PixelBackend*>(backend.get()) != nullptr)
        {
            theme = oui::classic_theme();
        }
#else
        backend = oui::desktop_backend(desktop_options);
#endif
#else
        std::fputs("oui_calc: this build cannot show on the desktop\n", stderr);
        return 2;
#endif
    }
    else
    {
        backend = oui::terminal_backend();
    }
    oui::Application application{std::move(backend)};
    application.set_theme(theme);
    application.set_title(oui::translate("Calculator", "Calculator"));

    auto window{oui::window("OUI Calc")};
    window->set_border(oui::WindowBorder::none);
    window->set_close_on_escape(false);
    auto view{std::make_unique<oui::CalculatorView>(application)};
    auto& prompt{view->prompt()};
    // exit written on the line ends it, as Ctrl+Q does.
    static_cast<void>(view->on_exit([&application] { application.quit(); }));
    window->add(std::move(view));
    application.set_window(std::move(window));
    application.focus(prompt);
    // Ctrl+Q ends it, as it ends OUI's other programs.
    application.commands().register_command("calculator.quit", oui::translate("Calculator", "Quit"),
                                            [&application] { application.quit(); });
    application.commands().bind("calculator.quit", {U'q', true});
    application.commands().register_command(
        "calculator.help", oui::translate("Calculator", "Help"),
        [&application] { oui::show_calculator_help(application, {{"Ctrl+Q", oui::translate("Calculator", "Quit")}}); });
    application.commands().bind("calculator.help", {oui::Key::f1});
    application.exec();
    return 0;
}
