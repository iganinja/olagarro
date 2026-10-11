#include <oui/translation.h>

#include <cstdlib>

namespace olagarro::oui
{

std::string system_language()
{
    // The order gettext follows: LANGUAGE is a list of preferences, and the first locale variable that is
    // set wins. A locale reads like "es_ES.UTF-8"; the language is the part before '_', '.' or '@'.
    for(const auto* const variable : {"LANGUAGE", "LC_ALL", "LC_MESSAGES", "LANG"})
    {
        const auto* const value{std::getenv(variable)};
        if(value == nullptr || value[0] == '\0')
        {
            continue;
        }
        const std::string_view locale{value};
        const auto language{std::string{locale.substr(0, locale.find_first_of("_.@:"))}};
        if(language.empty() || language == "C" || language == "POSIX")
        {
            return "en";
        }
        return language;
    }
    return "en";
}

} // namespace olagarro::oui
