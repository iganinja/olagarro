#include "workbench.h"

#include "dialogs.h"
#include "event_log.h"
#include "panels.h"
#include "resources.h"
#include "session.h"
#include "task_list.h"
#include "ui_descriptions.h"
#include "version.h"

#include <oui/oui.h>
#include <oui/terminal_themes.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace olagarro;

Workbench::Workbench(oui::Application& application, oui::Settings& settings, WorkbenchOptions options)
    : m_application{application}, m_settings{settings}, m_options{std::move(options)},
      m_scripts_folder{settings.file_path().parent_path() / oui::path_from_utf8("oui_code_scripts")},
      m_script_api{embedded_oui_script_api().front()},
      m_suggester{application,
                  [this]() -> std::optional<Suggester::Context>
                  {
                      auto& group{m_editors.active()};
                      const auto index{group.tabs.selected_index()};
                      auto* const editor{selected_document_editor(group.tabs)};
                      // Only in the document being written in: not in one on show while the focus is elsewhere,
                      // as on its tab, in a side panel, in the interface an Interactive UI tab builds, or in a
                      // dialog over it, where the document still counts itself focused for when it comes back.
                      if(editor == nullptr || m_application.focused() != editor || editor->is_read_only() ||
                         index >= group.documents.size())
                      {
                          return std::nullopt;
                      }
                      Suggester::Context context;
                      context.editor = editor;
                      context.language = group.documents[index].language.get();
                      context.script = context.language != nullptr && context.language->id == "javascript";
                      for(auto& each : m_editors.groups())
                      {
                          for(std::size_t tab{}; tab < each.documents.size(); ++ tab)
                          {
                              if(const auto* const document{document_editor(each.tabs, tab)})
                              {
                                  context.documents.push_back(document->text());
                              }
                          }
                      }
                      return context;
                  },
                  m_script_api},
      m_navigation{application, m_editors, [this](const std::filesystem::path& file) { open_from_panel(file); }}
{
#ifdef OLAGARRO_OUI_CODE_WINDOW
    if(m_options.window_screen != nullptr)
    {
        use_window_theme(m_settings.get("window_theme", "classic"));
        const auto zoom{m_settings.get("window_zoom", "")};
        if(!zoom.empty())
        {
            m_options.window_screen->set_zoom(std::atoi(std::string{zoom}.c_str()));
        }
    }
#endif
    if(m_options.in_cells && !m_options.headless)
    {
        use_terminal_theme(m_settings.get("terminal_theme", "turbo"));
    }
    else if(m_options.legacy_render)
    {
        use_terminal_theme("turbo");
    }
    auto ui{build_oui_code_ui(OuiCodeUi::main_window)};
    auto window{ui.take_window()};
    m_window = window.get();
    m_application.commands().register_command("file.quit", oui::translate("Commands", "&Quit"), [] {});
    m_application.commands().bind("file.quit", {U'q', true});
    m_application.commands().register_command("view.command_palette", oui::translate("Commands", "Command Palette"),
                                              [this] { show_palette(m_application); });
    // Ctrl+P goes to a file and Ctrl+Shift+P opens the palette, as in code editors; F1 is the help.
    m_application.commands().bind("view.command_palette", {U'p', true, false, true});
    auto& workspace{ui.get<oui::DockLayout>("workspace")};
    build_menu_bar(workspace);
    build_side_panels(workspace);
    prepare_editor_tabs();
    {
        auto problems{load_scripts()};
        m_application.post([this, problems{std::move(problems)}] { report_script_problems(problems); });
    }
    if(!m_options.headless)
    {
        remember_dialog_places();
    }
    for(const auto& p : m_settings.get_list("recent_files"))
    {
        m_recent_files.push_back(oui::path_from_utf8(p));
    }
    for(const auto& p : m_settings.get_list("recent_folders"))
    {
        m_recent_folders.push_back(oui::path_from_utf8(p));
    }
    auto editor{make_editor()};
    apply_tab_settings(*editor, m_settings);
    auto& editor_reference{*editor};
    auto find_bar{std::make_unique<FindBar>(m_application, editor_reference)};
    m_find_bar = find_bar.get();
    register_edit_commands();
    open_first_document(std::move(editor));
    register_line_commands();
    watch_external_changes();
    register_format_commands();
    restore_session();
    register_file_commands();
    watch_events();
    register_tool_commands();
    register_compare_commands();
    fill_menus();
    register_search_commands();
    register_panel_commands();
    // The find bar sits under the document only, then the tabs fill what is left.
    workspace.add(oui::Dock::bottom, std::move(find_bar));
    workspace.add(oui::Dock::fill, std::move(m_editor_area));
    install_status_bar();
    register_view_commands();
    if(m_options.compare)
    {
        auto left{compared_file(m_options.compare->first)};
        auto right{left ? compared_file(m_options.compare->second) : std::nullopt};
        if(left && right)
        {
            open_comparison(std::move(*left), std::move(*right));
        }
    }
    finish_window(std::move(window));
}

