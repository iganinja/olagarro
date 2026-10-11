#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace olagarro::oui
{

class CommandLine
{
  public:
    // The arguments after the program's name, as program_arguments gives them.
    explicit CommandLine(const std::vector<std::string>& arguments);

    [[nodiscard]] bool has_option(std::string_view option) const;

    [[nodiscard]] std::optional<std::string_view> positional(std::size_t index) const;

  private:
    std::vector<std::string> m_options;
    std::vector<std::string> m_positionals;
};

} // namespace olagarro::oui
