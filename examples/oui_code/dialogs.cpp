#include "dialogs.h"

#include "ui_descriptions.h"
#include "version.h"

#include <oui/oui.h>

#include <charconv>
#include <memory>
#include <optional>
#include <string_view>
#include <system_error>

using namespace olagarro;

namespace
{

[[nodiscard]] std::optional<std::size_t> parse_line_number(std::string_view text)
{
    std::size_t value{};
    const auto [position, error]{std::from_chars(text.data(), text.data() + text.size(), value)};
    if(error != std::errc{} || position != text.data() + text.size() || value == 0)
    {
        return std::nullopt;
    }
    return value;
}

} // namespace

void show_markdown(oui::Application& application, const std::string& title, std::string markdown,
                   const std::string& placement_key)
{
    auto ui{build_oui_code_ui(OuiCodeUi::markdown)};
    auto& view{ui.get<oui::MarkdownView>("view")};
    view.set_markdown(std::move(markdown));
    auto dialog{ui.take_window()};
    dialog->set_title(title);
    dialog->set_placement_key(placement_key);
    application.post([&application, &view] { application.focus(view); });
    static_cast<void>(application.show_modal(std::move(dialog)));
}

void show_palette(oui::Application& application)
{
    oui::CommandPaletteOptions options;
    options.placement_key = "command_palette";
    oui::show_command_palette(application, options);
}

oui::FileDialogOptions file_dialog_options(oui::FileDialogMode mode, const std::filesystem::path& folder)
{
    oui::FileDialogOptions options;
    options.mode = mode;
    options.directory = folder;
    options.placement_key = mode == oui::FileDialogMode::open_folder ? "open_folder"
                            : mode == oui::FileDialogMode::save_file ? "save_as"
                                                                     : "open_file";
    options.filters = {{oui::translate("Files", "All files (*)"), "*"},
                       {oui::translate("Files", "Text (*.txt, *.md)"), "*.txt;*.md"},
                       {oui::translate("Files", "C and C++ (*.c, *.cpp, *.h, *.hpp)"), "*.c;*.cpp;*.h;*.hpp"},
                       {oui::translate("Files", "JSON (*.json)"), "*.json"}};
    return options;
}

void show_error(oui::Application& application, const std::string& title, const std::string& message)
{
    static_cast<void>(oui::message_box(application, title, message));
}

UnsavedChangesAction confirm_unsaved_changes(oui::Application& application)
{
    const auto answer{oui::message_box(
        application, oui::translate("Files", "Unsaved Changes"),
        oui::translate("Files", "Save changes before closing?"),
        {oui::translate("Files", "Save"), oui::translate("Files", "Discard"), oui::translate("Files", "Cancel")})};
    if(answer == std::optional<std::size_t>{0})
    {
        return UnsavedChangesAction::save;
    }
    if(answer == std::optional<std::size_t>{1})
    {
        return UnsavedChangesAction::discard;
    }
    return UnsavedChangesAction::cancel;
}

void show_replace_dialog(oui::Application& application, oui::TextEdit& document)
{
    auto ui{build_oui_code_ui(OuiCodeUi::replace)};
    auto dialog{ui.take_window()};
    auto& dialog_reference{*dialog};
    auto& query{ui.get<oui::LineEdit>("query")};
    auto& replacement{ui.get<oui::LineEdit>("replacement")};
    auto& outcome{ui.get<oui::Label>("outcome")};
    static_cast<void>(ui.get<oui::Button>("replace").on_clicked(
        [&dialog_reference, &document, &query, &replacement, &outcome]
        {
            if(document.replace_next(query.text(), replacement.text()))
            {
                dialog_reference.close(oui::DialogResult::accepted);
                return;
            }
            outcome.set_text(oui::translate("ReplaceDialog", "Not found."));
        }));
    static_cast<void>(
        ui.get<oui::Button>("replace_all")
            .on_clicked(
                [&document, &query, &replacement, &outcome]
                {
                    const auto count{document.replace_all(query.text(), replacement.text())};
                    outcome.set_text(count == 0 ? oui::translate("ReplaceDialog", "Not found.")
                                                : oui::format_text(oui::translate("ReplaceDialog", "Replacements: {0}"),
                                                                   {std::to_string(count)}));
                }));
    static_cast<void>(ui.get<oui::Button>("cancel").on_clicked(
        [&dialog_reference] { dialog_reference.close(oui::DialogResult::cancelled); }));
    static_cast<void>(application.show_modal(std::move(dialog)));
}

