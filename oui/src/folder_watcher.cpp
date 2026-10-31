#include <oui/folder_watcher.h>

#include <oui/file_system.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <system_error>
#include <utility>

#ifdef __linux__
#include <sys/inotify.h>
#include <unistd.h>

#include <cerrno>
#endif

namespace olagarro::oui
{

namespace
{

// What is known of each thing under the folder, by its path from it.
struct Entry
{
    bool is_folder{false};
    std::uintmax_t size{};
    std::filesystem::file_time_type time{};
};
using Snapshot = std::map<std::string, Entry>;

[[nodiscard]] std::optional<Entry> entry_of(const std::filesystem::path& path)
{
    std::error_code error;
    // A link is not followed: one to a folder above would never end.
    const auto status{std::filesystem::symlink_status(path, error)};
    if(error || !std::filesystem::exists(status))
    {
        return std::nullopt;
    }
    Entry entry{std::filesystem::is_directory(status)};
    if(!entry.is_folder)
    {
        entry.size = std::filesystem::file_size(path, error);
        if(error)
        {
            entry.size = 0;
        }
        entry.time = std::filesystem::last_write_time(path, error);
    }
    return entry;
}

[[nodiscard]] std::string joined(const std::string& folder, const std::string& name)
{
    return folder.empty() ? name : folder + "/" + name;
}

#ifdef __linux__
[[nodiscard]] bool is_inside(const std::string& path, const std::string& folder)
{
    return path.size() > folder.size() && path.compare(0, folder.size(), folder) == 0 && path[folder.size()] == '/';
}
#endif

// Everything under `relative`, a folder under `root`, into `snapshot`; `found`, when given, hears each thing in the
// order met, a folder before what it holds. `entered`, when given, is told each folder before it is listed.
template <typename Entered>
void scan(const std::filesystem::path& root, const std::string& relative, Snapshot& snapshot,
          std::vector<std::string>* found, Entered&& entered)
{
    if(!entered(relative))
    {
        return;
    }
    std::error_code error;
    auto entries{list_directory(relative.empty() ? root : root / path_from_utf8(relative), error)};
    std::sort(entries.begin(), entries.end(),
              [](const DirectoryEntry& left, const DirectoryEntry& right) { return left.path < right.path; });
    for(const auto& listed : entries)
    {
        const auto path{joined(relative, utf8_of(listed.path.filename()))};
        const auto entry{entry_of(listed.path)};
        if(!entry)
        {
            continue;
        }
        const auto added{snapshot.emplace(path, *entry).second};
        if(added && found != nullptr)
        {
            found->push_back(path);
        }
        if(entry->is_folder)
        {
            scan(root, path, snapshot, found, entered);
        }
    }
}

[[nodiscard]] Snapshot snapshot_of(const std::filesystem::path& root)
{
    Snapshot snapshot;
    scan(root, {}, snapshot, nullptr, [](const std::string&) { return true; });
    return snapshot;
}

// What tells `before` from `now`: what went, deepest first, then what was renamed, then what came and what was
// written to, by path, so a folder comes before what it holds.
[[nodiscard]] std::vector<FolderChange> difference(const Snapshot& before, const Snapshot& now)
{
    std::vector<std::string> removed;
    std::vector<std::string> added;
    std::vector<std::string> changed;
    for(const auto& [path, entry] : before)
    {
        const auto found{now.find(path)};
        if(found == now.end() || found->second.is_folder != entry.is_folder)
        {
            removed.push_back(path);
        }
    }
    for(const auto& [path, entry] : now)
    {
        const auto found{before.find(path)};
        if(found == before.end() || found->second.is_folder != entry.is_folder)
        {
            added.push_back(path);
        }
        else if(!entry.is_folder && (found->second.size != entry.size || found->second.time != entry.time))
        {
            changed.push_back(path);
        }
    }
    // A file gone from one place and come to another with the same size and time was renamed, when no other could be.
    const auto same{[](const Entry& left, const Entry& right) {
        return !left.is_folder && !right.is_folder && left.size == right.size && left.time == right.time;
    }};
    std::vector<std::pair<std::string, std::string>> renamed;
    for(auto from{removed.begin()}; from != removed.end();)
    {
        const auto& old_entry{before.at(*from)};
        const auto matches{[&](const std::string& path) { return same(now.at(path), old_entry); }};
        const auto to{std::find_if(added.begin(), added.end(), matches)};
        const auto only{to != added.end() && std::find_if(std::next(to), added.end(), matches) == added.end() &&
                        std::count_if(removed.begin(), removed.end(), [&](const std::string& path)
                                      { return same(before.at(path), now.at(*to)); }) == 1};
        if(!only)
        {
            ++ from;
            continue;
        }
        renamed.emplace_back(*from, *to);
        added.erase(to);
        from = removed.erase(from);
    }
    std::vector<FolderChange> changes;
    for(auto path{removed.rbegin()}; path != removed.rend(); ++ path)
    {
        changes.push_back({FolderChange::Kind::removed, *path, {}, before.at(*path).is_folder});
    }
    for(auto& [from, to] : renamed)
    {
        changes.push_back({FolderChange::Kind::renamed, to, from, false});
    }
    std::vector<FolderChange> later;
    for(const auto& path : added)
    {
        later.push_back({FolderChange::Kind::added, path, {}, now.at(path).is_folder});
    }
    for(const auto& path : changed)
    {
        later.push_back({FolderChange::Kind::changed, path, {}, false});
    }
    std::stable_sort(later.begin(), later.end(),
                     [](const FolderChange& left, const FolderChange& right) { return left.path < right.path; });
    changes.insert(changes.end(), later.begin(), later.end());
    return changes;
}

#ifdef __linux__
// The last of `changes` about `path`, or none.
[[nodiscard]] std::vector<FolderChange>::iterator last_about(std::vector<FolderChange>& changes,
                                                             const std::string& path)
{
    for(auto change{changes.end()}; change != changes.begin();)
    {
        -- change;
        if(change->path == path)
        {
            return change;
        }
    }
    return changes.end();
}

// Adds `change` to what will be told, each thing once: written to after it came, it only came; gone after it came, it
// was never there; renamed after it came, it came where it went; come again after it went, a file was written to.
void note(std::vector<FolderChange>& changes, FolderChange change)
{
    using Kind = FolderChange::Kind;
    const auto last{last_about(changes, change.kind == Kind::renamed ? change.old_path : change.path)};
    const auto last_kind{last == changes.end() ? std::optional<Kind>{} : last->kind};
    switch(change.kind)
    {
    case Kind::changed:
        if(last_kind == Kind::added || last_kind == Kind::changed)
        {
            return;
        }
        break;
    case Kind::removed:
        if(last_kind == Kind::added)
        {
            const auto path{change.path};
            changes.erase(std::remove_if(changes.begin(), changes.end(),
                                         [&](const FolderChange& each) { return each.path == path; }),
                          changes.end());
            return;
        }
        if(last_kind == Kind::changed)
        {
            changes.erase(last);
        }
        break;
    case Kind::renamed:
        if(last_kind == Kind::added)
        {
            last->path = change.path;
            return;
        }
        break;
    case Kind::added:
        if(last_kind == Kind::removed && !last->is_folder && !change.is_folder)
        {
            changes.erase(last);
            change.kind = Kind::changed;
        }
        break;
    }
    changes.push_back(std::move(change));
}
#endif

} // namespace

struct FolderWatcher::Impl
{
    std::filesystem::path folder;
    Snapshot known;
    bool polling{true};
#ifdef __linux__
    int descriptor{-1};
    // Each folder watched, by the system's number for its watch.
    std::map<int, std::string> watches;

