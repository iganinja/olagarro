#include "panels.h"

#include <oui/file_system.h>
#include <oui/translation.h>

#include <system_error>

using namespace olagarro;

std::string panel_title(const oui::FolderView& panel, std::string_view action)
{
    return oui::format_text(oui::translate("Explorer", "{0}: {1}"), {panel.name(), action});
}

void open_project(oui::FolderView& project_tree, std::optional<std::filesystem::path>& project,
                  const std::filesystem::path& folder)
{
    project = folder;
    project_tree.set_scope(folder);
    project_tree.show_folder(folder);
    project_tree.set_visible(true);
}

std::optional<std::string> invalid_entry_name(const std::filesystem::path& folder, std::string_view name,
                                              const std::optional<std::filesystem::path>& current)
{
    if(name.empty() || name == "." || name == "..")
    {
        return std::string{oui::translate("Explorer", "Enter a name.")};
    }
    if(name.find_first_of("/\\") != std::string_view::npos)
    {
        return std::string{oui::translate("Explorer", "A name cannot contain / or \\.")};
    }
    std::error_code error;
    const auto target{folder / oui::path_from_utf8(name)};
    if(std::filesystem::exists(target, error) && target != current)
    {
        return std::string{oui::translate("Explorer", "Something with that name already exists.")};
    }
    return std::nullopt;
}
