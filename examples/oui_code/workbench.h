#pragma once

#include "editors.h"
#include "find_bar.h"
#include "session_entry.h"
#include "help.h"
#include "navigation.h"
#include "project_search.h"
#include "recovery.h"
#include "script_run.h"
#include "scripting.h"

#include <oui/application.h>
#include <oui/completion.h>
#include <oui/dock_layout.h>
#include <oui/folder_view.h>
#include <oui/menu_bar.h>
#include <oui/settings.h>
#include <oui/split_view.h>
#include <oui/status_bar.h>
#include <oui/suggester.h>
#include <oui/tab_switcher.h>
#include <oui/text_edit.h>
#include <oui/window.h>
#ifdef OLAGARRO_OUI_CODE_WINDOW
#include <oui/graphics/window_backend.h>
#endif

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// How OUI Code was started, as the command line says.
struct WorkbenchOptions
{
    // A test of OUI Code's own: nothing of the user's is read or kept, and the texts are in English.
    bool headless{false};
    bool no_menu{false};
    bool no_explorer{false};
    // The folder to open as the project, whole, in place of the one left.
    std::optional<std::filesystem::path> folder;
    // The files to open, whole, over the session left, the last in front.
    std::vector<std::filesystem::path> files;
    // Two files to compare, whole, as --diff names them.
    std::optional<std::pair<std::filesystem::path, std::filesystem::path>> compare;
    std::string interface_language{"en"};
    // OUI Code shows in characters, in a terminal or on a desktop of them: the terminal themes are its looks.
    bool in_cells{false};
    // The classic Windows console API draws it, which takes fewer colours.
    bool legacy_render{false};
#ifdef OLAGARRO_OUI_CODE_WINDOW
    // The window OUI Code shows in, when it does.
    olagarro::oui::DesktopWindow* window_screen{};
#endif
    // Where a debug build writes every event, to replay a bug; none otherwise.
    std::FILE* event_log{};
};

// What an interactive tab starts with: its script's file, when it has one, its code, and what it runs on, none for a
// script that only prints.
struct InteractiveStart
{
    std::optional<std::filesystem::path> script_file;
    std::string code;
    olagarro::oui::TextEdit* input{};
    std::optional<std::filesystem::path> input_file;
    std::string input_name;
    // Text pasted in the tab to run on, in place of a document: what it starts with.
    std::optional<std::string> pasted;
    // What the pasted text is when it is code Generate wrote, for a processGeneratedCode.
    std::optional<GeneratedFrom> generated;
};

// OUI Code's window and everything in it: the menus, the project tree and the explorer, the groups of tabs with the
// documents, the find bar and the status bar, and the commands that work on them. Made, it is shown; run() runs it
// until it is quit and keeps what is to come back next time.
class Workbench
{
  public:
    Workbench(olagarro::oui::Application& application, olagarro::oui::Settings& settings, WorkbenchOptions options);
    Workbench(const Workbench&) = delete;
    Workbench& operator=(const Workbench&) = delete;

    int run();
    // The document on show, for --input-test to print.
    [[nodiscard]] const olagarro::oui::TextEdit* shown_document();

  private:
    // workbench.cpp: what is on screen, and what comes back of the last time.
    void build_menu_bar(olagarro::oui::DockLayout& workspace);
    void build_side_panels(olagarro::oui::DockLayout& workspace);
    void prepare_editor_tabs();
    void remember_dialog_places();
    void open_first_document(std::unique_ptr<olagarro::oui::TextEdit> editor);
    void watch_external_changes();
    void restore_session();
    // A file given that is not there yet: an empty document under its name, which saving makes.
    void open_file_to_make(const std::filesystem::path& file);
    // A tab of the session that is not a file nor a task list, in the active group, last; false when what it needs is
    // gone.
    bool restore_tab(const SessionTab& tab);
    // The editor of the document of the file, open in any group; none when it is not open.
    [[nodiscard]] olagarro::oui::TextEdit* open_document_editor(const std::filesystem::path& file);
    void install_status_bar();
    void watch_events();
    void finish_window(std::unique_ptr<olagarro::oui::Window> window);
    // Where the window is, to open there the next time; nothing in a terminal.
    void remember_window();
    // In characters, the look picked in View > Theme.
    void use_terminal_theme(std::string_view id);
#ifdef OLAGARRO_OUI_CODE_WINDOW
    // In a window, the look picked in View > Theme.
    void use_window_theme(std::string_view id);
#endif

