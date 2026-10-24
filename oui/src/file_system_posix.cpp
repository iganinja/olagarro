#include <oui/file_system.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "atomic_file.h"

namespace olagarro::oui
{

std::vector<std::filesystem::path> root_directories()
{
    return {std::filesystem::path{"/"}};
}

std::vector<DirectoryEntry> list_directory(const std::filesystem::path& folder, std::error_code& error)
{
    std::vector<DirectoryEntry> entries;
    std::filesystem::directory_iterator entry{folder, std::filesystem::directory_options::skip_permission_denied,
                                              error};
    for(const std::filesystem::directory_iterator end; !error && entry != end; entry.increment(error))
    {
        std::error_code ignored;
        entries.push_back({entry->path(), entry->is_directory(ignored), entry->is_symlink(ignored)});
    }
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
                  return first.path.filename().native() < second.path.filename().native();
              });
    return result;
}

namespace
{

[[nodiscard]] std::string last_error()
{
    return std::error_code{errno, std::generic_category()}.message();
}

[[nodiscard]] bool write_all(int descriptor, std::string_view bytes)
{
    while(!bytes.empty())
    {
        const auto written{write(descriptor, bytes.data(), bytes.size())};
        if(written < 0)
        {
            if(errno == EINTR)
            {
                continue;
            }
            return false;
        }
        bytes.remove_prefix(static_cast<std::size_t>(written));
    }
    return true;
}

} // namespace

bool replace_file_contents(const std::filesystem::path& path, std::string_view bytes, std::string& error)
{
    // Write through a symbolic link to its target; renaming over the link would replace it with a file.
    // A path that does not exist yet is simply a new file.
    std::error_code link_error;
    const auto is_link{std::filesystem::symlink_status(path, link_error).type() == std::filesystem::file_type::symlink};
    const auto target{is_link ? std::filesystem::canonical(path, link_error) : path};
    if(is_link && link_error)
    {
        error = link_error.message();
        return false;
    }
    const auto folder{target.has_parent_path() ? target.parent_path() : std::filesystem::path{"."}};
    auto temporary{(folder / ("." + target.filename().string() + ".oui-save-XXXXXX")).string()};
    const auto descriptor{mkstemp(temporary.data())};
    if(descriptor < 0)
    {
        error = last_error();
        return false;
    }
    // mkstemp creates the file as 0600: give it the mode of the file it replaces, or the usual one for a
    // new file.
    struct stat original
    {
    };
    mode_t mode{};
    if(stat(target.c_str(), &original) == 0)
    {
        mode = original.st_mode & 07777;
    }
    else
    {
        const auto mask{umask(0)};
        umask(mask);
        mode = 0666 & ~mask;
    }
    const auto written{write_all(descriptor, bytes) && fchmod(descriptor, mode) == 0 && fsync(descriptor) == 0};
    const auto write_error{last_error()};
    const auto closed{close(descriptor) == 0};
    if(!written || !closed || rename(temporary.c_str(), target.c_str()) != 0)
    {
        error = written && closed ? last_error() : write_error;
        unlink(temporary.c_str());
        return false;
    }
    // Make the rename itself durable.
    const auto folder_descriptor{open(folder.c_str(), O_RDONLY)};
    if(folder_descriptor >= 0)
    {
        fsync(folder_descriptor);
        close(folder_descriptor);
    }
    return true;
}

namespace
{

std::filesystem::path env_var(const char* name)
{
    const auto value{std::getenv(name)};
    if(value && value[0] != '\0')
    {
        return value;
    }
    return {};
}

} // namespace

namespace
{

// The folder a user-dirs.dirs line such as XDG_DESKTOP_DIR="$HOME/Escritorio" gives for `key`, from the
// file xdg-user-dirs keeps; desktops name these folders in the user's language.
std::filesystem::path user_dirs_entry(const std::string& key, const std::filesystem::path& home)
{
    auto config{env_var("XDG_CONFIG_HOME")};
    if(config.empty())
    {
        config = home / ".config";
    }
    const auto text{read_file(config / "user-dirs.dirs").value_or(std::string{})};
    for(std::size_t start{}; start < text.size();)
    {
        const auto end{std::min(text.find('\n', start), text.size())};
        const auto line{text.substr(start, end - start)};
        start = end + 1;
        if(line.compare(0, key.size() + 1, key + "=") != 0)
        {
            continue;
        }
        auto value{line.substr(key.size() + 1)};
        if(value.size() >= 2 && value.front() == '"' && value.back() == '"')
        {
            value = value.substr(1, value.size() - 2);
        }
        constexpr std::string_view home_prefix{"$HOME"};
        if(value.compare(0, home_prefix.size(), home_prefix) == 0)
        {
            return home.string() + value.substr(home_prefix.size());
        }
        return value;
    }
    return {};
}

} // namespace

