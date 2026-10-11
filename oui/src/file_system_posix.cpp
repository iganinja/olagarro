#include <oui/file_system.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
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

std::vector<std::string> program_arguments(const int count, const char* const* const arguments)
{
    return count > 1 ? std::vector<std::string>{arguments + 1, arguments + count} : std::vector<std::string>{};
}

} // namespace olagarro::oui
