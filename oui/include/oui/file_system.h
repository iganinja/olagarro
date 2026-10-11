#pragma once

#include <oui/utf8.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace olagarro::oui
{

struct DirectoryEntry
{
    std::filesystem::path path;
    // A link to a folder is a folder too, and a link besides.
    bool is_directory{false};
    bool is_link{false};
};

[[nodiscard]] std::vector<std::filesystem::path> root_directories();
// What `folder` holds, in no order; what it could list before `error`, when listing fails.
// OUI lists folders only through these: on Windows, std::filesystem's own iterators bring in the C++ library's
// locales, a few hundred kilobytes of every program.
[[nodiscard]] std::vector<DirectoryEntry> list_directory(const std::filesystem::path& folder, std::error_code& error);
// As list_directory, folders first and then by name, and nothing when it fails.
[[nodiscard]] std::vector<DirectoryEntry> directory_entries(const std::filesystem::path& path);
// Copies a file over `to`, or a folder with everything in it into `to`, overwriting what is there.
void copy_entry(const std::filesystem::path& from, const std::filesystem::path& to, std::error_code& error);
// Removes a file, or a folder with everything in it; a link goes, not what it leads to. Nothing there is no error.
void remove_entry(const std::filesystem::path& path, std::error_code& error);
[[nodiscard]] std::filesystem::path known_folder(const std::string& folder);

// The program's arguments as UTF-8, without the program's own name. On Windows they are read again from the
// wide command line: main's are in the system's code page, which has no room for most letters.
[[nodiscard]] std::vector<std::string> program_arguments(int count, const char* const* arguments);

// A path from UTF-8 text, and its text back as UTF-8. OUI converts between paths and text only through these:
// on Windows, std::filesystem's own conversions go through the system's code page, which mangles names with
// letters outside it, and bring in the C++ library's locales, a few hundred kilobytes of every program.
// As text a path has '/' between folders on every system, wherever it goes: shown, kept in settings, given to a
// script. It becomes the system's own, '\' on Windows, only as a path, here, where it is handed to the system.
[[nodiscard]] inline std::filesystem::path path_from_utf8(std::string_view text)
{
#ifdef _WIN32
    std::wstring wide;
    wide.reserve(text.size());
    for(std::size_t index{}; index < text.size();)
    {
        const auto codepoint{decode_utf8(text, index)};
        if(codepoint >= 0x10000)
        {
            wide.push_back(static_cast<wchar_t>(0xD800 + ((codepoint - 0x10000) >> 10)));
            wide.push_back(static_cast<wchar_t>(0xDC00 + ((codepoint - 0x10000) & 0x3FF)));
        }
        else
        {
            wide.push_back(codepoint == U'/' ? L'\\' : static_cast<wchar_t>(codepoint));
        }
    }
    return std::filesystem::path{std::move(wide)};
#else
    return std::filesystem::path{std::string{text}};
#endif
}

[[nodiscard]] inline std::string utf8_of(const std::filesystem::path& path)
{
#ifdef _WIN32
    const auto& wide{path.native()};
    std::string text;
    text.reserve(wide.size());
    for(std::size_t index{}; index < wide.size(); ++ index)
    {
        char32_t codepoint{static_cast<char16_t>(wide[index])};
        if(codepoint >= 0xD800 && codepoint < 0xDC00 && index + 1 < wide.size())
        {
            const char32_t low{static_cast<char16_t>(wide[index + 1])};
            if(low >= 0xDC00 && low < 0xE000)
            {
                codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                ++ index;
            }
        }
        text += encode_utf8(codepoint == U'\\' ? U'/' : codepoint);
    }
    return text;
#else
    return path.native();
#endif
}

// Opens a file with stdio, `mode` as fopen takes it, by its whole path: wide on Windows, so any name opens.
// Streams would do the same and bring iostream and the C++ library's locales into every program.
[[nodiscard]] inline std::FILE* open_file(const std::filesystem::path& path, const char* mode)
{
#ifdef _WIN32
    std::wstring wide_mode;
    for(const auto* character{mode}; *character != '\0'; ++ character)
    {
        wide_mode.push_back(static_cast<wchar_t>(*character));
    }
    return _wfopen(path.c_str(), wide_mode.c_str());
#else
    return std::fopen(path.c_str(), mode);
#endif
}

// The whole of a file's bytes, or nothing when it cannot be read.
[[nodiscard]] inline std::optional<std::string> read_file(const std::filesystem::path& path)
{
    auto* const file{open_file(path, "rb")};
    if(file == nullptr)
    {
        return std::nullopt;
    }
    std::string bytes;
    char buffer[16384];
    for(std::size_t read{}; (read = std::fread(buffer, 1, sizeof buffer, file)) > 0;)
    {
        bytes.append(buffer, read);
    }
    const auto failed{std::ferror(file) != 0};
    std::fclose(file);
    if(failed)
    {
        return std::nullopt;
    }
    return bytes;
}

namespace detail
{
using PathCharacter = std::filesystem::path::value_type;
inline constexpr PathCharacter dot[]{'.', '\0'};
inline constexpr PathCharacter dot_dot[]{'.', '.', '\0'};
} // namespace detail

// `path` without "." parts, nor ".." after a folder they undo, nor a separator at its end, with the system's own
// separators. As path::lexically_normal, which on Windows brings in the C++ library's locales, and
// that keeps a separator at the end.
[[nodiscard]] inline std::filesystem::path normal_path(const std::filesystem::path& path)
{
    if(path.empty())
    {
        return path;
    }
    std::vector<std::filesystem::path> parts;
    for(const auto& part : path.relative_path())
    {
        if(part.empty() || part.native() == detail::dot)
        {
            continue;
        }
        if(part.native() == detail::dot_dot)
        {
            if(!parts.empty() && parts.back().native() != detail::dot_dot)
            {
                parts.pop_back();
                continue;
            }
            // Above the root is the root.
            if(path.has_root_directory())
            {
                continue;
            }
        }
        parts.push_back(part);
    }
    auto normal{path.root_path()};
    for(const auto& part : parts)
    {
        normal /= part;
    }
    if(normal.empty())
    {
        return detail::dot;
    }
    return normal.make_preferred();
}

// How to reach `path` from `base` without leaving the text of either, or nothing when there is no way. As
// path::lexically_relative, which on Windows brings in the C++ library's locales.
[[nodiscard]] inline std::filesystem::path relative_to(const std::filesystem::path& path,
                                                       const std::filesystem::path& base)
{
    if(path.root_name() != base.root_name() || path.is_absolute() != base.is_absolute() ||
       (!path.has_root_directory() && base.has_root_directory()))
    {
        return {};
    }
    auto [inside, from] = std::mismatch(path.begin(), path.end(), base.begin(), base.end());
    if(inside == path.end() && from == base.end())
    {
        return detail::dot;
    }
    int ups{};
    for(; from != base.end(); ++ from)
    {
        if(from->native() == detail::dot_dot)
        {
            -- ups;
        }
        else if(!from->empty() && from->native() != detail::dot)
        {
            ++ ups;
        }
    }
    if(ups < 0)
    {
        return {};
    }
    if(ups == 0 && (inside == path.end() || inside->empty()))
    {
        return detail::dot;
    }
    std::filesystem::path relative;
    for(; ups > 0; -- ups)
    {
        relative /= detail::dot_dot;
    }
    for(; inside != path.end(); ++ inside)
    {
        relative /= *inside;
    }
    return relative;
}

// `path` from the root and without "." or "..", nor a separator at its end: a relative path holds only from where
// the program started, and ".." has no parent of its own to go up to.
[[nodiscard]] inline std::filesystem::path whole_path(const std::filesystem::path& path)
{
    std::error_code ignored;
    return normal_path(std::filesystem::absolute(path, ignored));
}

} // namespace olagarro::oui