    ~Impl() { stop_watching(); }

    void stop_watching()
    {
        if(descriptor >= 0)
        {
            ::close(descriptor);
            descriptor = -1;
        }
        watches.clear();
    }

    [[nodiscard]] bool watch(const std::string& relative)
    {
        const auto path{relative.empty() ? folder : folder / path_from_utf8(relative)};
        constexpr std::uint32_t events{IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVED_FROM | IN_MOVED_TO |
                                       IN_ONLYDIR | IN_DONT_FOLLOW | IN_EXCL_UNLINK};
        const auto number{inotify_add_watch(descriptor, path.c_str(), events)};
        if(number < 0)
        {
            // Gone already is no failure; the system out of watches is.
            return errno == ENOENT || errno == ENOTDIR;
        }
        watches[number] = relative;
        return true;
    }

    // Watches `relative` and everything under it, adding to what is known what was not, and gives the paths it
    // added; false in `ok` when the system would not watch one.
    [[nodiscard]] std::vector<std::string> watch_tree(const std::string& relative, bool& ok)
    {
        std::vector<std::string> found;
        scan(folder, relative, known, &found,
             [&](const std::string& each)
             {
                 ok = ok && watch(each);
                 return ok;
             });
        return found;
    }

    void forget(const std::string& path)
    {
        known.erase(path);
        for(auto inside{known.upper_bound(path)}; inside != known.end() && is_inside(inside->first, path);)
        {
            inside = known.erase(inside);
        }
        for(auto watched{watches.begin()}; watched != watches.end();)
        {
            if(watched->second == path || is_inside(watched->second, path))
            {
                inotify_rm_watch(descriptor, watched->first);
                watched = watches.erase(watched);
            }
            else
            {
                ++ watched;
            }
        }
    }

