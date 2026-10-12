#include "session.h"

#include "session_entry.h"
#include "task_list.h"

#include <oui/calculator_view.h>
#include <oui/terminal_view.h>

#include <oui/file_system.h>

#include <cstdlib>

using namespace olagarro;

namespace
{

constexpr std::size_t max_recent{9};

[[nodiscard]] std::string text_of(const std::filesystem::path& path)
{
    return oui::utf8_of(path);
}

// The file of the document an editor holds, wherever it is open; none for one without a file.
[[nodiscard]] std::optional<std::filesystem::path> file_of(Editors& editors, const oui::TextEdit* editor)
{
    for(auto& group : editors.groups())
    {
        for(std::size_t tab{}; tab < group.documents.size(); ++ tab)
        {
            if(document_editor(group.tabs, tab) == editor && !group.documents[tab].is_interactive())
            {
                return group.documents[tab].path;
            }
        }
    }
    return std::nullopt;
}

// What the session keeps of a tab to bring it back as it was; none for what does not come back: a document without
// a file, which the copy of unsaved documents brings back, what Find in Files found and what a script printed, which
// only held for a while, and what follows a document that has no file.
[[nodiscard]] std::optional<SessionTab> session_tab_of(Editors& editors, const EditorGroup& group, std::size_t tab)
{
    const auto& document{group.documents[tab]};
    if(const auto* const tasks{group.tabs.content_at<TasksView>(tab)})
    {
        return SessionTab{"tasks", {text_of(tasks->file())}};
    }
    if(const auto* const calculator{group.tabs.content_at<oui::CalculatorView>(tab)})
    {
        return SessionTab{"calculator", calculator->entries()};
    }
    if(group.tabs.content_at<oui::TerminalView>(tab) != nullptr)
    {
        return SessionTab{"terminal", {}};
    }
    if(const auto& session{document.interactive})
    {
        // A script with a file is read from it again; one without keeps its code here. Text pasted in the tab is
        // kept here too, and what it was generated as when it is code Generate wrote.
        const auto pasted{session->input.refers_to(&session->view->input())};
        return SessionTab{"interactive",
                          {document.path ? text_of(*document.path) : std::string{},
                           session->input_file ? text_of(*session->input_file) : std::string{},
                           document.path ? std::string{} : std::string{session->view->code().text()},
                           pasted ? "pasted" : "", pasted ? std::string{session->view->input().text()} : std::string{},
                           session->generated ? session->generated->language : std::string{},
                           session->generated ? text_of(session->generated->file) : std::string{}}};
    }
    if(const auto& session{document.interactive_ui})
    {
        // A description with a file is read from it again; one without keeps its text here.
        return SessionTab{"interactive-ui",
                          {document.path ? text_of(*document.path) : std::string{},
                           document.path ? std::string{} : std::string{session->view->code().text()}}};
    }
    if(const auto& comparison{document.comparison})
    {
        if(comparison->against_saved && comparison->right.path)
        {
            return SessionTab{"compare-saved", {text_of(*comparison->right.path)}};
        }
        if(!comparison->against_saved && comparison->left.path && comparison->right.path)
        {
            return SessionTab{"compare", {text_of(*comparison->left.path), text_of(*comparison->right.path)}};
        }
        return std::nullopt;
    }
    if(const auto* const previewed{document.preview_of.get()})
    {
        const auto file{file_of(editors, previewed)};
        return file ? std::optional<SessionTab>{SessionTab{"preview", {text_of(*file)}}} : std::nullopt;
    }
    if(document.path)
    {
        return SessionTab{"", {text_of(*document.path)}};
    }
    return std::nullopt;
}

} // namespace

std::string recent_entry_title(const std::filesystem::path& path, std::size_t index)
{
    // The number is the entry's mnemonic, so an ampersand in the name must be doubled.
    std::string name;
    for(const auto character : oui::utf8_of(path.filename()))
    {
        name += character == '&' ? std::string{"&&"} : std::string{character};
    }
    return "&" + std::to_string(index + 1) + "  " + name;
}