void show_go_to_line_dialog(oui::Application& application, oui::TextEdit& document)
{
    oui::PromptOptions options;
    options.title = oui::translate("GoToLine", "Go To Line");
    options.label = oui::translate("GoToLine", "&Line number:");
    options.placeholder = "1";
    options.accept = oui::translate("GoToLine", "Go");
    options.validate = [&document](std::string_view text) -> std::optional<std::string>
    {
        const auto line_number{parse_line_number(text)};
        if(!line_number || !document.go_to_line(*line_number))
        {
            return oui::translate("GoToLine", "No such line.");
        }
        return std::nullopt;
    };
    static_cast<void>(oui::prompt(application, options));
}

void filter_through_command(oui::Application& application, oui::TextEdit& editor, std::string& last_command)
{
    oui::PromptOptions options;
    options.title = oui::translate("Filter", "Filter Through Command");
    options.label = oui::translate("Filter", "Command, which gets the selection or the whole document:");
    options.text = last_command;
    options.placeholder = "sort -u";
    const auto command{oui::prompt(application, options)};
    if(!command || command->empty())
    {
        return;
    }
    last_command = *command;
    if(editor.selected_text().empty())
    {
        editor.select_all();
    }
    std::string output;
    std::string error;
    std::optional<int> exit_code;
    auto ui{build_oui_code_ui(OuiCodeUi::filter_running)};
    ui.get<oui::Label>("running").set_text(oui::format_text(oui::translate("Filter", "Running {0}"), {*command}));
    auto dialog{ui.take_window()};
    auto& dialog_reference{*dialog};
    std::unique_ptr<oui::Process> process;
    try
    {
        process = oui::start_process(application, {oui::shell_command(*command), {}, editor.selected_text()},
                                     {[&output](std::string_view text) { output += text; },
                                      [&error](std::string_view text) { error += text; },
                                      [&exit_code, &dialog_reference](int code)
                                      {
                                          exit_code = code;
                                          dialog_reference.close(oui::DialogResult::accepted);
                                      }});
    }
    catch(const std::system_error& failure)
    {
        show_error(application, options.title, failure.what());
        return;
    }
    static_cast<void>(application.show_modal(std::move(dialog)));
    if(!exit_code)
    {
        // Stopped with Esc: the process goes, and the text stays as it was.
        return;
    }
    if(*exit_code != 0)
    {
        constexpr std::size_t most_shown{2000};
        show_error(application, options.title,
                   oui::format_text(oui::translate("Filter", "{0} ended with code {1}.\n{2}"),
                                    {*command, std::to_string(*exit_code), error.substr(0, most_shown)}));
        return;
    }
    editor.paste(output);
}

void show_about(oui::Application& application)
{
    auto ui{build_oui_code_ui(OuiCodeUi::about)};
    ui.get<oui::Label>("version").set_text(
        oui::format_text(oui::translate("About", "Version {0}"), {oui_code_version}));
    auto dialog{ui.take_window()};
    auto& dialog_reference{*dialog};
    static_cast<void>(ui.get<oui::Button>("ok").on_clicked([&dialog_reference]
                                                           { dialog_reference.close(oui::DialogResult::accepted); }));
    static_cast<void>(application.show_modal(std::move(dialog)));
}
