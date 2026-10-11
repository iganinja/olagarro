#pragma once

#include <oui/folder_view.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

// The two folder panels beside the editor: the project tree and the file explorer.

// A dialog title saying which panel acts, such as "Explorer: Delete": both panels look alike.
[[nodiscard]] std::string panel_title(const olagarro::oui::FolderView& panel, std::string_view action);
// Makes `folder` the project: the project tree shows it, held inside it.
void open_project(olagarro::oui::FolderView& project_tree, std::optional<std::filesystem::path>& project,
                  const std::filesystem::path& folder);
// Checks a name typed for a new or renamed entry in `folder`; `current` is the entry being renamed.
[[nodiscard]] std::optional<std::string> invalid_entry_name(const std::filesystem::path& folder, std::string_view name,
                                                            const std::optional<std::filesystem::path>& current);