    // What came at `path`, and all under it when it is a folder; false when it could not be watched.
    [[nodiscard]] bool came(const std::string& path, std::vector<FolderChange>& changes)
    {
        if(known.count(path) != 0)
        {
            return true;
        }
        const auto entry{entry_of(folder / path_from_utf8(path))};
        if(!entry)
        {
            return true;
        }
        known[path] = *entry;
        note(changes, {FolderChange::Kind::added, path, {}, entry->is_folder});
        if(!entry->is_folder)
        {
            return true;
        }
        auto ok{true};
        for(const auto& inside : watch_tree(path, ok))
        {
            note(changes, {FolderChange::Kind::added, inside, {}, known.at(inside).is_folder});
        }
        return ok;
    }

    void went(const std::string& path, std::vector<FolderChange>& changes)
    {
        const auto found{known.find(path)};
        if(found == known.end())
        {
            return;
        }
        const auto is_folder{found->second.is_folder};
        // What it held, when it was moved out whole rather than emptied first, deepest first.
        std::vector<FolderChange> inside;
        for(auto each{known.upper_bound(path)}; each != known.end() && is_inside(each->first, path); ++ each)
        {
            inside.push_back({FolderChange::Kind::removed, each->first, {}, each->second.is_folder});
        }
        for(auto each{inside.rbegin()}; each != inside.rend(); ++ each)
        {
            note(changes, std::move(*each));
        }
        forget(path);
        note(changes, {FolderChange::Kind::removed, path, {}, is_folder});
    }

    void moved(const std::string& from, const std::string& to, std::vector<FolderChange>& changes)
    {
        const auto found{known.find(from)};
        // Written under a name gone by the time it is heard, as a download or a program saving does, and renamed.
        if(found == known.end())
        {
            if(known.count(to) != 0)
            {
                written(to, changes);
            }
            else
            {
                static_cast<void>(came(to, changes));
            }
            return;
        }
        const auto entry{found->second};
        // A file put in another's place, as programs save: the one there was written to, the other went.
        if(const auto replaced{known.find(to)};
           replaced != known.end() && !replaced->second.is_folder && !entry.is_folder)
        {
            known.erase(from);
            replaced->second = entry;
            note(changes, {FolderChange::Kind::removed, from, {}, false});
            note(changes, {FolderChange::Kind::changed, to, {}, false});
            return;
        }
        // What it holds goes with it, and so do the watches on its folders.
        std::vector<std::pair<std::string, Entry>> inside;
        for(auto each{known.upper_bound(from)}; each != known.end() && is_inside(each->first, from);)
        {
            inside.emplace_back(to + each->first.substr(from.size()), each->second);
            each = known.erase(each);
        }
        known.erase(from);
        known[to] = entry;
        known.insert(inside.begin(), inside.end());
        for(auto& [number, watched] : watches)
        {
            if(watched == from || is_inside(watched, from))
            {
                watched = to + watched.substr(from.size());
            }
        }
        note(changes, {FolderChange::Kind::renamed, to, from, entry.is_folder});
    }

    void written(const std::string& path, std::vector<FolderChange>& changes)
    {
        const auto found{known.find(path)};
        const auto entry{entry_of(folder / path_from_utf8(path))};
        if(found == known.end() || !entry)
        {
            static_cast<void>(came(path, changes));
            return;
        }
        found->second = *entry;
        note(changes, {FolderChange::Kind::changed, path, {}, false});
    }

    // An event as the system told it; its path is found when it is dealt with, after the renames before it.
    struct Heard
    {
        std::uint32_t mask;
        std::uint32_t cookie;
        int watch;
        std::string name;
    };