int Workbench::run()
{
    if(!m_options.headless)
    {
        m_unsaved_backup.configure();
    }
    const auto result{m_application.exec()};
    if(!m_options.headless)
    {
        // A quit that asked about every unsaved document leaves nothing to bring back; one that kept
        // them, or a shutdown, has just written the copy the next start reopens.
        if(!m_application.is_shutting_down() && !m_settings.get_bool("hot_exit", false))
        {
            m_unsaved_backup.forget();
        }
        save_open_session(m_settings, m_editors);
        save_panels(m_settings, m_project_folder, *m_project_tree, *m_file_explorer);
    }
    return result;
}

const oui::TextEdit* Workbench::shown_document()
{
    return selected_document_editor(m_editors.active().tabs);
}

void Workbench::build_menu_bar(oui::DockLayout& workspace)
{
    auto menu_bar{oui::menu_bar(m_application.commands())};
    m_menu_bar = menu_bar.get();
    menu_bar->set_visible(!m_options.no_menu);
    // Their entries come from fill_menus, once the commands in them are there.
    menu_bar->add_menu(oui::translate("Menu", "&File"), {});
    menu_bar->add_menu(oui::translate("Menu", "&Edit"), {});
    menu_bar->add_menu(oui::translate("Menu", "&View"), {});
    menu_bar->add_menu(oui::translate("Menu", "&Tools"), {});
    menu_bar->add_menu(oui::translate("Menu", "&Help"), {});
    workspace.add(oui::Dock::top, std::move(menu_bar));
}

std::vector<oui::MenuItem> Workbench::file_menu()
{
    return {"file.new",
            "view.new_tab",
            "file.open",
            "file.open_folder",
            "file.quick_open",
            oui::menu_separator(),
            oui::menu_submenu(oui::translate("Menu", "Recent &Files"), [this] { return recent_files_menu(); }),
            oui::menu_submenu(oui::translate("Menu", "Recent Fol&ders"), [this] { return recent_folders_menu(); }),
            oui::menu_separator(),
            "file.save",
            "file.save_as",
            "file.save_all",
            oui::menu_separator(),
            "file.close_tab",
            "file.close_other_tabs",
            "file.quit"};
}

std::vector<oui::MenuItem> Workbench::edit_menu()
{
    return {"edit.undo",
            "edit.redo",
            oui::menu_separator(),
            "edit.copy",
            "edit.cut",
            "edit.paste",
            "edit.select_all",
            oui::menu_separator(),
            "edit.duplicate_lines",
            "edit.move_lines_up",
            "edit.move_lines_down",
            "edit.delete_lines",
            "edit.toggle_comment",
            "edit.add_cursor_above",
            "edit.add_cursor_below",
            oui::menu_separator(),
            "edit.find",
            "edit.find_next",
            "edit.find_previous",
            "edit.find_selected",
            "edit.replace",
            "edit.find_in_files",
            "edit.go_to_line",
            "edit.go_to_bracket",
            "edit.go_back",
            "edit.go_forward",
            oui::menu_separator(),
            "edit.preferences"};
}

std::vector<oui::MenuItem> Workbench::help_menu()
{
    return {"help.contents", "help.shortcuts", "help.about"};
}

