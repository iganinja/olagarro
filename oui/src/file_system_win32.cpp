#include <oui/file_system.h>

#include <algorithm>
#include <system_error>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <knownfolders.h>
#include <shellapi.h>
#include <shlobj.h>

#include "atomic_file.h"

namespace olagarro::oui
{

std::vector<std::filesystem::path> root_directories()
{
    std::vector<std::filesystem::path> result;
    const auto drives{GetLogicalDrives()};
    for(auto index{0u}; index < 26u; ++ index)
    {
        if((drives & (1u << index)) != 0)
        {
            result.emplace_back(std::wstring{static_cast<wchar_t>(L'A' + index), L':', L'\\'});
        }
    }
    return result;
}

std::vector<DirectoryEntry> list_directory(const std::filesystem::path& folder, std::error_code& error)
{
    error.clear();
    std::vector<DirectoryEntry> entries;
    if(folder.empty())
    {
        error = std::make_error_code(std::errc::no_such_file_or_directory);
        return entries;
    }
    auto pattern{folder.native()};
    if(pattern.back() != L'\\' && pattern.back() != L'/')
    {
        pattern += L'\\';
    }
    pattern += L'*';
    WIN32_FIND_DATAW found{};
    const auto search{FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &found, FindExSearchNameMatch, nullptr,
                                       FIND_FIRST_EX_LARGE_FETCH)};
    if(search == INVALID_HANDLE_VALUE)
    {
        error = std::error_code{static_cast<int>(GetLastError()), std::system_category()};
        return entries;
    }
    do
    {
        const std::wstring_view name{found.cFileName};
        if(name != L"." && name != L"..")
        {
            entries.push_back({folder / std::filesystem::path{std::wstring{name}},
                               (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0,
                               (found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0});
        }
    } while(FindNextFileW(search, &found) != 0);
    if(const auto code{GetLastError()}; code != ERROR_NO_MORE_FILES)
    {
        error = std::error_code{static_cast<int>(code), std::system_category()};
    }
    FindClose(search);
    return entries;
}

std::vector<DirectoryEntry> directory_entries(const std::filesystem::path& path)
{
    std::error_code ignored;
    auto result{list_directory(path, ignored)};
    std::sort(result.begin(), result.end(),
              [](const DirectoryEntry& first, const DirectoryEntry& second)
              {
                  if(first.is_directory != second.is_directory)
                  {
                      return first.is_directory;
                  }
                  return utf8_of(first.path.filename()) < utf8_of(second.path.filename());
              });
    return result;
}

namespace
{

[[nodiscard]] std::string last_error()
{
    return std::error_code{static_cast<int>(GetLastError()), std::system_category()}.message();
}

} // namespace

bool replace_file_contents(const std::filesystem::path& path, std::string_view bytes, std::string& error)
{
    const auto folder{path.has_parent_path() ? path.parent_path() : std::filesystem::path{L"."}};
    wchar_t temporary[MAX_PATH]{};
    if(GetTempFileNameW(folder.c_str(), L"oui", 0, temporary) == 0)
    {
        error = last_error();
        return false;
    }
    const auto file{CreateFileW(temporary, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if(file == INVALID_HANDLE_VALUE)
    {
        error = last_error();
        DeleteFileW(temporary);
        return false;
    }
    auto written{true};
    for(std::string_view rest{bytes}; written && !rest.empty();)
    {
        DWORD chunk{};
        const auto request{static_cast<DWORD>(std::min<std::size_t>(rest.size(), 1u << 30))};
        written = WriteFile(file, rest.data(), request, &chunk, nullptr) != 0;
        rest.remove_prefix(chunk);
    }
    written = written && FlushFileBuffers(file) != 0;
    const auto write_error{last_error()};
    CloseHandle(file);
    if(!written)
    {
        error = write_error;
        DeleteFileW(temporary);
        return false;
    }
    // ReplaceFileW keeps the original's attributes and permissions; a new file just moves into place.
    const auto replaced{
        GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES
            ? ReplaceFileW(path.c_str(), temporary, nullptr, REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr) != 0
            : MoveFileExW(temporary, path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0};
    if(!replaced)
    {
        error = last_error();
        DeleteFileW(temporary);
        return false;
    }
    return true;
}

std::filesystem::path known_folder(const std::string& folder)
{
    // The Known Folder API follows folders the user moved or that Windows names in their language.
    const auto* const id{folder == "home"        ? &FOLDERID_Profile
                         : folder == "desktop"   ? &FOLDERID_Desktop
                         : folder == "documents" ? &FOLDERID_Documents
                         : folder == "downloads" ? &FOLDERID_Downloads
                                                 : nullptr};
    if(id == nullptr)
    {
        return {};
    }
    PWSTR path{};
    std::filesystem::path result;
    if(SUCCEEDED(SHGetKnownFolderPath(*id, KF_FLAG_DEFAULT, nullptr, &path)))
    {
        result = path;
    }
    CoTaskMemFree(path);
    std::error_code ignored;
    return std::filesystem::is_directory(result, ignored) ? result : std::filesystem::path{};
}

void copy_entry(const std::filesystem::path& from, const std::filesystem::path& to, std::error_code& error)
{
    error.clear();
    const auto attributes{GetFileAttributesW(from.c_str())};
    if(attributes == INVALID_FILE_ATTRIBUTES)
    {
        error = std::error_code{static_cast<int>(GetLastError()), std::system_category()};
        return;
    }
    if((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
    {
        if(CopyFileW(from.c_str(), to.c_str(), FALSE) == 0)
        {
            error = std::error_code{static_cast<int>(GetLastError()), std::system_category()};
        }
        return;
    }
    if(CreateDirectoryW(to.c_str(), nullptr) == 0 && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        error = std::error_code{static_cast<int>(GetLastError()), std::system_category()};
        return;
    }
    for(const auto& entry : list_directory(from, error))
    {
        copy_entry(entry.path, to / entry.path.filename(), error);
        if(error)
        {
            return;
        }
    }
}

void remove_entry(const std::filesystem::path& path, std::error_code& error)
{
    error.clear();
    const auto attributes{GetFileAttributesW(path.c_str())};
    if(attributes == INVALID_FILE_ATTRIBUTES)
    {
        if(const auto code{GetLastError()}; code != ERROR_FILE_NOT_FOUND && code != ERROR_PATH_NOT_FOUND)
        {
            error = std::error_code{static_cast<int>(code), std::system_category()};
        }
        return;
    }
    const auto folder{(attributes & FILE_ATTRIBUTE_DIRECTORY) != 0};
    if(folder && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
    {
        for(const auto& entry : list_directory(path, error))
        {
            remove_entry(entry.path, error);
            if(error)
            {
                return;
            }
        }
        if(error)
        {
            return;
        }
    }
    if((folder ? RemoveDirectoryW(path.c_str()) : DeleteFileW(path.c_str())) == 0)
    {
        error = std::error_code{static_cast<int>(GetLastError()), std::system_category()};
    }
}

std::vector<std::string> program_arguments(int, const char* const*)
{
    std::vector<std::string> texts;
    int count{};
    auto* const arguments{CommandLineToArgvW(GetCommandLineW(), &count)};
    if(arguments == nullptr)
    {
        return texts;
    }
    for(int index{1}; index < count; ++ index)
    {
        texts.push_back(utf8_of(std::filesystem::path{std::wstring{arguments[index]}}));
    }
    LocalFree(arguments);
    return texts;
}

} // namespace olagarro::oui
