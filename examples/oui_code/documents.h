#pragma once

#include "comparison.h"
#include "interactive_view.h"
#include "languages.h"
#include "live_script.h"
#include "ui_preview.h"

#include <oui/application.h>
#include <oui/markdown_view.h>
#include <oui/settings.h>
#include <oui/tab_view.h>
#include <oui/text_edit.h>
#include <oui/text_file.h>

#include <signalslot/signalslot.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// The documents OUI Code edits: what it knows about the file behind each tab, how a tab is named, and saving and
// reloading.

// What keeps an interactive tab going: its script runs again a moment after each change, to the code or to what
// it runs on, and the tab shows what came of it.
struct InteractiveSession
{
    InteractiveView* view{};
    // What the script runs on, while it is open; none for a script that only prints.
    olagarro::oui::WidgetRef<olagarro::oui::TextEdit> input;
    std::optional<std::filesystem::path> input_file;
    std::string input_name;
    // Where the code runs from, for its imports: its own file, or beside what it runs on.
    std::filesystem::path module_path;
    // What its input is when it is code Generate wrote, for a processGeneratedCode.
    std::optional<GeneratedFrom> generated;
    std::unique_ptr<LiveScript> live;
    olagarro::oui::Timer pending;
    olagarro::signal_slot::ScopedConnection code_changed;
    olagarro::signal_slot::ScopedConnection input_changed;
};

// What keeps an Interactive UI tab going: its description is built again a moment after each change.
struct InteractiveUiSession
{
    UiPreviewView* view{};
    olagarro::oui::Timer pending;
    olagarro::signal_slot::ScopedConnection code_changed;
};

// Where a Markdown preview has been taken by the links followed in it. It shows the document it previews, following
// it as it is edited, until a link to another Markdown file takes it there, to read it as it is on disk; back and
// forward go over the places it showed.
struct PreviewReading
{
    // A place it showed: a file, or none for the document it previews, and how far down it was.
    struct Place
    {
        std::optional<std::filesystem::path> file;
        int scroll{};
    };
    olagarro::oui::MarkdownView* view{};
    olagarro::oui::WidgetRef<olagarro::oui::TextEdit> source;
    std::string source_title;
    // The file it shows; none while it shows the document.
    std::optional<std::filesystem::path> file;
    std::vector<Place> back;
    std::vector<Place> forward;
};

// What OUI Code knows about the file behind a tab.
struct Document
{
    std::optional<std::filesystem::path> path;
    olagarro::oui::TextFormat format;
    // The file as it was on disk when last loaded or saved, to notice changes made by someone else.
    std::optional<olagarro::oui::FileStamp> stamp;
    // A tab that shows another document instead of holding one, such as a Markdown preview: the editor it
    // follows, and the connection that keeps it up to date. A tab holds a text editor only when this is unset.
    olagarro::oui::WidgetRef<const olagarro::oui::TextEdit> preview_of;
    olagarro::signal_slot::Connection preview_updates;
    // Where the preview's links have taken it.
    std::shared_ptr<PreviewReading> reading{};
    // The tab that lists what Find in Files found; there is one at most.
    bool holds_find_results{false};
    // The tab that shows what a script run says; there is one at most, and each run takes it over.
    bool holds_script_output{false};
    // An interactive tab's script, run as it changes; its code is the tab's document.
    std::shared_ptr<InteractiveSession> interactive{};
    // An Interactive UI tab's description, built as it changes; it is the tab's document.
    std::shared_ptr<InteractiveUiSession> interactive_ui{};
    // The calculator's tab; there is one at most.
    bool holds_calculator{false};
    // A comparison tab's two sides, followed as they change.
    std::shared_ptr<Comparison> comparison{};
    // What language the file is in, as chosen for language_path; none for a file of no known language.
    std::shared_ptr<const Language> language{};
    std::optional<std::filesystem::path> language_path{};
    // The language picked in View > Syntax, by id, over the file's own; "" for plain text, none to follow the
    // file.
    std::optional<std::string> chosen_language{};

    // Whether the tab is an interactive one, a script's or a description's: its document is a second editor on its
    // file, beside the tab that may hold it too.
    [[nodiscard]] bool is_interactive() const { return interactive != nullptr || interactive_ui != nullptr; }
};

// The text editor a tab's document is in: the tab's own content, or the code of an interactive tab; none in a tab
// that holds no document, as a terminal or what Find in Files found.
[[nodiscard]] olagarro::oui::TextEdit* document_editor(olagarro::oui::TabView& tabs, std::size_t index);
[[nodiscard]] olagarro::oui::TextEdit* selected_document_editor(olagarro::oui::TabView& tabs);

[[nodiscard]] std::string title_for(const std::filesystem::path& path);
// A tab's title for a document: its file's name, said to be live in an interactive tab, so it stands apart from the
// file's own tab beside it.
[[nodiscard]] std::string document_tab_title(const std::filesystem::path& path, bool interactive);
// A file's whole path as OUI Code shows it, with the home folder as ~ where that is the custom.
[[nodiscard]] std::string shown_path(const std::filesystem::path& path);
// `text` cut from its start to fit `width` cells, with "…" where it was cut, as a path keeps its end.
[[nodiscard]] std::string fit_from_start(std::string text, int width);

// A text editor as OUI Code's documents have it, and the settings for tabs and wrapping applied to one.
[[nodiscard]] std::unique_ptr<olagarro::oui::TextEdit> make_editor();
void apply_tab_settings(olagarro::oui::TextEdit& editor, olagarro::oui::Settings& settings);

// Saves the editor text to `target` in the document's format. It asks before overwriting changes someone
// else made to the file, offers UTF-8 when the text no longer fits the file's encoding, and reports any
// failure; the document is only marked saved, and only takes `target` as its path, when the file was
// written.
bool save_document(olagarro::oui::Application& application, Document& document, olagarro::oui::TextEdit& editor,
                   const std::filesystem::path& target);
// Looks at the file behind a document for changes someone else made since it was loaded or saved. With
// no unsaved edits and automatic reloading on, it reloads; otherwise it asks. A deleted file is left
// alone: saving writes it again.
void check_external_change(olagarro::oui::Application& application, Document& document, olagarro::oui::TextEdit& editor,
                           bool reload_automatically);
