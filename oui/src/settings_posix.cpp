#include "settings_platform.h"

#include <cstdlib>
#include <system_error>

namespace olagarro::oui
{

std::filesystem::path settings_executable_dir()
{
    std::error_code ec;
    const auto executable{std::filesystem::read_symlink("/proc/self/exe", ec)};
    if(ec)
    {
        return {};
    }
    return executable.parent_path();
}

std::filesystem::path settings_app_data_dir(std::string_view app_name)
{
    if(const auto config_home{std::getenv("XDG_CONFIG_HOME")}; config_home && config_home[0] != '\0')
    {
        return std::filesystem::path{config_home} / app_name;
    }
    if(const auto home{std::getenv("HOME")}; home && home[0] != '\0')
    {
        return std::filesystem::path{home} / ".config" / app_name;
    }
    return {};
}

} // namespace olagarro::oui
