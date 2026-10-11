#include <oui/command_line.h>

namespace olagarro::oui
{

CommandLine::CommandLine(const std::vector<std::string>& arguments)
{
    for(std::size_t index{}; index < arguments.size(); ++ index)
    {
        const std::string_view argument{arguments[index]};
        if(argument == "--")
        {
            m_positionals.insert(m_positionals.end(), arguments.begin() + static_cast<std::ptrdiff_t>(index) + 1,
                                 arguments.end());
            break;
        }
        if(argument.rfind("--", 0) == 0)
        {
            m_options.emplace_back(argument.substr(2));
        }
        else
        {
            m_positionals.emplace_back(argument);
        }
    }
}

bool CommandLine::has_option(std::string_view option) const
{
    for(const auto& value : m_options)
    {
        if(value == option)
        {
            return true;
        }
    }
    return false;
}

std::optional<std::string_view> CommandLine::positional(std::size_t index) const
{
    if(index >= m_positionals.size())
    {
        return std::nullopt;
    }
    return m_positionals.at(index);
}

} // namespace olagarro::oui
