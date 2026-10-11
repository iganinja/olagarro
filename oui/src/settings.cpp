#include <oui/settings.h>
#include <oui/file_system.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>

#include "settings_platform.h"

namespace olagarro::oui
{

namespace
{

std::string_view trim(std::string_view s)
{
    while(!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r'))
    {
        s.remove_prefix(1);
    }
    while(!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
    {
        s.remove_suffix(1);
    }
    return s;
}

// Whether a file can be made in `folder`, found by making one: permissions alone do not tell, as on Windows, where
// a folder of the system is not read-only and still refuses.
[[nodiscard]] bool can_write_in(const std::filesystem::path& folder, std::string_view app_name)
{
    const auto probe{folder / path_from_utf8("." + std::string{app_name} + ".write_test")};
    auto* const file{open_file(probe, "wb")};
    if(file == nullptr)
    {
        return false;
    }
    std::fclose(file);
    std::error_code ec;
    remove_entry(probe, ec);
    return true;
}

} // namespace

Settings::Settings(SettingsLocation location, std::string_view app_name)
{
    auto dir{location == SettingsLocation::app_data ? settings_app_data_dir(app_name) : settings_executable_dir()};
    if(location == SettingsLocation::portable && !can_write_in(dir, app_name))
    {
        dir = settings_app_data_dir(app_name);
    }
    m_path = dir / path_from_utf8(std::string{app_name} + ".ini");
}

Settings::Settings(std::filesystem::path path) : m_path{std::move(path)}
{
}

bool Settings::load()
{
    const auto read{read_file(m_path)};
    if(!read)
    {
        return false;
    }
    const auto& text{*read};
    m_data.clear();
    for(std::size_t start{}; start < text.size();)
    {
        const auto end{std::min(text.find('\n', start), text.size())};
        const std::string_view line{std::string_view{text}.substr(start, end - start)};
        start = end + 1;
        if(line.empty() || line.front() == '#')
        {
            continue;
        }
        const auto eq{line.find('=')};
        if(eq == std::string::npos)
        {
            continue;
        }
        const auto key{trim(line.substr(0, eq))};
        // What follows the value is kept, as it was written: a list's last entry may end in a tab. Only a Windows
        // line end goes, and the blanks a person puts after the =.
        auto value{line.substr(eq + 1)};
        if(!value.empty() && value.back() == '\r')
        {
            value.remove_suffix(1);
        }
        while(!value.empty() && (value.front() == ' ' || value.front() == '\t'))
        {
            value.remove_prefix(1);
        }
        if(!key.empty())
        {
            m_data[std::string{key}] = std::string{value};
        }
    }
    return true;
}

bool Settings::save()
{
    std::error_code ec;
    std::filesystem::create_directories(m_path.parent_path(), ec);
    std::string text;
    for(const auto& [key, value] : m_data)
    {
        text += key + '=' + value + '\n';
    }
    auto* const output{open_file(m_path, "wb")};
    if(output == nullptr)
    {
        return false;
    }
    const auto written{std::fwrite(text.data(), 1, text.size(), output) == text.size()};
    return std::fclose(output) == 0 && written;
}

std::string_view Settings::get(std::string_view key, std::string_view default_value) const
{
    const auto it{m_data.find(std::string{key})};
    return it != m_data.end() ? std::string_view{it->second.data(), it->second.size()} : default_value;
}

void Settings::set(std::string_view key, std::string_view value)
{
    m_data[std::string{key}] = std::string{value};
}

void Settings::remove(std::string_view key)
{
    m_data.erase(std::string{key});
}

bool Settings::get_bool(std::string_view key, bool default_value) const
{
    const auto val{get(key, default_value ? "true" : "false")};
    return val == "true";
}

void Settings::set_bool(std::string_view key, bool value)
{
    set(key, value ? "true" : "false");
}

int Settings::get_int(std::string_view key, int default_value) const
{
    const auto val{get(key)};
    if(val.empty())
    {
        return default_value;
    }
    int result{};
    const auto [ptr, err]{std::from_chars(val.data(), val.data() + val.size(), result)};
    return (err == std::errc{}) ? result : default_value;
}

void Settings::set_int(std::string_view key, int value)
{
    std::array<char, 16> buffer{};
    const auto [ptr, err]{std::to_chars(buffer.data(), buffer.data() + buffer.size(), value)};
    if(err == std::errc{})
    {
        set(key, std::string_view{buffer.data(), static_cast<std::size_t>(ptr - buffer.data())});
    }
}

std::vector<std::string> Settings::get_list(std::string_view key) const
{
    const std::string raw{get(key)};
    std::vector<std::string> result;
    for(std::size_t start{}; start <= raw.size();)
    {
        const auto end{std::min(raw.find(';', start), raw.size())};
        if(end > start)
        {
            result.push_back(raw.substr(start, end - start));
        }
        start = end + 1;
    }
    return result;
}

void Settings::set_list(std::string_view key, const std::vector<std::string>& values)
{
    std::string raw;
    for(std::size_t i{}; i < values.size(); ++ i)
    {
        if(i > 0)
        {
            raw += ';';
        }
        raw += values[i];
    }
    set(key, raw);
}

const std::filesystem::path& Settings::file_path() const
{
    return m_path;
}

} // namespace olagarro::oui
