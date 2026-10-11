#include "ui_descriptions.h"

using namespace olagarro;

oui::Ui build_oui_code_ui(OuiCodeUi which)
{
    return oui::build_ui(embedded_oui_code_ui().at(static_cast<std::size_t>(which)));
}
