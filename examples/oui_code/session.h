#pragma once

#include "editors.h"

#include <oui/folder_view.h>
#include <oui/settings.h>
#ifdef OLAGARRO_OUI_CODE_WINDOW
#include <oui/graphics/window_backend.h>
#endif

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// What OUI Code keeps from one run to the next in its settings: the recent files and folders, the project and
// its panels, the window, and the groups with the files open in them.

// A recent file or folder as its menu entry shows it, numbered from 1 as its mnemonic.
[[nodiscard]] std::string recent_entry_title(const std::filesystem::path& path, std::size_t index);
// Puts `given` first in `list`, once, keeping the newest few.
void add_recent(std::vector<std::filesystem::path>& list, const std::filesystem::path& given);
void save_recent(olagarro::oui::Settings& settings, const std::vector<std::filesystem::path>& recent_files,
                 const std::vector<std::filesystem::path>& recent_folders);
// The project and the panels as the user leaves them, to find them so again on the next start.
void save_panels(olagarro::oui::Settings& settings, const std::optional<std::filesystem::path>& project,
                 const olagarro::oui::FolderView& project_tree, const olagarro::oui::FolderView& file_explorer);

#ifdef OLAGARRO_OUI_CODE_WINDOW
// Where the window was left, as save_window_geometry keeps it.
[[nodiscard]] olagarro::oui::WindowGeometry window_geometry_from(const olagarro::oui::Settings& settings);
void save_window_geometry(olagarro::oui::Settings& settings, const olagarro::oui::DesktopWindow* window);
#endif

// The groups as they lie, the tabs in each and the one each shows, and which group has the focus, to come back as they
// were: each tab as session_entry writes it, a file's as its path, the rest as what they are. Documents without a file
// are not part of it: the copy of unsaved documents brings them back.
void save_open_session(olagarro::oui::Settings& settings, Editors& editors);
