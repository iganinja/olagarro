#include "workbench.h"

#include "dialogs.h"
#include "preferences.h"
#include "task_list.h"

#include <oui/oui.h>

#include <string>
#include <tuple>

using namespace olagarro;

namespace
{

// A command that does `action` to the document of the active group, when there is one.
template <typename Action> auto on_current_editor(Editors& editors, Action action)
{
    return [&editors, action]
    {
        if(auto* const editor{selected_document_editor(editors.active().tabs)})
        {
            action(*editor);
        }
    };
}

} // namespace

void Workbench::register_edit_commands()
{
    m_application.commands().register_command("edit.undo", oui::translate("Commands", "Undo"),
                                              [this]
                                              {
                                                  auto& group{m_editors.active()};
                                                  auto& tabs_reference{group.tabs};

                                                  if(auto* const current{selected_document_editor(tabs_reference)})
                                                  {
                                                      static_cast<void>(current->undo());
                                                  }
                                              });
    m_application.commands().register_command("edit.redo", oui::translate("Commands", "Redo"),
                                              [this]
                                              {
                                                  auto& group{m_editors.active()};
                                                  auto& tabs_reference{group.tabs};

                                                  if(auto* const current{selected_document_editor(tabs_reference)})
                                                  {
                                                      static_cast<void>(current->redo());
                                                  }
                                              });
    m_application.commands().register_command(
        "edit.copy", oui::translate("Commands", "Copy"),
        [this]
        {
            auto& group{m_editors.active()};
            auto& tabs_reference{group.tabs};

            if(auto* const current{dynamic_cast<oui::LineEdit*>(m_application.focused())})
            {
                static_cast<void>(m_application.clipboard().set_text(current->selected_text()));
                return;
            }
            if(auto* const current{selected_document_editor(tabs_reference)})
            {
                static_cast<void>(m_application.clipboard().set_text(current->selected_text()));
            }
        });
    m_application.commands().register_command(
        "edit.cut", oui::translate("Commands", "Cut"),
        [this]
        {
            auto& group{m_editors.active()};
            auto& tabs_reference{group.tabs};

            if(auto* const current{dynamic_cast<oui::LineEdit*>(m_application.focused())})
            {
                static_cast<void>(m_application.clipboard().set_text(current->selected_text()));
                current->paste({});
                return;
            }
            if(auto* const current{selected_document_editor(tabs_reference)})
            {
                static_cast<void>(m_application.clipboard().set_text(current->selected_text()));
                current->paste({});
            }
        });
    m_application.commands().register_command(
        "edit.paste", oui::translate("Commands", "Paste"),
        [this]
        {
            auto& group{m_editors.active()};
            auto& tabs_reference{group.tabs};

            if(auto* const current{dynamic_cast<oui::LineEdit*>(m_application.focused())})
            {
                current->paste(m_application.clipboard().text());
                return;
            }
            if(auto* const current{selected_document_editor(tabs_reference)})
            {
                current->paste(m_application.clipboard().text());
            }
        });
    m_application.commands().register_command(
        "edit.select_all", oui::translate("Commands", "Select All"),
        [this]
        {
            auto& group{m_editors.active()};
            auto& tabs_reference{group.tabs};

            if(auto* const current{dynamic_cast<oui::LineEdit*>(m_application.focused())})
            {
                current->select_all();
                return;
            }
            if(auto* const current{selected_document_editor(tabs_reference)})
            {
                current->select_all();
            }
        });
    m_application.commands().register_command(
        "edit.find", oui::translate("Commands", "Find"),
        [this]
        {
            auto& group{m_editors.active()};
            auto& tabs_reference{group.tabs};

            if(auto* const current{selected_document_editor(tabs_reference)})
            {
                m_find_bar->open(*current);
            }
            // A task list finds in its own field.
            else if(auto* const tasks{tabs_reference.selected_content<TasksView>()})
            {
                tasks->focus_search();
            }
        });
    m_application.commands().register_command("edit.find_next", oui::translate("Commands", "Find Next"),
                                              [this]
                                              {
                                                  auto& group{m_editors.active()};
                                                  auto& tabs_reference{group.tabs};

                                                  if(auto* const current{selected_document_editor(tabs_reference)})
                                                  {
                                                      m_find_bar->set_document(*current);
                                                      m_find_bar->next_match();
                                                  }
                                              });
    m_application.commands().register_command(
        "edit.find_selected", oui::translate("Commands", "Find Next of the Selection"),
        [this]
        {
            if(auto* const current{selected_document_editor(m_editors.active().tabs)})
            {
                m_find_bar->find_selected(*current);
            }
        });
    m_application.commands().register_command("edit.find_previous", oui::translate("Commands", "Find Previous"),
                                              [this]
                                              {
                                                  auto& group{m_editors.active()};
                                                  auto& tabs_reference{group.tabs};

                                                  if(auto* const current{selected_document_editor(tabs_reference)})
                                                  {
                                                      m_find_bar->set_document(*current);
                                                      m_find_bar->previous_match();
                                                  }
                                              });
    m_application.commands().register_command("edit.replace", oui::translate("Commands", "Replace"),
                                              [this]
                                              {
                                                  auto& group{m_editors.active()};
                                                  auto& tabs_reference{group.tabs};

                                                  if(auto* const current{selected_document_editor(tabs_reference)})
                                                  {
                                                      show_replace_dialog(m_application, *current);
                                                  }
                                              });
    m_application.commands().register_command("edit.go_to_line", oui::translate("Commands", "Go To Line"),
                                              [this]
                                              {
                                                  auto& group{m_editors.active()};
                                                  auto& tabs_reference{group.tabs};

                                                  if(auto* const current{selected_document_editor(tabs_reference)})
                                                  {
                                                      show_go_to_line_dialog(m_application, *current);
                                                  }
                                              });
    m_suggester.set_as_you_type(m_settings.bool_value("suggest_as_you_type", true));
    m_application.commands().register_command("edit.suggest", oui::translate("Commands", "Suggest"),
                                              [this] { m_suggester.suggest(); });
    // Ctrl+Space, as code editors have it; Alt+/ for a terminal that sends Ctrl+Space as nothing OUI Code can tell.
    m_application.commands().bind("edit.suggest", oui::KeyShortcut{oui::Key::space, true});
    m_application.commands().bind("edit.suggest", {U'/', false, true});
    m_application.commands().register_command(
        "edit.preferences", oui::translate("Commands", "Preferences..."),
        [this] { show_preferences(m_application, m_settings, m_editors, m_unsaved_backup, m_suggester); });
    m_application.commands().bind("edit.undo", {U'z', true});
    m_application.commands().bind("edit.redo", {U'y', true});
    m_application.commands().bind("edit.copy", {U'c', true});
    m_application.commands().bind("edit.cut", {U'x', true});
    m_application.commands().bind("edit.paste", {U'v', true});
    m_application.commands().bind("edit.copy", {oui::Key::insert_, true});
    m_application.commands().bind("edit.paste", {oui::Key::insert_, false, false, true});
    m_application.commands().bind("edit.cut", {oui::Key::delete_, false, false, true});
    m_application.commands().bind("edit.select_all", {U'a', true});
    m_application.commands().bind("edit.find", {U'f', true});
    m_application.commands().bind("edit.find_next", {oui::Key::f3});
    m_application.commands().bind("edit.find_previous", {oui::Key::f3, false, false, true});
    m_application.commands().bind("edit.find_selected", {oui::Key::f3, true});
    m_application.commands().bind("edit.replace", {U'h', true});
    m_application.commands().bind("edit.go_to_line", {U'g', true});
    m_application.commands().register_command("edit.go_back", oui::translate("Commands", "Go Back"),
                                              [this]
                                              {
                                                  // In a preview, back over the places its links took it.
                                                  if(!step_preview(true))
                                                  {
                                                      m_navigation.back();
                                                  }
                                              });
    m_application.commands().register_command("edit.go_forward", oui::translate("Commands", "Go Forward"),
                                              [this]
                                              {
                                                  if(!step_preview(false))
                                                  {
                                                      m_navigation.forward();
                                                  }
                                              });
    m_application.commands().bind("edit.go_back", {oui::Key::left, false, true});
    m_application.commands().bind("edit.go_forward", {oui::Key::right, false, true});
}

