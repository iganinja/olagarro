#include "workbench.h"

#include "dialogs.h"
#include "panels.h"

#include <oui/oui.h>

#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

using namespace olagarro;

void Workbench::register_panel_commands()
{
    for(auto* const panel : {m_project_tree, m_file_explorer})
    {
        panel->on_activated([this](const std::filesystem::path& file) { open_from_panel(file); });
        panel->on_create_requested([this, panel](const std::filesystem::path&, bool folder)
                                   { create_entry(*panel, folder); });
        panel->on_rename_requested([this, panel](const std::filesystem::path&) { rename_entry(*panel); });
        panel->on_delete_requested([this, panel](const std::filesystem::path&) { delete_entry(*panel); });
        panel->on_copy_requested([this, panel](const std::filesystem::path&) { copy_entry(*panel); });
        // The click has already put the focus on the panel the commands act on.
        panel->on_context_requested([this, panel](oui::Point at) { show_panel_menu(*panel, at); });
    }
    // From the palette, on the panel that has the focus.
    const auto on_active_panel{[this](auto action)
                               {
                                   return [this, action]
                                   {
                                       if(m_application.focused() == m_file_explorer)
                                       {
                                           action(*m_file_explorer);
                                       }
                                       else if(m_application.focused() == m_project_tree)
                                       {
                                           action(*m_project_tree);
                                       }
                                   };
                               }};
    m_application.commands().register_command(
        "files.new_file", oui::translate("Commands", "Files: New File"),
        on_active_panel([this](oui::FolderView& panel) { create_entry(panel, false); }));
    m_application.commands().register_command(
        "files.new_folder", oui::translate("Commands", "Files: New Folder"),
        on_active_panel([this](oui::FolderView& panel) { create_entry(panel, true); }));
    m_application.commands().register_command("files.rename", oui::translate("Commands", "Files: Rename"),
                                              on_active_panel([this](oui::FolderView& panel) { rename_entry(panel); }));
    m_application.commands().register_command("files.delete", oui::translate("Commands", "Files: Delete"),
                                              on_active_panel([this](oui::FolderView& panel) { delete_entry(panel); }));
    m_application.commands().register_command("files.copy",
                                              oui::translate("Commands", "Files: Copy to the Other Panel"),
                                              on_active_panel([this](oui::FolderView& panel) { copy_entry(panel); }));
}

oui::FolderView& Workbench::other_panel(const oui::FolderView& panel)
{
    return &panel == m_project_tree ? *m_file_explorer : *m_project_tree;
}

void Workbench::create_entry(oui::FolderView& panel, bool folder)
{
    const auto parent{panel.current_path()};
    if(parent.empty())
    {
        return;
    }
    oui::PromptOptions options;
    options.title =
        panel_title(panel, folder ? oui::translate("Explorer", "New Folder") : oui::translate("Explorer", "New File"));
    options.label = oui::translate("Explorer", "Name:");
    options.validate = [parent](std::string_view name) { return invalid_entry_name(parent, name, std::nullopt); };
    const auto name{oui::prompt(m_application, options)};
    if(!name)
    {
        return;
    }
    const auto target{parent / oui::path_from_utf8(*name)};
    std::error_code error;
    if(folder)
    {
        std::filesystem::create_directory(target, error);
    }
    else if(auto* const file{oui::open_file(target, "wb")})
    {
        std::fclose(file);
    }
    else
    {
        error = std::make_error_code(std::errc::io_error);
    }
    if(error)
    {
        show_error(m_application, options.title,
                   oui::format_text(oui::translate("Explorer", "Could not create {0}:\n{1}"),
                                    {oui::utf8_of(target), error.message()}));
        return;
    }
    panel.refresh(target);
    other_panel(panel).refresh();
    if(!folder)
    {
        static_cast<void>(
            open_file_at_path(m_application, m_editors, *m_project_tree, m_current_folder, target, m_settings));
    }
}

void Workbench::rename_entry(oui::FolderView& panel)
{
    const auto current{panel.selected_path()};
    if(!current)
    {
        return;
    }
    const auto parent{current->parent_path()};
    oui::PromptOptions options;
    options.title = panel_title(panel, oui::translate("Explorer", "Rename"));
    options.label = oui::translate("Explorer", "Name:");
    options.text = oui::utf8_of(current->filename());
    // The name without its extension, as file managers select it, so typing keeps the
    // extension.
    options.selection = std::pair<std::size_t, std::size_t>{0, oui::utf8_of(current->stem()).size()};
    options.validate = [parent, current](std::string_view name) { return invalid_entry_name(parent, name, current); };
    const auto name{oui::prompt(m_application, options)};
    if(!name || parent / oui::path_from_utf8(*name) == *current)
    {
        return;
    }
    const auto target{parent / oui::path_from_utf8(*name)};
    std::error_code error;
    std::filesystem::rename(*current, target, error);
    if(error)
    {
        show_error(m_application, options.title,
                   oui::format_text(oui::translate("Explorer", "Could not rename {0}:\n{1}"),
                                    {oui::utf8_of(*current), error.message()}));
        return;
    }
    // Tabs of the file, or of files inside the folder, follow it to its new name.
    for(auto& group : m_editors.groups())
    {
        for(std::size_t index{}; index < group.documents.size(); ++ index)
        {
            auto& document{group.documents[index]};
            if(!document.path)
            {
                continue;
            }
            const auto relative{oui::relative_to(*document.path, *current)};
            if(relative.empty() || oui::utf8_of(*relative.begin()) == "..")
            {
                continue;
            }
            document.path = oui::utf8_of(relative) == "." ? target : target / relative;
            group.tabs.set_title(index, document_tab_title(*document.path, document.is_interactive()));
        }
    }
    panel.refresh(target);
    other_panel(panel).refresh();
}