    // file_commands.cpp
    void register_file_commands();
    // Recent files and folders are built each time their submenu opens, so they are always current.
    [[nodiscard]] std::vector<olagarro::oui::MenuItem> recent_files_menu();
    [[nodiscard]] std::vector<olagarro::oui::MenuItem> recent_folders_menu();
    // Opens a file the user picked, from a panel, a list or a menu, and adds it to the recent files.
    void open_from_panel(const std::filesystem::path& file, OpenAs as = OpenAs::natural);

    // edit_commands.cpp
    void register_edit_commands();
    void register_line_commands();
    void register_format_commands();
    // How the active document is written to disk: shown in the status bar, changed from the palette.
    void set_active_format(const std::function<void(olagarro::oui::TextFormat&)>& change);

    // view_commands.cpp
    void register_view_commands();
    // What the help opens on: the part about what is in the tab shown.
    [[nodiscard]] HelpTopic help_topic();
    [[nodiscard]] std::vector<olagarro::oui::MenuItem> view_menu();
    [[nodiscard]] std::vector<olagarro::oui::MenuItem> file_menu();
    [[nodiscard]] std::vector<olagarro::oui::MenuItem> edit_menu();
    [[nodiscard]] std::vector<olagarro::oui::MenuItem> help_menu();
    // Sets every menu's entries, with the text commands the user's scripts put in each; again when they reload.
    void fill_menus();
    // `items`, a menu's own, with the text commands and tools whose menuPath starts with `menu` after them.
    [[nodiscard]] std::vector<olagarro::oui::MenuItem> with_text_commands(std::string_view menu,
                                                                          std::vector<olagarro::oui::MenuItem> items);
    // View > Syntax: the language a document is coloured as, the file's own unless one is picked.
    [[nodiscard]] std::vector<olagarro::oui::MenuItem> syntax_menu();
    // To the tab `step` away in the active group, wrapping round at the ends.
    void step_tab(int step);