// Line operations, with the keys VS Code and Sublime Text use for them.
void Workbench::register_line_commands()
{
    m_application.commands().register_command(
        "edit.duplicate_lines", oui::translate("Commands", "Duplicate Line"),
        on_current_editor(m_editors, [](oui::TextEdit& editor) { static_cast<void>(editor.duplicate_lines()); }));
    m_application.commands().register_command(
        "edit.move_lines_up", oui::translate("Commands", "Move Line Up"),
        on_current_editor(m_editors, [](oui::TextEdit& editor) { static_cast<void>(editor.move_lines(false)); }));
    m_application.commands().register_command(
        "edit.move_lines_down", oui::translate("Commands", "Move Line Down"),
        on_current_editor(m_editors, [](oui::TextEdit& editor) { static_cast<void>(editor.move_lines(true)); }));
    m_application.commands().register_command(
        "edit.delete_lines", oui::translate("Commands", "Delete Line"),
        on_current_editor(m_editors, [](oui::TextEdit& editor) { static_cast<void>(editor.delete_lines()); }));
    m_application.commands().register_command(
        "edit.toggle_comment", oui::translate("Commands", "Toggle Line Comment"),
        [this]
        {
            auto& group{m_editors.active()};
            auto& tabs_reference{group.tabs};
            auto& documents{group.documents};

            const auto index{tabs_reference.selected_index()};
            auto* const editor{selected_document_editor(tabs_reference)};
            if(editor != nullptr && index < documents.size())
            {
                const auto& language{documents[index].language};
                static_cast<void>(editor->toggle_line_comment(language ? language->comment : std::string{}));
            }
        });
    m_application.commands().register_command(
        "edit.filter", oui::translate("Commands", "Filter Through Command..."),
        [this]
        {
            if(auto* const editor{selected_document_editor(m_editors.active().tabs)};
               editor != nullptr && !editor->is_read_only())
            {
                filter_through_command(m_application, *editor, m_last_filter_command);
            }
        });
    m_application.commands().register_command(
        "edit.go_to_bracket", oui::translate("Commands", "Go To Matching Bracket"),
        on_current_editor(m_editors,
                          [](oui::TextEdit& editor) { static_cast<void>(editor.go_to_matching_bracket()); }));
    m_application.commands().register_command(
        "edit.add_cursor_above", oui::translate("Commands", "Add Cursor Above"),
        on_current_editor(m_editors, [](oui::TextEdit& editor) { static_cast<void>(editor.add_cursor_above()); }));
    m_application.commands().register_command(
        "edit.add_cursor_below", oui::translate("Commands", "Add Cursor Below"),
        on_current_editor(m_editors, [](oui::TextEdit& editor) { static_cast<void>(editor.add_cursor_below()); }));
    // As VS Code has them on Linux and Windows.
    m_application.commands().bind("edit.add_cursor_above", {oui::Key::up, false, true, true});
    m_application.commands().bind("edit.add_cursor_below", {oui::Key::down, false, true, true});
    m_application.commands().bind("edit.duplicate_lines", {U'd', true, false, true});
    m_application.commands().bind("edit.move_lines_up", {oui::Key::up, false, true});
    m_application.commands().bind("edit.move_lines_down", {oui::Key::down, false, true});
    m_application.commands().bind("edit.delete_lines", {U'k', true, false, true});
    // Terminals send Ctrl+/ as the byte Ctrl+7 also sends, so on a Spanish keyboard, where '/' is Shift+7,
    // Ctrl+7 works as well.
    m_application.commands().bind("edit.toggle_comment", {U'/', true});
    // As in JetBrains editors; VS Code's Ctrl+Shift+\ cannot be typed on many keyboards.
    m_application.commands().bind("edit.go_to_bracket", {U'm', true, false, true});
}