std::filesystem::path known_folder(const std::string& folder)
{
    const auto home{env_var("HOME")};
    if(folder == "home")
    {
        return home;
    }
    struct Known
    {
        const char* name;
        const char* key;
        const char* english;
    };
    constexpr Known known[]{{"desktop", "XDG_DESKTOP_DIR", "Desktop"},
                            {"documents", "XDG_DOCUMENTS_DIR", "Documents"},
                            {"downloads", "XDG_DOWNLOAD_DIR", "Downloads"}};
    for(const auto& entry : known)
    {
        if(folder != entry.name)
        {
            continue;
        }
        // The environment first, then the user's xdg-user-dirs settings, then the English default.
        std::error_code ignored;
        for(const auto& candidate :
            {env_var(entry.key), home.empty() ? std::filesystem::path{} : user_dirs_entry(entry.key, home),
             home.empty() ? std::filesystem::path{} : home / entry.english})
        {
            if(!candidate.empty() && std::filesystem::is_directory(candidate, ignored))
            {
                return candidate;
            }
        }
        return {};
    }
    return {};
}

void copy_entry(const std::filesystem::path& from, const std::filesystem::path& to, std::error_code& error)
{
    error.clear();
    const auto status{std::filesystem::status(from, error)};
    if(error)
    {
        return;
    }
    if(std::filesystem::is_directory(status))
    {
        std::filesystem::create_directory(to, error);
        if(error)
        {
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
        return;
    }
    // Opening the target would empty the very file being read.
    if(std::error_code ignored; std::filesystem::equivalent(from, to, ignored))
    {
        error = std::make_error_code(std::errc::file_exists);
        return;
    }
    // By hand: std::filesystem::copy falls back on a file stream, which brings in the C++ library's locales.
    auto* const source{open_file(from, "rb")};
    if(source == nullptr)
    {
        error = std::error_code{errno, std::generic_category()};
        return;
    }
    auto* const target{open_file(to, "wb")};
    if(target == nullptr)
    {
        error = std::error_code{errno, std::generic_category()};
        std::fclose(source);
        return;
    }
    char buffer[65536];
    for(std::size_t read{}; !error && (read = std::fread(buffer, 1, sizeof buffer, source)) > 0;)
    {
        if(std::fwrite(buffer, 1, read, target) != read)
        {
            error = std::error_code{errno, std::generic_category()};
        }
    }
    if(!error && std::ferror(source) != 0)
    {
        error = std::make_error_code(std::errc::io_error);
    }
    std::fclose(source);
    if(std::fclose(target) != 0 && !error)
    {
        error = std::error_code{errno, std::generic_category()};
    }
    if(!error)
    {
        std::filesystem::permissions(to, status.permissions(), error);
    }
}

void remove_entry(const std::filesystem::path& path, std::error_code& error)
{
    std::filesystem::remove_all(path, error);
}

namespace
{

[[nodiscard]] std::error_code errno_error()
{
    return std::error_code{errno, std::generic_category()};
}

// `name` for the `count`th thing of that name in a trash: "notes.txt", then "notes.2.txt".
[[nodiscard]] std::string numbered(const std::filesystem::path& name, int count)
{
    if(count == 1)
    {
        return name.native();
    }
    return name.stem().native() + "." + std::to_string(count) + name.extension().native();
}

// A folder of the person's own, made if missing: not a link, as someone else could have put one there.
[[nodiscard]] bool own_folder(const std::filesystem::path& folder)
{
    if(mkdir(folder.c_str(), 0700) != 0 && errno != EEXIST)
    {
        return false;
    }
    struct stat status
    {
    };
    return lstat(folder.c_str(), &status) == 0 && S_ISDIR(status.st_mode) && status.st_uid == getuid();
}

#ifndef __APPLE__

// The freedesktop.org trash specification's: a trash folder, and where the paths written in it start from, the top
// of its drive, or nothing for the home's, whose paths are whole.
struct Trash
{
    std::filesystem::path folder;
    std::filesystem::path top;
};

[[nodiscard]] std::optional<Trash> home_trash()
{
    auto data{env_var("XDG_DATA_HOME")};
    if(data.empty())
    {
        const auto home{env_var("HOME")};
        if(home.empty())
        {
            return std::nullopt;
        }
        data = home / ".local" / "share";
    }
    std::error_code error;
    std::filesystem::create_directories(data, error);
    Trash trash{data / "Trash", {}};
    if(!own_folder(trash.folder) || !own_folder(trash.folder / "files") || !own_folder(trash.folder / "info"))
    {
        return std::nullopt;
    }
    return trash;
}

// The top of the drive `path` is on: the last folder up from it on the same device.
[[nodiscard]] std::filesystem::path top_of_drive(const std::filesystem::path& path)
{
    struct stat status
    {
    };
    if(lstat(path.c_str(), &status) != 0)
    {
        return {};
    }
    auto top{path.parent_path()};
    for(struct stat parent{};
        top.has_relative_path() && stat(top.parent_path().c_str(), &parent) == 0 && parent.st_dev == status.st_dev;)
    {
        top = top.parent_path();
    }
    return top;
}

// The trash of the drive at `top`: its shared .Trash, sticky and no link, with a folder of the person's own in it, or
// else a .Trash-uid of their own.
[[nodiscard]] std::optional<Trash> drive_trash(const std::filesystem::path& top)
{
    const auto user{std::to_string(getuid())};
    struct stat shared
    {
    };
    std::optional<std::filesystem::path> folder;
    if(lstat((top / ".Trash").c_str(), &shared) == 0 && S_ISDIR(shared.st_mode) && (shared.st_mode & S_ISVTX) != 0 &&
       own_folder(top / ".Trash" / user))
    {
        folder = top / ".Trash" / user;
    }
    else if(own_folder(top / (".Trash-" + user)))
    {
        folder = top / (".Trash-" + user);
    }
    if(!folder || !own_folder(*folder / "files") || !own_folder(*folder / "info"))
    {
        return std::nullopt;
    }
    return Trash{*folder, top};
}

// A path as a .trashinfo has it: each byte but letters, figures, "-._~" and '/' as %XX.
[[nodiscard]] std::string escaped(const std::string& path)
{
    constexpr char figures[]{"0123456789ABCDEF"};
    std::string text;
    for(const auto character : path)
    {
        const auto byte{static_cast<unsigned char>(character)};
        if((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') || (byte >= '0' && byte <= '9') ||
           byte == '-' || byte == '.' || byte == '_' || byte == '~' || byte == '/')
        {
            text += character;
        }
        else
        {
            text += '%';
            text += figures[byte >> 4];
            text += figures[byte & 15];
        }
    }
    return text;
}

[[nodiscard]] std::string deletion_date()
{
    const auto now{std::time(nullptr)};
    std::tm local{};
    localtime_r(&now, &local);
    char text[32];
    std::strftime(text, sizeof text, "%Y-%m-%dT%H:%M:%S", &local);
    return text;
}

// Puts `path` in `trash` under a name not taken there, its .trashinfo made first so no other program takes the name.
// With `copy`, it goes by copying and removing, from another drive; otherwise by renaming, and EXDEV says it cannot.
void put_in(const Trash& trash, const std::filesystem::path& path, bool copy, std::error_code& error)
{
    const auto original{trash.top.empty() ? utf8_of(path) : utf8_of(relative_to(path, trash.top))};
    const auto info_text{"[Trash Info]\nPath=" + escaped(original) + "\nDeletionDate=" + deletion_date() + "\n"};
    for(int count{1};; ++ count)
    {
        const auto name{numbered(path.filename(), count)};
        const auto info{trash.folder / "info" / (name + ".trashinfo")};
        const auto descriptor{open(info.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600)};
        if(descriptor < 0)
        {
            if(errno == EEXIST)
            {
                continue;
            }
            error = errno_error();
            return;
        }
        const auto written{write_all(descriptor, info_text)};
        close(descriptor);
        const auto target{trash.folder / "files" / name};
        // A file left there without its .trashinfo keeps its name.
        struct stat there
        {
        };
        if(lstat(target.c_str(), &there) == 0)
        {
            unlink(info.c_str());
            continue;
        }
        if(!written)
        {
            error = errno_error();
            unlink(info.c_str());
            return;
        }
        if(copy)
        {
            copy_entry(path, target, error);
            if(!error)
            {
                std::filesystem::remove_all(path, error);
            }
            if(error)
            {
                std::error_code ignored;
                std::filesystem::remove_all(target, ignored);
                unlink(info.c_str());
            }
            return;
        }
        if(rename(path.c_str(), target.c_str()) != 0)
        {
            error = errno_error();
            unlink(info.c_str());
            return;
        }
        error.clear();
        return;
    }
}

#endif

} // namespace

void move_to_trash(const std::filesystem::path& path, std::error_code& error)
{
    error.clear();
    const auto whole{whole_path(path)};
    struct stat status
    {
    };
    if(lstat(whole.c_str(), &status) != 0)
    {
        error = errno_error();
        return;
    }
#ifdef __APPLE__
    // The Finder's Trash, where it can be put back by hand; the system's own call, which knows where it came from,
    // is Objective-C's.
    const auto home{env_var("HOME")};
    if(home.empty() || !own_folder(home / ".Trash"))
    {
        error = std::make_error_code(std::errc::no_such_file_or_directory);
        return;
    }
    for(int count{1};; ++ count)
    {
        const auto target{home / ".Trash" / numbered(whole.filename(), count)};
        struct stat there
        {
        };
        if(lstat(target.c_str(), &there) == 0)
        {
            continue;
        }
        if(rename(whole.c_str(), target.c_str()) != 0)
        {
            error = errno_error();
        }
        return;
    }
#else
    const auto home{home_trash()};
    if(home)
    {
        put_in(*home, whole, false, error);
        if(error != std::errc::cross_device_link)
        {
            return;
        }
    }
    // On another drive: that drive's trash, or the home's by copying when the drive has none.
    if(const auto drive{drive_trash(top_of_drive(whole))})
    {
        put_in(*drive, whole, false, error);
        return;
    }
    if(home)
    {
        put_in(*home, whole, true, error);
        return;
    }
    error = std::make_error_code(std::errc::no_such_file_or_directory);
#endif
}

std::vector<std::string> program_arguments(const int count, const char* const* const arguments)
{
    return count > 1 ? std::vector<std::string>{arguments + 1, arguments + count} : std::vector<std::string>{};
}

} // namespace olagarro::oui
