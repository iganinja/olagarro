#pragma once

#include <oui/translation.h>

#include <vector>

namespace olagarro::paint
{

// The panels' catalogs, built in from `paint/translations`: a program gives them to `oui::install_language` with its
// own, before them, so that the program's may translate the same texts otherwise.
[[nodiscard]] const std::vector<oui::Translation>& panel_translations();

} // namespace olagarro::paint