    // tools_commands.cpp
    // Scripts: OUI Code's own, then the user's from a folder beside the settings. They define languages, which colour
    // the documents, and text commands, registered as any other command; and the user's tools, one script each in the
    // folder's tools folder, registered too. Loading them again replaces them all; what failed is given back.
    std::vector<std::string> load_scripts();
    // The tools in the tools folder of the user's scripts, each loaded once to read what its `tool` says: what failed.
    std::vector<std::string> load_tools();
    // What failed in the scripts is said, not hidden: a script that does not load does nothing else.
    void report_script_problems(const std::vector<std::string>& problems);
    void register_tool_commands();
    [[nodiscard]] std::vector<olagarro::oui::MenuItem> tools_menu();
    void open_interactive_script();
    // An interactive tab, in the active group, as open_interactive_script or a session coming back starts one.
    void start_interactive_script(InteractiveStart start);
    // Tools > Interactive UI: on the description open, or on a new one from an example.
    void open_interactive_ui();
    // The tab, on `file` or on a description without one, starting with `text`.
    void start_interactive_ui(std::optional<std::filesystem::path> file, std::string_view text);
    // An Interactive UI tab's Generate: code that opens its window, a script for oui_run or C++, in a new tab.
    void generate_ui_code(const InteractiveUiSession& session, SkeletonLanguage language);
    // The script that runs after Generate in `language`, kept in the settings: one for each language, or none.
    [[nodiscard]] std::optional<std::filesystem::path> generate_script(SkeletonLanguage language) const;
    // Each Interactive UI tab shows the script for the language chosen in it.
    void show_generate_scripts();
    // Asks for a script to run after Generate in `language`, kept when it exports processGeneratedCode.
    void choose_generate_script(SkeletonLanguage language);
    // The code Generate writes in `language` for the description, and the description's file, saved first when it
    // has none; nothing when it is not saved or does not read, which is said.
    [[nodiscard]] std::optional<std::pair<std::filesystem::path, std::string>>
    generated_ui_code(const InteractiveUiSession& session, SkeletonLanguage language);
    // A new script to run after Generate in `language`, from the template, in an Interactive Script tab that runs it
    // on what Generate writes now; saved, it is the language's script.
    void create_generate_script(const InteractiveUiSession& session, SkeletonLanguage language);
    // A document was saved under a file it did not have: an interactive script's imports start from there, and a
    // script written to run after Generate is the one that does.
    void saved_under_new_file(Document& document);
    void open_terminal();
    // comparison.cpp: Tools > Compare Files, Compare with Saved, and the tab they open.
    void register_compare_commands();
    // A file as a side to compare: its open document when it has one, followed as it changes; its text otherwise.
    [[nodiscard]] std::optional<ComparedSide> compared_file(const std::filesystem::path& file);
    void open_comparison(ComparedSide left, ComparedSide right, bool against_saved = false);
    // The document against its file as last saved.
    void compare_with_saved(olagarro::oui::TextEdit& editor, const std::filesystem::path& file);
    // A calculator tab, with what was written in it before worked out again.
    void open_calculator(const std::vector<std::string>& entries);
    // A tab that shows the Markdown document as it is edited, named after it.
    void open_markdown_preview(olagarro::oui::TextEdit& editor, const std::string& source_title);
    // A link followed in a preview: another Markdown file is read in it, a section of one too; a web address goes
    // to the browser; any other file opens as a document.
    void follow_preview_link(PreviewReading& reading, const std::string& target);
    // Shows `place` in the preview `reading` is of, where its tab is.
    void show_preview_place(PreviewReading& reading, const PreviewReading::Place& place);
    // Back or forward over the places the preview on show has been taken; false when it has nowhere to go, or it
    // is not a preview.
    bool step_preview(bool back);
    // The group and the tab of the preview `reading` is of.
    [[nodiscard]] std::optional<std::pair<EditorGroup*, std::size_t>> preview_tab(const PreviewReading& reading);
    // To the line chosen in a comparison, in its document, opening its file when it is not open.
    void go_to_compared_line(Comparison& comparison, olagarro::oui::DiffSide chosen, std::optional<std::size_t> line);
    // Tools > Script Templates: a script of each kind to start from, written where the cursor is.
    void write_template(std::string_view text);

    // run_script.cpp
    // Tools > Run Script: a script the user picks, kept for the next run, that exports processFile(file, dryRun) to
    // work on the document or process(dryRun, folder) to work on its own. F5 runs it as a dry run, which should change
    // nothing and say what it would do; a processFile then also works on a copy of the document, and what it made of
    // the copy opens in a new tab. Shift+F5 runs it for real.
    void run_script(bool dry_run, bool choose);
    // The script that runs after Generate: processGeneratedCode(code, {language, file}) of `script`, on `code`
    // generated in `language` from the description at `file`, with what it says in the run's output tab, and what it
    // gives back at the end.
    void run_generate_script(const std::filesystem::path& script, std::string_view code, SkeletonLanguage language,
                             const std::filesystem::path& file);
    // A tool of the user's, by its command's id: the current document is saved, then the tool's run(context) gets what
    // the editor has; text it gives back replaces the selection, and what it says opens the output tab.
    void run_tool(const std::string& id);
    // After a run for real: what the script changed shows, in open documents and in the panels' folders.
    void show_script_changes();
    // The tab that shows what the script run says, looked for each time: the user may have closed it.
    [[nodiscard]] ScriptOutput* script_output_tab();
    // A run whose tab was closed is not wanted: it stops, and what it started with it.
    void drop_unseen_run();
    // Whether a script runs, so no other may start: then its output tab shows, to see it or stop it, and `title` says.
    [[nodiscard]] bool refuse_while_running(const std::string& title);
    // Opens the output tab for a new run, titled `title`, with `heading` over the output: the tab there is, wherever
    // it is, or a new one in the active group.
    void open_script_output(const std::string& title, const std::string& heading);
    void append_script_output(std::string_view text);
    void end_script_output();

