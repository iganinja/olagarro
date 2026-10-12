#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace olagarro::oui
{

enum class SettingsLocation
{
    next_to_executable,
    app_data,
    // Beside the executable when its folder can be written, as a program carried on a stick or unpacked in a home
    // folder; the user's configuration folder when not, as a program installed for the whole system.
    portable
};

class Settings
{
  public:
    // "<app_name>.ini" beside the executable, or in the user's configuration folder.
    Settings(SettingsLocation location, std::string_view app_name);
    // A settings file at a path of the application's choosing.
    explicit Settings(std::filesystem::path path);

    [[nodiscard]] bool load();
    bool save();

    [[nodiscard]] std::string_view value(std::string_view key, std::string_view default_value = {}) const;
    void set_value(std::string_view key, std::string_view value);
    // Forgets the key, so the next save leaves it out of the file.
    void remove(std::string_view key);

    [[nodiscard]] bool bool_value(std::string_view key, bool default_value = false) const;
    void set_bool_value(std::string_view key, bool value);

    [[nodiscard]] int int_value(std::string_view key, int default_value = 0) const;
    void set_int_value(std::string_view key, int value);

    [[nodiscard]] std::vector<std::string> list_value(std::string_view key) const;
    void set_list_value(std::string_view key, const std::vector<std::string>& values);

    [[nodiscard]] const std::filesystem::path& file_path() const;

  private:
    std::filesystem::path m_path;
    std::unordered_map<std::string, std::string> m_data;
};

} // namespace olagarro::oui
