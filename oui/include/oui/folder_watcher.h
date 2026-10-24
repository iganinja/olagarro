#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace olagarro::oui
{

// Something that changed under a watched folder. Paths are from the watched folder, in UTF-8 with '/' between
// folders on every system.
struct FolderChange
{
    enum class Kind
    {
        added,
        removed,
        // A file written to; a folder does not change, what is in it does.
        changed,
        renamed
    };
    Kind kind;
    std::string path;
    // Where it was, for a rename; otherwise empty.
    std::string old_path{};
    bool is_folder{false};

    bool operator==(const FolderChange& other) const
    {
        return kind == other.kind && path == other.path && old_path == other.old_path && is_folder == other.is_folder;
    }
    bool operator!=(const FolderChange& other) const { return !(*this == other); }
};

// Hears what is added, removed, changed and renamed in a folder and every folder under it. It waits for nothing and
// runs no thread of its own: changes() says what happened since it was last asked, as a program asks on a timer.
// On Linux the system tells it (inotify), and a file counts as changed once whoever wrote it closed it; elsewhere, or
// when the system cannot tell it any more (too many folders, or too many changes at once), it compares what the
// folder holds each time it is asked, telling files apart by their size and time.
class FolderWatcher
{
  public:
    enum class Way
    {
        // The system's, where there is one, else polling.
        system,
        polling
    };

    explicit FolderWatcher(std::filesystem::path folder, Way way = Way::system);
    ~FolderWatcher();

    FolderWatcher(const FolderWatcher&) = delete;
    FolderWatcher& operator=(const FolderWatcher&) = delete;
    FolderWatcher(FolderWatcher&&) = delete;
    FolderWatcher& operator=(FolderWatcher&&) = delete;

    // What changed since it was made or last asked, each change once: a file added and written to is only added, one
    // added and removed again is nothing. A folder added comes before what it holds. Polling tells a rename only of a
    // file, one that kept its size and time; a folder renamed shows as removed and added, with what it holds.
    [[nodiscard]] std::vector<FolderChange> changes();
    // Whether it is comparing what the folder holds rather than hearing from the system.
    [[nodiscard]] bool is_polling() const;
    [[nodiscard]] const std::filesystem::path& folder() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace olagarro::oui