    // search_commands.cpp
    void register_search_commands();
    // Which group holds the results tab of Find in Files, and where in it.
    [[nodiscard]] std::optional<std::pair<EditorGroup*, std::size_t>> find_results_tab();
    // Looks through the project's files for `query` and lists what it finds in the results tab, titled `title` and
    // the count of matches.
    void search_project(const FindInFilesQuery& query, std::string title);

    // panel_commands.cpp: file operations in either panel. Its own keys (Insert, F7, F2, Delete, F5) act on it, and
    // the palette on the panel that has the focus.
    void register_panel_commands();
    [[nodiscard]] olagarro::oui::FolderView& other_panel(const olagarro::oui::FolderView& panel);
    void create_entry(olagarro::oui::FolderView& panel, bool folder);
    void rename_entry(olagarro::oui::FolderView& panel);
    void delete_entry(olagarro::oui::FolderView& panel);
    // Copies the selected entry into the folder the other panel shows, as two-panel file managers do: from the
    // explorer into the project, or out of it.
    void copy_entry(olagarro::oui::FolderView& panel);
    // A right click offers what the keys do, for whoever looks for it rather than knows the key.
    void show_panel_menu(olagarro::oui::FolderView& panel, olagarro::oui::Point at);

    olagarro::oui::Application& m_application;
    olagarro::oui::Settings& m_settings;
    WorkbenchOptions m_options;
    // Where the user's own scripts are, beside the settings.
    std::filesystem::path m_scripts_folder;

    // The window and what is in it, which the window owns.
    olagarro::oui::Window* m_window{};
    olagarro::oui::MenuBar* m_menu_bar{};
    // The project tree on the left, held inside the project; the explorer on the right, free to roam the whole file
    // system and bring things into the project.
    olagarro::oui::FolderView* m_project_tree{};
    olagarro::oui::FolderView* m_file_explorer{};
    olagarro::oui::StatusBar* m_status_bar{};
    FindBar* m_find_bar{};
    // The documents: groups of tabs that splitting divides. The area goes into the window as it is built.
    std::unique_ptr<olagarro::oui::SplitView> m_editor_area{olagarro::oui::split_view()};
    Editors m_editors{m_application, *m_editor_area};

    UnsavedBackup m_unsaved_backup{m_application, m_settings, m_editors};
    // What OUI's scripts can use, for the suggestions in scripts.
    olagarro::oui::ScriptApi m_script_api;
    olagarro::oui::Suggester m_suggester;
    NavigationHistory m_navigation;
    ProjectSearch m_project_search{m_application};
    olagarro::oui::TabSwitcher m_tab_switcher;
    // The tab the tab list started on, which Escape goes back to.
    std::optional<std::size_t> m_switcher_original_index;

    std::unique_ptr<Scripting> m_scripting;
    // A tool of the user's: its command's id, what its `tool` says, and its script, loaded again at each run.
    struct UserTool
    {
        std::string id;
        CommandPlacement placement;
        std::filesystem::path script;
    };
    std::vector<UserTool> m_tools;
    // The commands the scripts registered, which loading them again takes away first.
    std::vector<std::string> m_script_command_ids;

    std::vector<std::filesystem::path> m_recent_files;
    std::vector<std::filesystem::path> m_recent_folders;
    // Where the file dialogs start: the folder of what was opened or saved last.
    std::filesystem::path m_current_folder;
    // The folder opened as the project, if any.
    std::optional<std::filesystem::path> m_project_folder;

    // The script running, if one is.
    std::unique_ptr<ScriptRun> m_script_run;
    // While a tool runs, whether its output tab has opened, which it does once it says something, and what opens it,
    // to see it or stop it: one that has said nothing has no tab, and is not dropped for it.
    std::shared_ptr<bool> m_tool_run_shown;
    std::function<void()> m_reveal_tool_run;
    // The document the last script run was on, to go on with from the tab that shows its output; only compared, as
    // it may be gone.
    olagarro::oui::WidgetRef<const olagarro::oui::TextEdit> m_last_script_subject;
    std::string m_last_filter_command;
    // Find in Files: the last query comes back in the dialog, and its matches fill one tab, in the order of the files
    // and of the text.
    FindInFilesQuery m_last_find_in_files;
    std::vector<FileMatch> m_find_results;
};
