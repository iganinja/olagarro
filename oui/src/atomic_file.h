#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace olagarro::oui
{

// Replaces the contents of `path` with `bytes` through a temporary file in the same folder that is then
// renamed over it: readers see either the old contents or the new ones, never a partial file, and a
// failure leaves the old contents in place. On failure returns false and puts the reason in `error`.
[[nodiscard]] bool replace_file_contents(const std::filesystem::path& path, std::string_view bytes, std::string& error);

} // namespace olagarro::oui
