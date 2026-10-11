#pragma once

#include <oui/application.h>
#include <oui/file_dialog.h>
#include <oui/text_edit.h>

#include <filesystem>
#include <string>

// OUI Code's own dialogs, and the options of the ones OUI has.

// Shows Markdown in a dialog that fills most of the screen and scrolls, as the help and previews do.
void show_markdown(olagarro::oui::Application& application, const std::string& title, std::string markdown,
                   const std::string& placement_key);
void show_palette(olagarro::oui::Application& application);
// Help > About: the version, and who made it.
void show_about(olagarro::oui::Application& application);
// OUI Code's file dialogs: the file types an editor offers, and where on screen each kind was left.
[[nodiscard]] olagarro::oui::FileDialogOptions file_dialog_options(olagarro::oui::FileDialogMode mode,
                                                                   const std::filesystem::path& folder);
void show_error(olagarro::oui::Application& application, const std::string& title, const std::string& message);

enum class UnsavedChangesAction
{
    save,
    discard,
    cancel
};

[[nodiscard]] UnsavedChangesAction confirm_unsaved_changes(olagarro::oui::Application& application);
void show_replace_dialog(olagarro::oui::Application& application, olagarro::oui::TextEdit& document);
void show_go_to_line_dialog(olagarro::oui::Application& application, olagarro::oui::TextEdit& document);
// Runs a shell command with the selection, or the whole document when nothing is selected, as its input,
// and puts its output in their place, as one step to undo. A window says it is running, and Esc stops it.
// A command that fails leaves the text alone and shows what it wrote to its error output. `last_command` is what
// it offers, and keeps what was run.
void filter_through_command(olagarro::oui::Application& application, olagarro::oui::TextEdit& editor,
                            std::string& last_command);