void Workbench::fill_menus()
{
    m_menu_bar->set_menu_items(0, with_text_commands("file", file_menu()));
    m_menu_bar->set_menu_items(1, with_text_commands("edit", edit_menu()));
    m_menu_bar->set_menu_items(2, with_text_commands("view", view_menu()));
    m_menu_bar->set_menu_items(3, with_text_commands("tools", tools_menu()));
    m_menu_bar->set_menu_items(4, with_text_commands("help", help_menu()));
}

void Workbench::build_side_panels(oui::DockLayout& workspace)
{
    // The project tree on the left, held inside the project; the explorer on the right, free to roam the whole
    // file system and bring things into the project.
    auto project_tree_panel{oui::folder_view()};
    project_tree_panel->set_colors(&oui::Theme::side_panel_background, &oui::Theme::panel_foreground);
    m_project_tree = project_tree_panel.get();
    auto file_explorer_panel{oui::folder_view()};
    file_explorer_panel->set_colors(&oui::Theme::side_panel_background, &oui::Theme::panel_foreground);
    file_explorer_panel->set_visible(false);
    m_file_explorer = file_explorer_panel.get();
    if(!m_options.headless && m_options.folder)
    {
        m_current_folder = *m_options.folder;
    }
    else if(!m_options.headless && !m_options.files.empty())
    {
        m_current_folder = m_options.files.front();
    }
    std::error_code unreadable;
    const auto opened_a_folder{!m_current_folder.empty() &&
                               std::filesystem::is_directory(m_current_folder, unreadable)};
    if(!m_current_folder.empty() && std::filesystem::is_regular_file(m_current_folder, unreadable))
    {
        m_current_folder = m_current_folder.parent_path();
    }
    if(opened_a_folder)
    {
        open_project(*m_project_tree, m_project_folder, m_current_folder);
    }
    else if(!m_current_folder.empty())
    {
        m_file_explorer->show_folder(m_current_folder);
    }
    m_project_tree->set_visible(!m_options.no_explorer && opened_a_folder);
    // Started with no folder to open, OUI Code comes back to the project and panels the user left.
    if(!m_options.headless && !m_options.folder)
    {
        const auto saved_project{oui::path_from_utf8(m_settings.get("project_folder"))};
        std::error_code error;
        if(!saved_project.empty() && std::filesystem::is_directory(saved_project, error))
        {
            open_project(*m_project_tree, m_project_folder, saved_project);
            m_current_folder = saved_project;
            m_project_tree->set_visible(m_settings.get_bool("project_tree_visible", true) && !m_options.no_explorer);
        }
        const auto saved_explorer{oui::path_from_utf8(m_settings.get("explorer_folder"))};
        if(!saved_explorer.empty() && std::filesystem::is_directory(saved_explorer, error))
        {
            m_file_explorer->show_folder(saved_explorer);
            m_file_explorer->set_visible(m_settings.get_bool("explorer_visible", false));
        }
    }
    // The status bar spans the whole width under everything; the explorer runs down the left above it.
    m_status_bar = &workspace.add(oui::Dock::bottom, oui::status_bar());
    // The side panels are as wide as they were last dragged.
    auto& project_tree_strip{
        workspace.add(oui::Dock::left, std::move(project_tree_panel), m_settings.get_int("project_tree_width", 25))};
    auto& file_explorer_strip{
        workspace.add(oui::Dock::right, std::move(file_explorer_panel), m_settings.get_int("explorer_width", 31))};
    workspace.set_resizable(project_tree_strip);
    workspace.set_resizable(file_explorer_strip);
    workspace.on_resized(
        [this, tree_strip{&project_tree_strip}](const oui::Widget& strip, int width)
        {
            m_settings.set_int(&strip == tree_strip ? "project_tree_width" : "explorer_width", width);
            static_cast<void>(m_settings.save());
        });
}