    [[nodiscard]] std::optional<std::string> path_of(const Heard& event) const
    {
        const auto watched{watches.find(event.watch)};
        if(watched == watches.end())
        {
            return std::nullopt;
        }
        return joined(watched->second, event.name);
    }

    // What the system has to say, none when it says nothing; false when it can no longer tell.
    [[nodiscard]] bool read_all(std::vector<Heard>& heard)
    {
        alignas(inotify_event) char buffer[64 * 1024];
        for(;;)
        {
            const auto length{::read(descriptor, buffer, sizeof buffer)};
            if(length < 0)
            {
                return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
            }
            if(length == 0)
            {
                return true;
            }
            for(auto offset{std::size_t{}}; offset < static_cast<std::size_t>(length);)
            {
                const auto* const event{reinterpret_cast<const inotify_event*>(buffer + offset)};
                offset += sizeof(inotify_event) + event->len;
                if((event->mask & IN_Q_OVERFLOW) != 0)
                {
                    return false;
                }
                // What happens to a watched folder itself is heard from the one holding it.
                if(event->len == 0)
                {
                    continue;
                }
                heard.push_back({event->mask, event->cookie, event->wd, event->name});
            }
        }
    }

    [[nodiscard]] std::vector<FolderChange> heard_from_system()
    {
        std::vector<Heard> heard;
        auto ok{read_all(heard)};
        std::vector<FolderChange> changes;
        for(std::size_t index{}; ok && index < heard.size(); ++ index)
        {
            const auto& event{heard[index]};
            const auto path{path_of(event)};
            if(!path)
            {
                continue;
            }
            if((event.mask & IN_MOVED_FROM) != 0)
            {
                // Its other half, if it went somewhere under the folder.
                const auto to{std::find_if(heard.begin() + static_cast<std::ptrdiff_t>(index) + 1, heard.end(),
                                           [&](const Heard& other) {
                                               return (other.mask & IN_MOVED_TO) != 0 && other.cookie == event.cookie;
                                           })};
                const auto to_path{to == heard.end() ? std::nullopt : path_of(*to)};
                if(to_path)
                {
                    moved(*path, *to_path, changes);
                    to->mask = 0;
                }
                else
                {
                    went(*path, changes);
                }
            }
            else if((event.mask & (IN_CREATE | IN_MOVED_TO)) != 0)
            {
                ok = came(*path, changes);
            }
            else if((event.mask & IN_DELETE) != 0)
            {
                went(*path, changes);
            }
            else if((event.mask & IN_CLOSE_WRITE) != 0)
            {
                written(*path, changes);
            }
        }
        if(!ok)
        {
            // The system cannot tell any more: what it did not tell is found by comparing, and from now on so is all.
            stop_watching();
            polling = true;
            auto now{snapshot_of(folder)};
            for(auto& change : difference(known, now))
            {
                note(changes, std::move(change));
            }
            known = std::move(now);
        }
        return changes;
    }

    void start(Way way)
    {
        if(way == Way::polling)
        {
            return;
        }
        descriptor = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if(descriptor < 0)
        {
            return;
        }
        auto ok{true};
        static_cast<void>(watch_tree({}, ok));
        if(!ok)
        {
            stop_watching();
            known.clear();
            return;
        }
        polling = false;
    }
#else
    void start(Way) {}
#endif
};

FolderWatcher::FolderWatcher(std::filesystem::path folder, Way way) : m_impl{std::make_unique<Impl>()}
{
    m_impl->folder = std::move(folder);
    m_impl->start(way);
    if(m_impl->polling)
    {
        m_impl->known = snapshot_of(m_impl->folder);
    }
}

FolderWatcher::~FolderWatcher() = default;

std::vector<FolderChange> FolderWatcher::changes()
{
#ifdef __linux__
    if(!m_impl->polling)
    {
        return m_impl->heard_from_system();
    }
#endif
    auto now{snapshot_of(m_impl->folder)};
    auto changes{difference(m_impl->known, now)};
    m_impl->known = std::move(now);
    return changes;
}

bool FolderWatcher::is_polling() const
{
    return m_impl->polling;
}

const std::filesystem::path& FolderWatcher::folder() const
{
    return m_impl->folder;
}

} // namespace olagarro::oui
