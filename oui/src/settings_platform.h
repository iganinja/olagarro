#pragma once

#include <filesystem>
#include <string_view>

namespace olagarro::oui
{

// Directory holding the running executable, or empty when it cannot be resolved.
std::filesystem::path settings_executable_dir();

// Per-user configuration directory for app_name, or empty when it cannot be resolved.
std::filesystem::path settings_app_data_dir(std::string_view app_name);

} // namespace olagarro::oui