void Workbench::prepare_editor_tabs()
{
    m_editors.open_context_menu = [this](std::vector<oui::MenuItem> items, oui::Point at)
    { m_menu_bar->open_context_menu(std::move(items), at); };
    m_editors.prepare_tabs(
        [this](oui::TabView& group_tabs)
        {
            auto* const tabs{&group_tabs};
            // A tab's title is the file's name; resting the pointer on it shows the whole path.
            tabs->set_tooltip_provider(
                [this, tabs](std::size_t index) -> std::optional<std::string>
                {
                    if(const auto* const view{tabs->content_at<TasksView>(index)})
                    {
                        return shown_path(view->file());
                    }
                    for(const auto& group : m_editors.groups())
                    {
                        if(&group.tabs == tabs && index < group.documents.size() && group.documents[index].path)
                        {
                            return shown_path(*group.documents[index].path);
                        }
                    }
                    return std::nullopt;
                });
            tabs->set_highlight_predicate(
                [tabs](std::size_t index)
                {
                    const auto* const document{document_editor(*tabs, index)};
                    return document != nullptr && document->is_modified();
                });
            // The tab closes in its own group, which the focus moves into first.
            tabs->on_close_requested(
                [this, tabs](std::size_t)
                {
                    focus_current_tab(m_application, *tabs);
                    static_cast<void>(m_application.commands().execute("file.close_tab"));
                });
            tabs->on_tab_activated([this, tabs](std::size_t) { focus_current_tab(m_application, *tabs); });
            // A tab dragged by its title: let go on a tab of its own group it takes that place; anywhere on another
            // group it moves there, before the tab it lands on. After the drag, as the group it leaves may close.
            tabs->on_tab_dropped(
                [this, tabs](std::size_t index, oui::Point where)
                {
                    m_application.post(
                        [this, tabs, index, where]
                        {
                            const auto inside{[](const oui::Rect& area, oui::Point point) {
                                return point.x >= area.x && point.x < area.x + area.width && point.y >= area.y &&
                                       point.y < area.y + area.height;
                            }};
                            EditorGroup* from{};
                            EditorGroup* to{};
                            for(auto& group : m_editors.groups())
                            {
                                from = &group.tabs == tabs ? &group : from;
                                to = inside(group.tabs.bounds(), where) ? &group : to;
                            }
                            if(from == nullptr || to == nullptr || index >= from->documents.size())
                            {
                                return;
                            }
                            const auto landed_on{where.y == to->tabs.bounds().y ? to->tabs.tab_at(where.x)
                                                                                : std::nullopt};
                            if(to == from)
                            {
                                if(landed_on)
                                {
                                    reorder_tab(*from, index, *landed_on);
                                }
                                return;
                            }
                            move_tab(*from, index, *to);
                            if(landed_on)
                            {
                                reorder_tab(*to, to->documents.size() - 1, *landed_on);
                            }
                            settle_group(m_editors, *from, m_settings);
                            focus_current_tab(m_application, to->tabs);
                        });
                });
        });
}

// Dialogs reopen where the user left them: window.<key>.x/y when moved, .width/.height when resized.
void Workbench::remember_dialog_places()
{
    m_application.set_window_placement_store(
        [this](std::string_view key) -> std::optional<oui::WindowPlacement>
        {
            const auto prefix{"window." + std::string{key} + "."};
            const auto read{[this, &prefix](const char* const field) -> std::optional<int>
                            {
                                if(m_settings.get(prefix + field).empty())
                                {
                                    return std::nullopt;
                                }
                                return m_settings.get_int(prefix + field);
                            }};
            oui::WindowPlacement placement;
            if(const auto x{read("x")}, y{read("y")}; x && y)
            {
                placement.position = oui::Point{*x, *y};
            }
            if(const auto width{read("width")}, height{read("height")}; width && height)
            {
                placement.size = oui::Size{*width, *height};
            }
            return placement;
        },
        [this](std::string_view key, const oui::WindowPlacement& placement)
        {
            const auto prefix{"window." + std::string{key} + "."};
            const auto write{[this, &prefix](const char* const field, std::optional<int> value)
                             { m_settings.set(prefix + field, value ? std::to_string(*value) : ""); }};
            write("x", placement.position ? std::optional<int>{placement.position->x} : std::nullopt);
            write("y", placement.position ? std::optional<int>{placement.position->y} : std::nullopt);
            write("width", placement.size ? std::optional<int>{placement.size->width} : std::nullopt);
            write("height", placement.size ? std::optional<int>{placement.size->height} : std::nullopt);
            static_cast<void>(m_settings.save());
        });
}

