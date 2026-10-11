#include <oui/translation.h>

#include <windows.h>

namespace olagarro::oui
{

std::string system_language()
{
    // A locale name reads like "es-ES"; the language is the part before '-'.
    wchar_t name[LOCALE_NAME_MAX_LENGTH]{};
    if(GetUserDefaultLocaleName(name, LOCALE_NAME_MAX_LENGTH) == 0)
    {
        return "en";
    }
    std::string language;
    for(const auto* c{name}; *c != L'\0' && *c != L'-'; ++ c)
    {
        language += static_cast<char>(*c);
    }
    return language.empty() ? "en" : language;
}

} // namespace olagarro::oui
