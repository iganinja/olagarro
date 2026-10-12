#include "workbench.h"

#include "dialogs.h"
#include "panels.h"
#include "project_search.h"
#include "session.h"
#include "task_list.h"

#include <oui/oui.h>

#include <algorithm>
#include <vector>

using namespace olagarro;

void Workbench::register_file_commands()
{
    m_application.commands().register_command("file.new", oui::translate("Commands", "&New File"),
                                              [this]
                                              {
                                                  auto& group{m_editors.active()};
                                                  auto& tabs_reference{group.tabs};
                                                  auto& documents{group.documents};

                                                  auto document{make_editor()};
                                                  apply_tab_settings(*document, m_settings);
                                                  auto& document_reference{*document};
                                                  tabs_reference.add_tab(oui::translate("Files", "Untitled"),
                                                                         std::move(document));
                                                  documents.push_back({});
                                                  tabs_reference.select(tabs_reference.child_count() - 1);
                                                  m_application.focus(document_reference);
                                              });
    m_application.commands().register_command(
        "file.open_folder", oui::translate("Commands", "Open Fo&lder"),
        [this]
        {
            if(const auto selected{oui::file_dialog(
                   m_application, file_dialog_options(oui::FileDialogMode::open_folder, m_current_folder))})
            {
                m_current_folder = *selected;
                open_project(*m_project_tree, m_project_folder, *selected);
                add_recent(m_recent_folders, *selected);
                save_recent(m_settings, m_recent_files, m_recent_folders);
            }
        });
    m_application.commands().register_command(
        "file.close_tab", oui::translate("Commands", "&Close Tab"),
        [this]
        {
            auto& group{m_editors.active()};
            auto& tabs_reference{group.tabs};
            auto& documents{group.documents};

            const auto index{tabs_reference.selected_index()};
            auto* const document{selected_document_editor(tabs_reference)};
            if(document != nullptr && document->is_modified())
            {
                const auto action{confirm_unsaved_changes(m_application)};
                if(action == UnsavedChangesAction::cancel)
                {
                    return;
                }
                if(action == UnsavedChangesAction::save)
                {
                    static_cast<void>(m_application.commands().execute("file.save"));
                    if(document->is_modified())
                    {
                        return;
                    }
                }
            }
            // The document's previews close with it; the tab closed first, then its previews, from the last.
            std::vector<std::size_t> closing{index};
            for(std::size_t other{documents.size()}; other > 0; -- other)
            {
                if(document != nullptr && documents[other - 1].preview_of.refers_to(document))
                {
                    closing.push_back(other - 1);
                }
            }
            std::sort(closing.rbegin(), closing.rend());
            for(const auto tab : closing)
            {
                documents[tab].preview_updates.disconnect();
                documents.erase(documents.begin() + static_cast<std::ptrdiff_t>(tab));
                tabs_reference.close_tab(tab);
            }
            // Its previews in other groups go too, and a group they leave empty.
            std::vector<EditorGroup*> emptied;
            for(auto& other : m_editors.groups())
            {
                for(std::size_t tab{other.documents.size()}; &other != &group && document != nullptr && tab > 0; -- tab)
                {
                    if(other.documents[tab - 1].preview_of.refers_to(document))
                    {
                        other.documents[tab - 1].preview_updates.disconnect();
                        other.documents.erase(other.documents.begin() + static_cast<std::ptrdiff_t>(tab - 1));
                        other.tabs.close_tab(tab - 1);
                        if(other.tabs.child_count() == 0)
                        {
                            emptied.push_back(&other);
                        }
                    }
                }
            }
            for(auto* const empty : emptied)
            {
                settle_group(m_editors, *empty, m_settings);
            }
            settle_group(m_editors, group, m_settings);
            focus_current_tab(m_application, m_editors.active().tabs);
        });
    m_application.commands().register_command(
        "file.close_other_tabs", oui::translate("Commands", "Close Ot&her Tabs"),
        [this]
        {
            auto& group{m_editors.active()};
            auto& tabs_reference{group.tabs};

            auto* const keep{selected_document_editor(tabs_reference)};
            // Each tab goes through Close Tab, which asks about unsaved changes; cancelling stops here.
            for(auto index{tabs_reference.child_count()}; index > 0; -- index)
            {
                if(document_editor(tabs_reference, index - 1) == keep)
                {
                    continue;
                }
                tabs_reference.select(index - 1);
                const auto count{tabs_reference.child_count()};
                static_cast<void>(m_application.commands().execute("file.close_tab"));
                if(tabs_reference.child_count() == count)
                {
                    return;
                }
            }
            for(std::size_t index{}; index < tabs_reference.child_count(); ++ index)
            {
                if(document_editor(tabs_reference, index) == keep)
                {
                    tabs_reference.select(index);
                }
            }
            if(keep != nullptr)
            {
                m_application.focus(*keep);
            }
        });
    m_application.commands().register_command(
        "file.open", oui::translate("Commands", "&Open File"),
        [this]
        {
            const auto selected{
                oui::file_dialog(m_application, file_dialog_options(oui::FileDialogMode::open_file, m_current_folder))};
            if(!selected)
            {
                return;
            }
            if(!open_file_at_path(m_application, m_editors, *m_project_tree, m_current_folder, *selected, m_settings))
            {
                return;
            }
            m_current_folder = selected->parent_path();
            // A file opens without changing the project; with no project the explorer follows it.
            if(!m_project_folder)
            {
                m_file_explorer->show_folder(m_current_folder);
            }
            add_recent(m_recent_files, *selected);
            save_recent(m_settings, m_recent_files, m_recent_folders);
        });
    m_application.commands().register_command(
        "file.save", oui::translate("Commands", "&Save"),
        [this]
        {
            auto& group{m_editors.active()};
            auto& tabs_reference{group.tabs};
            auto& documents{group.documents};
            if(auto* const tasks{tabs_reference.selected_content<TasksView>()})
            {
                tasks->save();
                m_project_tree->refresh();
                return;
            }

            const auto index{tabs_reference.selected_index()};
            // A tab with no text of its own, such as a preview, has nothing to save.
            if(index >= documents.size() || selected_document_editor(tabs_reference) == nullptr)
            {
                return;
            }
            // A document without a file asks where to save it, as Save As does.
            const auto had_file{documents.at(index).path.has_value()};
            auto target{documents.at(index).path};
            if(!target)
            {
                // Offered a name ending as its language's files do, as Save As offers.
                auto options{file_dialog_options(oui::FileDialogMode::save_file, m_current_folder)};
                if(const auto* const language{documents.at(index).language.get()};
                   language != nullptr && !language->extensions.empty())
                {
                    options.new_file_extension = language->extensions.front();
                }
                target = oui::file_dialog(m_application, options);
                if(!target)
                {
                    return;
                }
            }
            if(auto* const document{selected_document_editor(tabs_reference)})
            {
                if(save_document(m_application, documents.at(index), *document, *target))
                {
                    tabs_reference.set_title(index, document_tab_title(*target, documents.at(index).is_interactive()));
                    m_editors.apply_languages();
                    m_current_folder = target->parent_path();
                    m_project_tree->refresh();
                    if(!had_file)
                    {
                        saved_under_new_file(documents.at(index));
                    }
                }
            }
        });
    m_application.commands().register_command(
        "file.save_all", oui::translate("Commands", "Sa&ve All"),
        [this]
        {
            auto& active{m_editors.active()};
            const auto original{active.tabs.selected_index()};
            // Each modified tab goes through Save, which asks where for a document
            // without a file, and acts on the group with the focus. A save that is
            // cancelled or fails stops here, on that tab, so the user sees which
            // one is left.
            for(auto& group : m_editors.groups())
            {
                for(std::size_t index{}; index < group.tabs.child_count(); ++ index)
                {
                    const auto* const editor{document_editor(group.tabs, index)};
                    if(editor == nullptr || !editor->is_modified())
                    {
                        continue;
                    }
                    group.tabs.select(index);
                    focus_current_tab(m_application, group.tabs);
                    static_cast<void>(m_application.commands().execute("file.save"));
                    if(editor->is_modified())
                    {
                        return;
                    }
                }
            }
            active.tabs.select(original);
            focus_current_tab(m_application, active.tabs);
        });
    m_application.commands().register_command(
        "file.save_as", oui::translate("Commands", "Save &As"),
        [this]
        {
            auto& group{m_editors.active()};
            auto& tabs_reference{group.tabs};
            auto& documents{group.documents};

            // A task list goes to the file chosen and stays there; a name not ending as a task list's gets the
            // ending, so it opens as one again.
            if(auto* const tasks{tabs_reference.selected_content<TasksView>()})
            {
                auto options{file_dialog_options(oui::FileDialogMode::save_file, m_current_folder)};
                options.directory = tasks->file().parent_path();
                options.file_name = oui::utf8_of(tasks->file().filename());
                if(auto selected{oui::file_dialog(m_application, options)})
                {
                    if(!is_task_list_file(*selected))
                    {
                        selected =
                            selected->parent_path() / oui::path_from_utf8(oui::utf8_of(selected->stem()) + ".tasks.md");
                    }
                    tasks->save_as(*selected);
                    m_current_folder = selected->parent_path();
                    m_project_tree->refresh();
                }
                return;
            }

            const auto index{tabs_reference.selected_index()};
            if(selected_document_editor(tabs_reference) == nullptr)
            {
                return;
            }
            auto options{file_dialog_options(oui::FileDialogMode::save_file, m_current_folder)};
            // Save As starts from the document's own name and folder; one with no file yet is offered a name ending
            // as its language's files do, untitled.oui for a user interface description.
            if(index < documents.size() && documents.at(index).path)
            {
                options.directory = documents.at(index).path->parent_path();
                options.file_name = oui::utf8_of(documents.at(index).path->filename());
            }
            else if(const auto* const language{index < documents.size() ? documents.at(index).language.get() : nullptr};
                    language != nullptr && !language->extensions.empty())
            {
                options.new_file_extension = language->extensions.front();
            }
            const auto selected{oui::file_dialog(m_application, options)};
            if(!selected)
            {
                return;
            }
            if(auto* const document{selected_document_editor(tabs_reference)})
            {
                if(save_document(m_application, documents.at(index), *document, *selected))
                {
                    tabs_reference.set_title(index,
                                             document_tab_title(*selected, documents.at(index).is_interactive()));
                    m_editors.apply_languages();
                    m_current_folder = selected->parent_path();
                    m_project_tree->refresh();
                    saved_under_new_file(documents.at(index));
                }
            }
        });
    m_application.commands().bind("file.new", {U'n', true});
    m_application.commands().bind("file.open", {U'o', true});
    m_application.commands().bind("file.save", {U's', true});
    m_application.commands().bind("file.save_as", {U's', true, false, true});
    m_application.commands().bind("file.close_tab", {U'w', true});
    m_application.commands().register_command(
        "file.quit", oui::translate("Commands", "&Quit"),
        [this]
        {
            // Keeping unsaved changes, as Sublime Text does: nothing to ask, everything comes back next time.
            if(!m_options.headless && m_settings.bool_value("hot_exit", false))
            {
                m_unsaved_backup.keep(false);
                m_window->close(oui::DialogResult::cancelled);
                return;
            }
            for(auto& group : m_editors.groups())
            {
                for(std::size_t index{}; index < group.tabs.child_count(); ++ index)
                {
                    const auto* const document{document_editor(group.tabs, index)};
                    if(document != nullptr && document->is_modified())
                    {
                        const auto action{confirm_unsaved_changes(m_application)};
                        if(action == UnsavedChangesAction::cancel)
                        {
                            return;
                        }
                        if(action == UnsavedChangesAction::save)
                        {
                            // Saving acts on the active group's tab.
                            group.tabs.select(index);
                            focus_current_tab(m_application, group.tabs);
                            static_cast<void>(m_application.commands().execute("file.save"));
                            if(document->is_modified())
                            {
                                return;
                            }
                        }
                    }
                }
            }
            m_window->close(oui::DialogResult::cancelled);
        });
    m_application.commands().register_command(
        "file.quick_open", oui::translate("Commands", "&Go to File..."),
        [this]
        {
            if(!m_project_folder)
            {
                show_error(m_application, oui::translate("Files", "Go to File"),
                           oui::translate("Files", "Open a folder first, to look for files in it."));
                return;
            }
            const auto files{project_files(*m_project_folder)};
            std::vector<oui::QuickPickItem> items;
            items.reserve(files.size());
            for(const auto& file : files)
            {
                items.push_back({oui::utf8_of(file), {}});
            }
            oui::QuickPickOptions options;
            options.title = oui::translate("Files", "Go to File");
            options.placeholder = oui::translate("Files", "Type part of a file name");
            options.placement_key = "go_to_file";
            if(const auto picked{oui::quick_pick(m_application, items, options)})
            {
                open_from_panel(*m_project_folder / files.at(*picked));
            }
        });
    m_application.commands().bind("file.quick_open", {U'p', true});
}