void Workbench::open_file_to_make(const std::filesystem::path& file)
{
    // Its tab, when unsaved work kept from before has it open already.
    for(auto& group : m_editors.groups())
    {
        for(std::size_t index{}; index < group.documents.size(); ++ index)
        {
            if(group.documents[index].path == file)
            {
                group.tabs.select(index);
                focus_current_tab(m_application, group.tabs);
                return;
            }
        }
    }
    auto& group{m_editors.active()};
    drop_lone_empty_tab(group.tabs, group.documents);
    auto document{make_editor()};
    apply_tab_settings(*document, m_settings);
    auto& document_reference{*document};
    group.tabs.add_tab(document_tab_title(file, false), std::move(document));
    // Its path and no stamp: no file yet to notice changes in, until it is saved.
    group.documents.emplace_back();
    group.documents.back().path = file;
    group.tabs.select(group.tabs.child_count() - 1);
    m_editors.apply_languages(true);
    m_application.focus(document_reference);
}

void Workbench::open_first_document(std::unique_ptr<oui::TextEdit> editor)
{
    // Untitled, until the session or the files given take its place.
    m_editors.groups().front().tabs.add_tab(oui::translate("Files", "Untitled"), std::move(editor));
    m_editors.groups().front().documents.emplace_back();
    m_editors.apply_languages();
}

// Changes made to a file by someone else are looked for when its tab is shown and when the terminal gets the focus
// back. Deferred with post so no dialog opens in the middle of the event that caused it.
void Workbench::watch_external_changes()
{
    const auto check_active_document{
        [this]
        {
            m_application.post(
                [this]
                {
                    auto& group{m_editors.active()};
                    auto& tabs_reference{group.tabs};
                    auto& documents{group.documents};

                    const auto index{tabs_reference.selected_index()};
                    auto* const editor{selected_document_editor(tabs_reference)};
                    if(editor != nullptr && index < documents.size())
                    {
                        check_external_change(m_application, documents.at(index), *editor,
                                              m_settings.get_bool("reload_external_changes", true));
                    }
                });
        }};
    m_editors.prepare_tabs(
        [check_active_document](oui::TabView& tabs)
        { tabs.on_selection_changed([check_active_document](std::size_t) { check_active_document(); }); });
    m_application.on_terminal_focus_changed(
        [check_active_document](bool focused)
        {
            if(focused)
            {
                check_active_document();
            }
        });
}

oui::TextEdit* Workbench::open_document_editor(const std::filesystem::path& file)
{
    for(auto& group : m_editors.groups())
    {
        for(std::size_t tab{}; tab < group.documents.size(); ++ tab)
        {
            if(group.documents[tab].path == file && !group.documents[tab].is_interactive())
            {
                return document_editor(group.tabs, tab);
            }
        }
    }
    return nullptr;
}

