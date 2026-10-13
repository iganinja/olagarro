#pragma once

#include "editors.h"
#include "recovery.h"
#include <oui/suggester.h>

#include <oui/application.h>
#include <oui/settings.h>

// Edit > Preferences: the interface's language, how documents indent and suggest, and what is done with files
// changed on disk and with unsaved work. Each choice is kept as it is made; OK applies the indentation to the open
// documents.
void show_preferences(olagarro::oui::Application& application, olagarro::oui::Settings& settings, Editors& editors,
                      UnsavedBackup& unsaved_backup, olagarro::oui::Suggester& suggester);
