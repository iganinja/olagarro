#pragma once

#include <oui/application.h>
#include <oui/text_search.h>

#include <cstddef>
#include <filesystem>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <atomic>
#include <string>
#include <string_view>
#include <vector>

// Go to File and Find in Files: the files of a project, and looking through them.

// The files of `folder` and its subfolders, relative to it, for Go to File and Find in Files. In a git
// repository, what git ignores is left out; elsewhere, hidden files and folders, and the folders builds and
// package managers fill. The walk stops at a size a list can still filter quickly.
[[nodiscard]] std::vector<std::filesystem::path> project_files(const std::filesystem::path& folder);

// What Find in Files looks for, and in which files: patterns such as "*.cpp *.h", matched as Go to File
// matches them; none means every file.
struct FindInFilesQuery
{
    std::string text;
    std::string files;
    olagarro::oui::SearchOptions options;
};

// Asks what to look for, starting from `last`; nothing when cancelled.
[[nodiscard]] std::optional<FindInFilesQuery> show_find_in_files_dialog(olagarro::oui::Application& application,
                                                                        FindInFilesQuery last);
// Whether a file, relative to the project, is one of those `patterns` asks for: a pattern with a '/' is
// matched against the whole path, any other against the file name, as Go to File does.
[[nodiscard]] bool matches_file_patterns(const std::vector<std::string>& patterns, const std::filesystem::path& file);
// The patterns of a Find in Files query, which spaces, commas or semicolons separate.
[[nodiscard]] std::vector<std::string> file_patterns(std::string_view text);

// A match of Find in Files: where it is, as the editor counts lines and bytes, and its line's text.
struct FileMatch
{
    std::filesystem::path file;
    std::size_t line{};
    std::size_t column{};
    std::size_t length{};
    std::string text;
};

// Looks through a project's files on other threads, so the editor goes on answering, and hands what it
// found to the application's thread. A new search stops the one running, and so does the end of OUI Code.
class ProjectSearch
{
  public:
    using Done = std::function<void(std::vector<FileMatch> matches)>;
    // What to search: the project's folder, its files relative to it, and the text of the files open with
    // changes, by the same relative path. Those are searched as the editor has them, not as they are on disk.
    struct Request
    {
        std::filesystem::path folder;
        std::vector<std::filesystem::path> files;
        std::map<std::filesystem::path, std::string> unsaved;
    };

    explicit ProjectSearch(olagarro::oui::Application& application);
    ~ProjectSearch();
    ProjectSearch(const ProjectSearch&) = delete;
    ProjectSearch& operator=(const ProjectSearch&) = delete;

    void start(Request request, olagarro::oui::TextSearch search, Done done);
    void stop();

  private:
    olagarro::oui::Application& m_application;
    std::shared_ptr<std::atomic<bool>> m_stopped;
    std::future<void> m_work;
};