bool Workbench::restore_tab(const SessionTab& tab)
{
    const auto path{[&tab](std::size_t part) { return oui::path_from_utf8(tab.parts.at(part)); }};
    std::error_code ec;
    if(tab.kind == "calculator")
    {
        std::vector<std::string> entries;
        std::copy_if(tab.parts.begin(), tab.parts.end(), std::back_inserter(entries),
                     [](const std::string& entry) { return !entry.empty(); });
        open_calculator(entries);
        return true;
    }
    if(tab.kind == "terminal")
    {
        const auto before{m_editors.active().tabs.child_count()};
        open_terminal();
        return m_editors.active().tabs.child_count() > before;
    }
    if(tab.kind == "preview")
    {
        auto* const editor{open_document_editor(path(0))};
        if(editor != nullptr)
        {
            open_markdown_preview(*editor, title_for(path(0)));
        }
        return editor != nullptr;
    }
    if(tab.kind == "compare-saved")
    {
        auto* const editor{open_document_editor(path(0))};
        if(editor == nullptr || !std::filesystem::is_regular_file(path(0), ec))
        {
            return false;
        }
        compare_with_saved(*editor, path(0));
        return true;
    }
    if(tab.kind == "compare" && tab.parts.size() == 2)
    {
        if(!std::filesystem::is_regular_file(path(0), ec) || !std::filesystem::is_regular_file(path(1), ec))
        {
            return false;
        }
        auto left{compared_file(path(0))};
        auto right{left ? compared_file(path(1)) : std::nullopt};
        if(left && right)
        {
            open_comparison(std::move(*left), std::move(*right));
        }
        return left && right;
    }
    // Seven parts since text can be pasted in the tab; three in a session kept before.
    if(tab.kind == "interactive" && (tab.parts.size() == 3 || tab.parts.size() == 7))
    {
        InteractiveStart start;
        if(!tab.parts[0].empty())
        {
            start.script_file = path(0);
            const auto code{oui::read_file(*start.script_file)};
            if(!code)
            {
                return false;
            }
            start.code = *code;
        }
        else
        {
            start.code = tab.parts[2];
        }
        if(!tab.parts[1].empty())
        {
            start.input_file = path(1);
            start.input = open_document_editor(*start.input_file);
            start.input_name = title_for(*start.input_file);
        }
        if(tab.parts.size() == 7 && tab.parts[3] == "pasted")
        {
            start.pasted = tab.parts[4];
            if(!tab.parts[5].empty())
            {
                start.generated = GeneratedFrom{tab.parts[5], path(6)};
            }
        }
        start_interactive_script(std::move(start));
        return true;
    }
    if(tab.kind == "interactive-ui" && tab.parts.size() == 2)
    {
        if(tab.parts[0].empty())
        {
            start_interactive_ui(std::nullopt, tab.parts[1]);
            return true;
        }
        const auto text{oui::read_file(path(0))};
        if(!text)
        {
            return false;
        }
        start_interactive_ui(path(0), *text);
        return true;
    }
    return false;
}

