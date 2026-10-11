#include <oui/oui.h>

#include <cstddef>
#include <string_view>
#include <vector>

using namespace olagarro;

// rename_files.oui, built into the program by CMake, in CMakeLists.txt:
//     olagarro_embed_files(<target> FUNCTION embedded_rename_files_ui FILES <its folder>/rename_files.oui)
std::vector<std::string_view> embedded_rename_files_ui();

// Shows the window rename_files.oui describes and waits for it to close.
void show_rename_files(oui::Application& application)
{
    // As written: with a context= in the description, its texts are translated.
    auto ui{oui::build_ui(embedded_rename_files_ui().front(), oui::UiTexts::as_written)};
    auto& pattern{ui.get<oui::LineEdit>("pattern")};
    pattern.on_changed([&](std::string_view /*text*/) {});
    auto& dry{ui.get<oui::CheckButton>("dry")};
    dry.on_toggled([&](bool /*checked*/) {});
    auto& case_{ui.get<oui::ComboBox>("case")};
    case_.set_options({});
    case_.on_selected([&](std::size_t /*index*/) {});
    // ui.get<oui::Label>("status").set_text("");
    auto& go{ui.get<oui::Button>("go")};
    go.on_clicked([&] {});
    static_cast<void>(application.show_modal(ui.take_window()));
}

// Or rename_files.oui read from disk as the program runs, rather than built in: without the CMake line, this,
// with the path to it from where the program is started.
// std::vector<std::string_view> embedded_rename_files_ui()
// {
//     static const auto text{oui::read_file("rename_files.oui").value_or("")};
//     return {text};
// }
