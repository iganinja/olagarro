#pragma once

#include <oui/application.h>
#include <oui/geometry.h>
#include <oui/translation.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace olagarro::oui
{

enum class FileDialogMode
{
    open_file,
    open_folder,
    save_file
};

// A named set of patterns separated by ';', such as {"C++ sources", "*.cpp;*.h"}. "*" matches everything.
struct FileFilter
{
    std::string name;
    std::string patterns;
};

// Every piece of text the dialog shows: English, or its translation in the installed catalogs.
struct FileDialogTexts
{
    std::string open_file_title{translate("FileDialog", "Open File")};
    std::string open_folder_title{translate("FileDialog", "Open Folder")};
    std::string save_title{translate("FileDialog", "Save As")};
    std::string location{translate("FileDialog", "&Location:")};
    std::string file_name{translate("FileDialog", "File &name:")};
    std::string file_type{translate("FileDialog", "File &type:")};
    std::string open{translate("FileDialog", "&Open")};
    std::string select_folder{translate("FileDialog", "&Select Folder")};
    std::string save{translate("FileDialog", "&Save")};
    std::string cancel{translate("FileDialog", "Cancel")};
    std::string overwrite{translate("FileDialog", "Overwrite File?")};
    std::string all_files{translate("FileDialog", "All files (*)")};
    std::string places{translate("FileDialog", "Places")};
    std::string home{translate("FileDialog", "Home")};
    std::string desktop{translate("FileDialog", "Desktop")};
    std::string documents{translate("FileDialog", "Documents")};
    std::string downloads{translate("FileDialog", "Downloads")};
    std::string name_column{translate("FileDialog", "Name")};
    std::string size_column{translate("FileDialog", "Size")};
    std::string type_column{translate("FileDialog", "Type")};
    std::string parent_folder{translate("FileDialog", "[..]")};
    std::string folder_marker{translate("FileDialog", "[D] ")};
    // The name a new file is saved under unless another is typed.
    std::string new_file_name{translate("FileDialog", "untitled.txt")};
    std::string enter_a_name{translate("FileDialog", "Enter a file name.")};
    std::string file_not_found{translate("FileDialog", "File not found: {0}")};
    std::string folder_not_found{translate("FileDialog", "Folder not found: {0}")};
    std::string overwrite_hint{translate("FileDialog", "A file with that name exists. Press again to overwrite.")};
    std::string new_folder{translate("FileDialog", "New &Folder")};
    std::string new_folder_title{translate("FileDialog", "New Folder")};
    std::string folder_name{translate("FileDialog", "Folder &name:")};
    std::string folder_not_made{translate("FileDialog", "Could not make the folder {0}: {1}")};
};

struct FileDialogOptions
{
    FileDialogMode mode{FileDialogMode::open_file};
    // Empty: the title the texts give for the mode.
    std::string title;
    // Empty: the current directory.
    std::filesystem::path directory;
    // The name the save dialog starts with.
    std::string file_name;
    // With no file_name, what the name it offers for a new file ends in, as ".oui": the texts' new_file_name with
    // this in place of its own ending. Empty: new_file_name as it is.
    std::string new_file_extension;
    // Empty: a single "All files" filter. Folder dialogs list folders only and ignore filters.
    std::vector<FileFilter> filters;
    std::size_t selected_filter{};
    // Asks before a save replaces an existing file.
    bool confirm_overwrite{true};
    // Remembers where the user left the dialog, through Application::set_window_placement_store.
    std::string placement_key;
    Size preferred_size{72, 22};
    Size minimum_size{50, 16};
    FileDialogTexts texts;
};

// Shows a modal file or folder dialog. Returns the chosen path, or nothing when the user cancelled.
[[nodiscard]] std::optional<std::filesystem::path> file_dialog(Application& application,
                                                               const FileDialogOptions& options);

} // namespace olagarro::oui