void Workbench::restore_session()
{
    if(!m_options.headless && !m_options.files.empty())
    {
        for(const auto& file : m_options.files)
        {
            std::error_code unreadable;
            if(std::filesystem::is_regular_file(file, unreadable))
            {
                add_recent(m_recent_files, file);
            }
        }
        save_recent(m_settings, m_recent_files, m_recent_folders);
    }
    // The session comes back as it was: its groups, their tabs in their order and the group with the focus. A group
    // none of whose tabs could be brought back does not. Files and task lists first, everywhere, then what follows
    // them, as a comparison or a preview, which finds them open.
    if(!m_options.headless)
    {
        const auto groups{m_editors.restore_layout(m_settings.get("layout"))};
        std::vector<std::vector<SessionTab>> kept(groups.size());
        std::vector<std::vector<bool>> back(groups.size());
        for(std::size_t index{}; index < groups.size(); ++ index)
        {
            if(groups[index] == nullptr)
            {
                continue;
            }
            for(const auto& entry : m_settings.get_list("layout.group." + std::to_string(index) + ".files"))
            {
                kept[index].push_back(session_tab(entry));
            }
            back[index].assign(kept[index].size(), false);
            for(std::size_t tab{}; tab < kept[index].size(); ++ tab)
            {
                const auto& each{kept[index][tab]};
                if(each.kind == "tasks")
                {
                    // A task list comes back even before it has a file, as one with no tasks yet.
                    open_task_list(m_application, m_editors, oui::path_from_utf8(each.parts.front()), groups[index]);
                    back[index][tab] = true;
                }
                else if(each.kind.empty())
                {
                    const auto saved{oui::path_from_utf8(each.parts.front())};
                    std::error_code ec;
                    back[index][tab] = std::filesystem::is_regular_file(saved, ec) &&
                                       open_file_at_path(m_application, m_editors, *m_project_tree, m_current_folder,
                                                         saved, m_settings, groups[index], OpenAs::text);
                }
            }
        }
        for(std::size_t index{}; index < groups.size(); ++ index)
        {
            for(std::size_t tab{}; groups[index] != nullptr && tab < kept[index].size(); ++ tab)
            {
                const auto& each{kept[index][tab]};
                if(each.kind.empty() || each.kind == "tasks")
                {
                    continue;
                }
                m_editors.set_active(*groups[index]);
                if(restore_tab(each))
                {
                    // Where it was among those that came back.
                    const auto at{static_cast<std::size_t>(
                        std::count(back[index].begin(), back[index].begin() + static_cast<std::ptrdiff_t>(tab), true))};
                    reorder_tab(*groups[index], groups[index]->tabs.child_count() - 1, at);
                    back[index][tab] = true;
                }
            }
            const auto selected{m_settings.get_int("layout.group." + std::to_string(index) + ".selected", -1)};
            if(groups[index] != nullptr && selected >= 0 && static_cast<std::size_t>(selected) < back[index].size() &&
               back[index][static_cast<std::size_t>(selected)])
            {
                groups[index]->tabs.select(
                    static_cast<std::size_t>(std::count(back[index].begin(), back[index].begin() + selected, true)));
            }
        }
        for(auto* const group : groups)
        {
            if(group != nullptr)
            {
                settle_group(m_editors, *group, m_settings);
            }
        }
        const auto active{static_cast<std::size_t>(std::max(0, m_settings.get_int("layout.active", 0)))};
        auto* const focused{active < groups.size() && groups[active] != nullptr ? groups[active]
                                                                                : &m_editors.groups().front()};
        // A group left empty is gone; the first one is always there.
        const auto still_there{std::any_of(m_editors.groups().begin(), m_editors.groups().end(),
                                           [focused](const EditorGroup& each) { return &each == focused; })};
        m_editors.set_active(still_there ? *focused : m_editors.groups().front());
    }
    // Then the files given, over what came back, the last in front; once the window shows, so one that does not open
    // says why over it.
    if(!m_options.headless && !m_options.files.empty())
    {
        m_application.post(
            [this]
            {
                for(const auto& file : m_options.files)
                {
                    std::error_code unknown;
                    if(std::filesystem::exists(file, unknown) || unknown)
                    {
                        static_cast<void>(open_file_at_path(m_application, m_editors, *m_project_tree, m_current_folder,
                                                            file, m_settings));
                    }
                    else
                    {
                        open_file_to_make(file);
                    }
                }
            });
    }
    auto& first_group{m_editors.groups().front()};
    const auto reopened{m_options.headless
                            ? Reopened::nothing
                            : reopen_unsaved_documents(first_group.tabs, first_group.documents, m_settings)};
    m_editors.apply_languages();
    if(reopened == Reopened::with_notice)
    {
        first_group.tabs.select(first_group.tabs.child_count() - 1);
        m_application.post(
            [this]
            {
                static_cast<void>(oui::message_box(
                    m_application, oui::translate("Files", "Unsaved Changes Recovered"),
                    oui::translate("Files", "OUI Code closed last time with unsaved changes.\nThe documents "
                                            "that had them are open again, still unsaved.")));
            });
    }
}

void Workbench::install_status_bar()
{
    m_status_bar->set_provider(
        [this]() -> oui::StatusText
        {
            auto& group{m_editors.active()};
            auto& tabs_reference{group.tabs};
            auto& documents{group.documents};

            const auto version{"OUI Code v" + std::string{oui_code_version}};
            // The keys that matter most first, as Turbo C++'s status line has them; the fewest when room is short.
            std::vector<oui::StatusKey> keys;
            const auto narrow{m_status_bar->bounds().width < 110};
            // Save only where there is a document to save: not on the calculator, a terminal or a list.
            const auto* const document{selected_document_editor(tabs_reference)};
            for(const auto& [command, label] : {std::pair{"help.contents", oui::translate("StatusBar", "Help")},
                                                std::pair{"file.save", oui::translate("StatusBar", "Save")},
                                                std::pair{"file.open", oui::translate("StatusBar", "Open")},
                                                std::pair{"view.open_menu", oui::translate("StatusBar", "Menu")}})
            {
                const auto shortcut{m_application.commands().shortcut(command)};
                const auto save{std::string_view{command} == "file.save"};
                if(shortcut && !(save && document == nullptr) &&
                   !(narrow && (save || std::string_view{command} == "file.open")))
                {
                    keys.push_back({oui::format_shortcut(*shortcut), label});
                }
            }
            if(document == nullptr)
            {
                return {"", version, keys};
            }
            const auto [line, column]{document->line_column()};
            const auto index{tabs_reference.selected_index()};
            const auto format{index < documents.size() ? documents.at(index).format : oui::TextFormat{}};
            // The document's whole path, which is what tells two files of the same name apart, cut from its start
            // when the rest would not fit.
            const auto rest{std::string{document->is_modified() ? " *" : ""} + " | " +
                            std::string{oui::encoding_name(format.encoding)} + " | " +
                            std::string{oui::line_ending_name(format.line_ending)} + " | " +
                            (index < documents.size() && documents.at(index).language
                                 ? documents.at(index).language->name
                                 : oui::translate("StatusBar", "Plain Text")) +
                            " | " +
                            oui::format_text(oui::translate("StatusBar", "Ln {0}, Col {1}"),
                                             {std::to_string(line), std::to_string(column)})};
            const auto name{index < documents.size() && documents.at(index).path
                                ? shown_path(*documents.at(index).path)
                                : std::string{tabs_reference.title(index)}};
            const auto room{m_status_bar->bounds().width - oui::status_keys_width(keys) - oui::text_width(version) -
                            oui::text_width(rest) - 1};
            return {fit_from_start(name, std::max(8, room)) + rest, version, keys};
        });
}