std::vector<oui::MenuItem> Workbench::recent_files_menu()
{
    std::vector<oui::MenuItem> items;
    for(std::size_t i{}; i < m_recent_files.size(); ++ i)
    {
        const auto file{m_recent_files.at(i)};
        items.push_back(oui::menu_action(recent_entry_title(file, i),
                                         [this, file]
                                         {
                                             if(open_file_at_path(m_application, m_editors, *m_project_tree,
                                                                  m_current_folder, file, m_settings))
                                             {
                                                 add_recent(m_recent_files, file);
                                                 save_recent(m_settings, m_recent_files, m_recent_folders);
                                             }
                                         }));
    }
    if(items.empty())
    {
        items.push_back(oui::menu_action(oui::translate("Menu", "(none)"), [] {}, false));
    }
    return items;
}

std::vector<oui::MenuItem> Workbench::recent_folders_menu()
{
    std::vector<oui::MenuItem> items;
    for(std::size_t i{}; i < m_recent_folders.size(); ++ i)
    {
        const auto folder{m_recent_folders.at(i)};
        items.push_back(oui::menu_action(recent_entry_title(folder, i),
                                         [this, folder]
                                         {
                                             m_current_folder = folder;
                                             open_project(*m_project_tree, m_project_folder, folder);
                                             add_recent(m_recent_folders, folder);
                                             save_recent(m_settings, m_recent_files, m_recent_folders);
                                         }));
    }
    if(items.empty())
    {
        items.push_back(oui::menu_action(oui::translate("Menu", "(none)"), [] {}, false));
    }
    return items;
}

void Workbench::open_from_panel(const std::filesystem::path& file, OpenAs as)
{
    m_current_folder = file.parent_path();
    if(!open_file_at_path(m_application, m_editors, *m_project_tree, m_current_folder, file, m_settings, nullptr, as))
    {
        return;
    }
    add_recent(m_recent_files, file);
    save_recent(m_settings, m_recent_files, m_recent_folders);
}