void Workbench::delete_entry(oui::FolderView& panel)
{
    const auto current{panel.selected_path()};
    if(!current)
    {
        return;
    }
    std::error_code error;
    const auto folder{std::filesystem::is_directory(*current, error)};
    // The whole path, under the question: the two panels look alike, and this is the one to delete.
    const auto question{
        oui::format_text(folder ? oui::translate("Explorer", "Delete this folder and everything in it?\n{0}")
                                : oui::translate("Explorer", "Delete this file?\n{0}"),
                         {oui::utf8_of(*current)})};
    const auto action{oui::translate("Explorer", "Delete")};
    const auto title{panel_title(panel, action)};
    if(oui::message_box(m_application, title, question, {action, oui::translate("Explorer", "Cancel")}) !=
       std::optional<std::size_t>{0})
    {
        return;
    }
    oui::remove_entry(*current, error);
    if(error)
    {
        show_error(m_application, title,
                   oui::format_text(oui::translate("Explorer", "Could not delete {0}:\n{1}"),
                                    {oui::utf8_of(*current), error.message()}));
    }
    // A tab of a deleted file stays open with its text; saving it writes the file again.
    panel.refresh();
    other_panel(panel).refresh();
}

void Workbench::copy_entry(oui::FolderView& panel)
{
    const auto source{panel.selected_path()};
    auto& other{other_panel(panel)};
    const auto title{panel_title(panel, oui::translate("Explorer", "Copy"))};
    if(!source)
    {
        return;
    }
    if(!other.is_visible() || other.current_path().empty())
    {
        show_error(m_application, title,
                   oui::translate("Explorer", "Show the other panel first, to say where to copy to."));
        return;
    }
    const auto target{other.current_path() / source->filename()};
    if(target == *source)
    {
        return;
    }
    const auto into_itself{oui::relative_to(other.current_path(), *source)};
    if(!into_itself.empty() && oui::utf8_of(*into_itself.begin()) != "..")
    {
        show_error(m_application, title, oui::translate("Explorer", "A folder cannot be copied into itself."));
        return;
    }
    std::error_code error;
    if(std::filesystem::exists(target, error) &&
       oui::message_box(m_application, title,
                        oui::format_text(oui::translate("Explorer", "{0} already exists in {1}. Replace it?"),
                                         {oui::utf8_of(target.filename()), oui::utf8_of(other.current_path())}),
                        {oui::translate("Explorer", "Replace"), oui::translate("Explorer", "Cancel")}) !=
           std::optional<std::size_t>{0})
    {
        return;
    }
    oui::copy_entry(*source, target, error);
    if(error)
    {
        show_error(m_application, title,
                   oui::format_text(oui::translate("Explorer", "Could not copy {0}:\n{1}"),
                                    {oui::utf8_of(*source), error.message()}));
    }
    other.refresh(target);
}

void Workbench::show_panel_menu(oui::FolderView& panel, oui::Point at)
{
    // The entries act on this panel, and show the keys that do the same in it.
    const auto selected{panel.selected_path()};
    const auto chosen{selected.has_value()};
    const auto key{[](oui::Key key) { return oui::format_shortcut(oui::Shortcut{{}, false, false, false, key}); }};
    std::vector<oui::MenuItem> items;
    // A file opens, as Enter does, or as text or a task list whatever its name says.
    std::error_code ignored;
    if(selected && std::filesystem::is_regular_file(*selected, ignored))
    {
        const auto file{*selected};
        items.push_back(oui::menu_action(oui::translate("FilesMenu", "&Open"), [this, file] { open_from_panel(file); })
                            .with_shortcut(key(oui::Key::enter)));
        items.push_back(oui::menu_submenu(
            oui::translate("FilesMenu", "Open &As"),
            std::vector<oui::MenuItem>{oui::menu_action(oui::translate("FilesMenu", "&Text"),
                                                        [this, file] { open_from_panel(file, OpenAs::text); }),
                                       oui::menu_action(
                                           oui::translate("FilesMenu", "Task &List"), [this, file]
                                           { open_from_panel(file, OpenAs::task_list); }, could_be_task_list(file))}));
        items.push_back(oui::menu_separator());
    }
    std::vector<oui::MenuItem> rest{
        oui::menu_action(oui::translate("FilesMenu", "New &File"), [this, &panel] { create_entry(panel, false); })
            .with_shortcut(key(oui::Key::insert_)),
        oui::menu_action(oui::translate("FilesMenu", "New Fold&er"), [this, &panel] { create_entry(panel, true); })
            .with_shortcut(key(oui::Key::f7)),
        oui::menu_separator(),
        oui::menu_action(
            oui::translate("FilesMenu", "&Rename"), [this, &panel] { rename_entry(panel); }, chosen)
            .with_shortcut(key(oui::Key::f2)),
        oui::menu_action(
            oui::translate("FilesMenu", "&Delete"), [this, &panel] { delete_entry(panel); }, chosen)
            .with_shortcut(key(oui::Key::delete_)),
        oui::menu_action(
            oui::translate("FilesMenu", "&Copy to the Other Panel"), [this, &panel] { copy_entry(panel); }, chosen)
            .with_shortcut(key(oui::Key::f5))};
    items.insert(items.end(), rest.begin(), rest.end());
    m_menu_bar->open_context_menu(std::move(items), at);
}