void Workbench::watch_events()
{
    m_application.set_event_observer(
        [this](const oui::Event& event)
        {
            if(m_options.event_log != nullptr)
            {
                record_event(m_options.event_log, event);
            }
            m_navigation.remember();
            // Once the event is handled, the suggestions follow what it did: what was typed may open them.
            std::optional<char32_t> typed;
            if(const auto* const text{std::get_if<oui::TextEvent>(&event)};
               text != nullptr && !text->control && !text->alt)
            {
                typed = text->codepoint;
            }
            // A terminal sends the space bar as a key: it is typed all the same.
            if(const auto* const key{std::get_if<oui::KeyEvent>(&event)};
               key != nullptr && key->key == oui::Key::space && !key->control && !key->alt)
            {
                typed = U' ';
            }
            m_application.post([this, typed] { m_suggester.after_event(typed); });
        });
}

void Workbench::finish_window(std::unique_ptr<oui::Window> window)
{
    if(!m_options.headless)
    {
        window->on_closed(
            [this](oui::DialogResult)
            {
                remember_window();
                save_open_session(m_settings, m_editors);
                save_panels(m_settings, m_project_folder, *m_project_tree, *m_file_explorer);
            });
    }
    // The close button of a window, or the desktop asking to close it, quits as Ctrl+Q does: asking about unsaved work.
    m_application.on_close_requested([this] { static_cast<void>(m_application.commands().execute("file.quit")); });
    if(!m_options.headless)
    {
        m_application.on_shutdown(
            [this](oui::ShutdownEvent::Reason)
            {
                remember_window();
                m_unsaved_backup.keep(!m_settings.get_bool("hot_exit", false));
            });
    }
    m_application.set_window(std::move(window));
    if(auto* const active_editor{selected_document_editor(m_editors.active().tabs)})
    {
        m_application.focus(*active_editor);
    }
    else if(auto* const tasks{m_editors.active().tabs.selected_content<TasksView>()})
    {
        tasks->focus_list();
    }
    else
    {
        // What else a tab holds, as a comparison.
        focus_current_tab(m_application, m_editors.active().tabs);
    }
}

void Workbench::remember_window()
{
#ifdef OLAGARRO_OUI_CODE_WINDOW
    save_window_geometry(m_settings, m_options.window_screen);
    static_cast<void>(m_settings.save());
#endif
}

#ifdef OLAGARRO_OUI_CODE_WINDOW
void Workbench::use_window_theme(std::string_view id)
{
    for(const auto& theme : oui::graphical_themes())
    {
        if(theme.id == id)
        {
            m_options.window_screen->set_style(theme.style);
            m_application.set_theme(oui::theme_for(theme.style));
        }
    }
}
#endif

void Workbench::use_terminal_theme(std::string_view id)
{
    for(auto& [theme_id, name, theme] : oui::terminal_themes())
    {
        if(theme_id == id)
        {
            if(m_options.legacy_render)
            {
                // Under the classic console, buttons in plain green.
                theme.button_background = {0, 255, 0};
            }
            m_application.set_theme(theme);
        }
    }
}