void add_recent(std::vector<std::filesystem::path>& list, const std::filesystem::path& given)
{
    const auto entry{oui::whole_path(given)};
    for(auto it{list.begin()}; it != list.end();)
    {
        if(*it == entry)
        {
            it = list.erase(it);
        }
        else
        {
            ++ it;
        }
    }
    list.insert(list.begin(), entry);
    if(list.size() > max_recent)
    {
        list.resize(max_recent);
    }
}

void save_recent(oui::Settings& settings, const std::vector<std::filesystem::path>& recent_files,
                 const std::vector<std::filesystem::path>& recent_folders)
{
    std::vector<std::string> files_str;
    for(const auto& f : recent_files)
    {
        files_str.push_back(oui::utf8_of(f));
    }
    settings.set_list_value("recent_files", files_str);
    std::vector<std::string> folders_str;
    for(const auto& f : recent_folders)
    {
        folders_str.push_back(oui::utf8_of(f));
    }
    settings.set_list_value("recent_folders", folders_str);
    settings.save();
}

void save_panels(oui::Settings& settings, const std::optional<std::filesystem::path>& project,
                 const oui::FolderView& project_tree, const oui::FolderView& file_explorer)
{
    settings.set_value("project_folder", project ? oui::utf8_of(*project) : "");
    settings.set_bool_value("project_tree_visible", project_tree.is_visible());
    settings.set_value("explorer_folder", oui::utf8_of(file_explorer.current_path()));
    settings.set_bool_value("explorer_visible", file_explorer.is_visible());
    static_cast<void>(settings.save());
}

#ifdef OLAGARRO_OUI_CODE_WINDOW
oui::WindowGeometry window_geometry_from(const oui::Settings& settings)
{
    oui::WindowGeometry geometry;
    const auto number{[&settings](std::string_view key) -> std::optional<int>
                      {
                          const std::string text{settings.value(key, "")};
                          if(text.empty())
                          {
                              return std::nullopt;
                          }
                          return std::atoi(text.c_str());
                      }};
    const auto x{number("window_x")};
    const auto y{number("window_y")};
    if(x && y)
    {
        geometry.position = oui::Point{*x, *y};
    }
    geometry.size.width = number("window_width").value_or(geometry.size.width);
    geometry.size.height = number("window_height").value_or(geometry.size.height);
    geometry.maximized = settings.bool_value("window_maximized", false);
    return geometry;
}

void save_window_geometry(oui::Settings& settings, const oui::DesktopWindow* window)
{
    if(window == nullptr)
    {
        return;
    }
    const auto geometry{window->geometry()};
    if(geometry.position)
    {
        settings.set_value("window_x", std::to_string(geometry.position->x));
        settings.set_value("window_y", std::to_string(geometry.position->y));
    }
    settings.set_value("window_width", std::to_string(geometry.size.width));
    settings.set_value("window_height", std::to_string(geometry.size.height));
    settings.set_bool_value("window_maximized", geometry.maximized);
}
#endif

void save_open_session(oui::Settings& settings, Editors& editors)
{
    std::vector<const EditorGroup*> order;
    settings.set_value("layout", editors.layout(order));
    const auto& active{editors.active()};
    for(std::size_t index{}; index < order.size(); ++ index)
    {
        const auto& group{*order[index]};
        std::vector<std::string> files;
        int selected{-1};
        for(std::size_t tab{}; tab < group.documents.size(); ++ tab)
        {
            const auto kept{session_tab_of(editors, group, tab)};
            if(!kept)
            {
                continue;
            }
            if(tab == group.tabs.selected_index())
            {
                selected = static_cast<int>(files.size());
            }
            files.push_back(session_entry(*kept));
        }
        const auto prefix{"layout.group." + std::to_string(index) + "."};
        settings.set_list_value(prefix + "files", files);
        settings.set_int_value(prefix + "selected", selected);
        if(&group == &active)
        {
            settings.set_int_value("layout.active", static_cast<int>(index));
        }
    }
    // The groups a bigger layout left behind go.
    for(auto index{order.size()}; index < static_cast<std::size_t>(settings.int_value("layout.groups", 0)); ++ index)
    {
        const auto prefix{"layout.group." + std::to_string(index) + "."};
        settings.remove(prefix + "files");
        settings.remove(prefix + "selected");
    }
    settings.set_int_value("layout.groups", static_cast<int>(order.size()));
    settings.save();
}
