#include "settings_platform.h"

#include <oui/file_system.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace olagarro::oui
{

std::filesystem::path settings_executable_dir()
{
    wchar_t buffer[MAX_PATH]{};
    const auto len{GetModuleFileNameW(nullptr, buffer, MAX_PATH)};
    if(len == 0 || len >= MAX_PATH)
    {
        return {};
    }
    return std::filesystem::path{buffer}.parent_path();
}

std::filesystem::path settings_app_data_dir(std::string_view app_name)
{
    wchar_t buffer[MAX_PATH]{};
    const auto len{GetEnvironmentVariableW(L"APPDATA", buffer, MAX_PATH)};
    if(len == 0 || len >= MAX_PATH)
    {
        return {};
    }
    return std::filesystem::path{buffer} / path_from_utf8(app_name);
}

} // namespace olagarro::oui