void Workbench::register_format_commands()
{
    for(const auto& [id, title, line_ending] :
        {std::tuple{"document.line_endings_lf", oui::translate("Commands", "Line Endings: LF"), oui::LineEnding::lf},
         std::tuple{"document.line_endings_crlf", oui::translate("Commands", "Line Endings: CRLF"),
                    oui::LineEnding::crlf}})
    {
        m_application.commands().register_command(
            id, title, [this, line_ending = line_ending]
            { set_active_format([line_ending](oui::TextFormat& format) { format.line_ending = line_ending; }); });
    }
    for(const auto& [id, title, encoding] :
        {std::tuple{"document.encoding_utf8", oui::translate("Commands", "Encoding: UTF-8"), oui::TextEncoding::utf8},
         std::tuple{"document.encoding_utf8_bom", oui::translate("Commands", "Encoding: UTF-8 BOM"),
                    oui::TextEncoding::utf8_bom},
         std::tuple{"document.encoding_windows_1252", oui::translate("Commands", "Encoding: Windows-1252"),
                    oui::TextEncoding::windows_1252}})
    {
        m_application.commands().register_command(
            id, title, [this, encoding = encoding]
            { set_active_format([encoding](oui::TextFormat& format) { format.encoding = encoding; }); });
    }
}

void Workbench::set_active_format(const std::function<void(oui::TextFormat&)>& change)
{
    auto& group{m_editors.active()};
    auto& tabs_reference{group.tabs};
    auto& documents{group.documents};

    const auto index{tabs_reference.selected_index()};
    auto* const editor{selected_document_editor(tabs_reference)};
    if(editor == nullptr || index >= documents.size())
    {
        return;
    }
    auto format{documents.at(index).format};
    change(format);
    if(format.encoding != documents.at(index).format.encoding ||
       format.line_ending != documents.at(index).format.line_ending)
    {
        documents.at(index).format = format;
        editor->mark_modified();
    }
}
