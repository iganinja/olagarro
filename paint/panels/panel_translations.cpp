#include <paint/panels/panel_translations.h>

#include <string_view>

namespace olagarro::paint
{

// Defined in the source CMake generates from the catalogs it builds in.
std::vector<std::string_view> embedded_paint_catalogs();

const std::vector<oui::Translation>& panel_translations()
{
    static const std::vector<oui::Translation> translations{oui::embedded_translations(embedded_paint_catalogs())};
    return translations;
}

} // namespace olagarro::paint
