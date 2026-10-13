#include "preferences.h"

#include "resources.h"
#include "ui_descriptions.h"

#include <oui/oui.h>

#include <algorithm>
#include <charconv>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using namespace olagarro;

void show_preferences(oui::Application& application, oui::Settings& settings, Editors& editors,
                      UnsavedBackup& unsaved_backup, oui::Suggester& suggester)
{
    auto ui{build_oui_code_ui(OuiCodeUi::preferences)};
    // "auto" follows the system; the rest are English, which the sources are written in, and the languages OUI Code
    // has catalogs for.
    std::vector<std::string> language_codes{"auto", "en"};
    std::vector<std::string> language_names{oui::translate("Preferences", "System language"), "English"};
    for(const auto& translation : oui_code_translations())
    {
        language_codes.push_back(translation.language());
        language_names.push_back(translation.name());
    }
    const auto current_language{
        std::find(language_codes.begin(), language_codes.end(), settings.value("language", "auto"))};
    auto& language{ui.get<oui::ComboBox>("language")};
    language.set_options(language_names);
    language.set_selected(current_language == language_codes.end()
                              ? 0
                              : static_cast<std::size_t>(current_language - language_codes.begin()));
    language.on_selected(
        [&settings, language_codes](std::size_t index)
        {
            settings.set_value("language", language_codes.at(index));
            static_cast<void>(settings.save());
        });
    // A box ticked or not is kept at once, under its setting, and does what it changes.
    const auto keep{[&ui, &settings](std::string_view id, const char* setting, bool otherwise, auto&& then)
                    {
                        auto& box{ui.get<oui::CheckButton>(id)};
                        box.set_checked(settings.bool_value(setting, otherwise));
                        box.on_toggled(
                            [&settings, setting, then](bool checked)
                            {
                                settings.set_bool_value(setting, checked);
                                settings.save();
                                then(checked);
                            });
                    }};
    keep("use_spaces", "tab_use_spaces", false, [](bool) {});
    keep("suggest", "suggest_as_you_type", true, [&suggester](bool checked) { suggester.set_as_you_type(checked); });
    keep("reload", "reload_external_changes", true, [](bool) {});
    keep("hot_exit", "hot_exit", false, [](bool) {});
    keep("backup", "backup_unsaved", true, [&unsaved_backup](bool) { unsaved_backup.configure(); });
    // A number is kept as it is written, when it is one within its bounds.
    const auto keep_number{
        [&ui, &settings](std::string_view id, const char* setting, int value, int least, int most, auto&& then)
        {
            auto& field{ui.get<oui::LineEdit>(id)};
            field.set_text(std::to_string(value));
            field.on_changed(
                [&settings, setting, least, most, then](std::string_view text)
                {
                    int number{};
                    const auto [end, error]{std::from_chars(text.data(), text.data() + text.size(), number)};
                    if(error == std::errc{} && end == text.data() + text.size() && number >= least && number <= most)
                    {
                        settings.set_int_value(setting, number);
                        settings.save();
                        then();
                    }
                });
        }};
    keep_number("spaces", "tab_space_count", settings.int_value("tab_space_count", 4), 1, 32, [] {});
    keep_number("backup_interval", "backup_interval_seconds", UnsavedBackup::interval_seconds(settings), 1, 3600,
                [&unsaved_backup] { unsaved_backup.configure(); });
    auto dialog{ui.take_window()};
    auto& dialog_reference{*dialog};
    static_cast<void>(ui.get<oui::Button>("ok").on_clicked(
        [&dialog_reference, &editors, &settings]
        {
            for(auto& group : editors.groups())
            {
                for(std::size_t i{}; i < group.tabs.child_count(); ++ i)
                {
                    if(auto* editor{document_editor(group.tabs, i)})
                    {
                        apply_tab_settings(*editor, settings);
                    }
                }
            }
            dialog_reference.close(oui::DialogResult::accepted);
        }));
    static_cast<void>(ui.get<oui::Button>("cancel").on_clicked(
        [&dialog_reference] { dialog_reference.close(oui::DialogResult::cancelled); }));
    static_cast<void>(application.show_modal(std::move(dialog)));
}
