# OUI Code's architecture

OUI Code is the text editor in `examples/oui_code`, built on OUI. This document is a map for someone changing it: which
file holds what, and how the pieces talk to each other. It does not describe what OUI Code does for its user: that is
the help, [help.md](../examples/oui_code/help/help.md) (and [help.es.md](../examples/oui_code/help/help.es.md) in
Spanish), which OUI Code carries inside and shows with F1. OUI itself is described in [the overview](overview.md),
[layout](oui_layout.md) and [user interface descriptions](oui_descriptions.md); scripting from C++ in
[OUIScript](ouiscript.md).

- [The shape of it](#the-shape-of-it)
- [main.cpp and the command line](#maincpp-and-the-command-line)
- [The Workbench](#the-workbench)
- [Documents, editors, tabs and groups](#documents-editors-tabs-and-groups)
- [Commands, menus and the palette](#commands-menus-and-the-palette)
- [Settings, the session and unsaved work](#settings-the-session-and-unsaved-work)
- [Scripting](#scripting)
- [Other tabs](#other-tabs)
- [Search and navigation](#search-and-navigation)
- [Help, dialogs and translations](#help-dialogs-and-translations)
- [Where it shows: terminal, window, console, desktop](#where-it-shows-terminal-window-console-desktop)
- [Crashes](#crashes)
- [Tests](#tests)

## The shape of it

`main.cpp` reads the command line, picks a backend, makes the `Application` and a `Workbench`, and calls
`Workbench::run()`. The `Workbench` (declared in [workbench.h](../examples/oui_code/workbench.h)) is OUI Code's window
and everything in it: it owns the state, registers every command, and builds the menus from them. Its member
functions are spread over several sources by subject, and the things it works on (documents, groups of tabs, the
session, scripts, searches) are classes and functions of their own, which know nothing of the `Workbench`.

Everything runs on the application's thread. Work that would block it runs elsewhere and comes back with
`Application::post`: Find in Files searches on other threads, programs report through OUI's `Process`, and the
terminal tab reads its pseudo-terminal the same way.

What OUI Code carries inside it is embedded at build time by `olagarro_embed_files` in the top
[CMakeLists.txt](../CMakeLists.txt), and declared in [resources.h](../examples/oui_code/resources.h) and
[ui_descriptions.h](../examples/oui_code/ui_descriptions.h):

| Embedded | From | Read by |
|----------|------|---------|
| `embedded_oui_code_scripts()` | `scripts/prelude.js`, `scripts/languages/*.js`, `scripts/commands.js` | `built_in_scripts()`, which names them in the same order |
| `embedded_oui_code_ui()` | `ui/*.oui`, in the order of `OuiCodeUi` | `build_oui_code_ui(OuiCodeUi::...)` |
| `embedded_oui_code_help()` | `help/help.md`, `help/help.es.md` | `help_markdown` |
| `embedded_oui_code_catalogs()` | `translations/es.json` | `oui_code_translations()` |
| `embedded_oui_script_api()` | `oui/oui.d.ts` | `ScriptApi`, for suggestions in scripts |

The order of the `.oui` files in `OLAGARRO_OUI_CODE_UI` and of the scripts in CMake must match `OuiCodeUi` and
`built_in_scripts()`: they are found by index.

## main.cpp and the command line

[main.cpp](../examples/oui_code/main.cpp) does, in order:

1. Installs the crash reporter (see [Crashes](#crashes)).
2. Reads the options with `oui::CommandLine`: `--help`, `--no-menu`, `--no-explorer`, `--diff left right`, the
   backends (`--window`, `--drm`, `--desktop`, each only in a build that has it), `--legacy-render` (the classic
   Windows console API), and the self tests (`--smoke-test`, `--stress-test`, `--input-test`, `--key-test`). The
   positional arguments are files to open and at most one folder, made whole so the session keeps them right.
3. Loads the settings, `oui::Settings{SettingsLocation::portable, "oui_code"}`: `oui_code.ini` beside the executable
   when its folder can be written, in the user's configuration folder when not. `--smoke-test` backs the file up and
   puts it back after.
4. Installs the interface language, the `language` setting or the system's; the self tests run in English.
5. Makes the backend: a scripted one for the self tests, the desktop window, the DRM screen, OUI's desktop, or the
   terminal. Picks the theme: Turbo C's in characters, `classic_theme()` in pixels; `in_cells` tells the `Workbench`
   which.
6. Fills `WorkbenchOptions` and makes the `Workbench`. A debug build (`OLAGARRO_DEBUG_BUILD`) also opens
   `oui_code_events.log`, where [event_log.cpp](../examples/oui_code/event_log.cpp) writes every event to replay a
   bug.

The self tests themselves are in [self_tests.cpp](../examples/oui_code/self_tests.cpp): a list of events the scripted
backend feeds, or, for `--key-test`, the real terminal.

## The Workbench

The `Workbench` constructor builds the window from `ui/main_window.oui`, a window holding one `DockLayout` named
`workspace`, and fills it:

| Step | Function | File |
|------|----------|------|
| Menu bar, five empty menus | `build_menu_bar` | workbench.cpp |
| Project tree (left) and explorer (right), status bar | `build_side_panels` | workbench.cpp |
| What every group of tabs does: tooltips, closing, dragging | `prepare_editor_tabs` | workbench.cpp |
| Languages and text commands | `load_scripts` | tools_commands.cpp |
| Commands, by subject | `register_*_commands` | see below |
| The session of last time, then the files given | `restore_session` | workbench.cpp |
| Menu entries | `fill_menus` | workbench.cpp |
| Find bar under the documents, the editor area | `workspace.add` | workbench.cpp |
| Status bar contents | `install_status_bar` | workbench.cpp |
| A `--diff` comparison | `open_comparison` | comparison.cpp |
| The window shown, closing and shutdown wired | `finish_window` | workbench.cpp |

`run()` starts the unsaved-work copies, runs `Application::exec()`, then saves the session and the panels.

The member functions live in a source per subject, as [CONTRIBUTING.md](../CONTRIBUTING.md) lists:

| Source | What it has |
|--------|-------------|
| [workbench.cpp](../examples/oui_code/workbench.cpp) | building the window, restoring the session, the status bar, the event observer, themes |
| [file_commands.cpp](../examples/oui_code/file_commands.cpp) | File menu: new, open, save, close, quit, recent files and folders, Go to File |
| [edit_commands.cpp](../examples/oui_code/edit_commands.cpp) | Edit menu: undo, find, replace, go to line, suggest, back and forward, lines, Filter Through Command, formats, Preferences |
| [view_commands.cpp](../examples/oui_code/view_commands.cpp) | View and Help menus: panels, splitting, Markdown preview, tabs, themes, help, about |
| [tools_commands.cpp](../examples/oui_code/tools_commands.cpp) | Tools menu: scripts, text commands, Interactive Script, Interactive UI, the terminal, templates |
| [run_script.cpp](../examples/oui_code/run_script.cpp) | Run Script and the output tab |
| [search_commands.cpp](../examples/oui_code/search_commands.cpp) | Find in Files and its results, the TODO list of the code, the task list, New Tab |
| [panel_commands.cpp](../examples/oui_code/panel_commands.cpp) | file operations in the project tree and the explorer |
| [comparison.cpp](../examples/oui_code/comparison.cpp) | Compare Files, Compare with Saved, the calculator tab |

## Documents, editors, tabs and groups

[editors.h](../examples/oui_code/editors.h) and [documents.h](../examples/oui_code/documents.h) are the core model.

- An `EditorGroup` is a `TabView` and a `std::vector<Document>` beside it, one per tab, in tab order. Code that adds,
  closes or moves a tab changes both; `move_tab`, `reorder_tab`, `drop_lone_empty_tab` and `settle_group` keep them in
  step.
- A `Document` is what OUI Code knows about a tab: the file's path, its `TextFormat` (encoding, line endings), the
  `FileStamp` to notice changes made outside, its language, and, for a tab that is not a plain document, what it is:
  a preview (`preview_of`, `reading`), the Find in Files results, the script output, an Interactive Script
  (`interactive`), an Interactive UI (`interactive_ui`), the calculator, a comparison. A terminal or a task list tab
  has a `Document` with none of those set; its content is told apart by its widget type.
- `document_editor(tabs, index)` gives the `TextEdit` a tab's document is in: the tab's content, or the code editor
  inside an interactive tab; none for a tab that holds no document. Commands that act on "the document" go through it,
  so they work in interactive tabs and do nothing in a terminal.
- `Editors` owns the groups, in a `std::list` so references stay valid, laid out in a tree of `SplitView`s.
  `split`, `remove`, `neighbour` and `move_line` change and walk the tree; `layout()` writes it as text such as
  `h0.5(0,v0.33(1,2))` for the session, and `restore_layout` reads it back. `active()` is the group the focus is in or
  was in last; commands act on it. `prepare_tabs(setup)` runs a setup on every group's `TabView`, those that exist
  and those a split makes later: that is how the `Workbench` wires tooltips, closing and dragging once.
- Opening a file goes through `open_file_at_path`, which shows the file's tab if it is open anywhere, and otherwise
  reads it with OUI's text file functions, keeping its encoding and line endings. `Workbench::open_from_panel` wraps it
  and adds the file to the recent ones.
- Saving is `save_document` in [documents.cpp](../examples/oui_code/documents.cpp): it asks before overwriting changes
  made outside, offers UTF-8 when the text no longer fits the file's encoding, and only marks the document saved when
  the file was written. `check_external_change` reloads or asks; `Workbench::watch_external_changes` calls it when a
  tab is shown and when the terminal gets the focus back, posted so no dialog opens inside the event that caused it.
- Split Editor (Ctrl+K) reads one more key: `split_choice_of` turns it into a `SplitChoice`, and
  `apply_split_choice` does it. Alt+K, "Move Around the Window", is a reserved command, so it works from a terminal tab,
  which keeps every other key for its programs.
- Languages: `Editors::set_languages` points at the languages the scripts defined, and `apply_languages` gives each
  document the highlighter, comment and brackets of its file's language, or of the one picked in View > Syntax
  (`Document::chosen_language`).

## Commands, menus and the palette

Every action is a command in the application's `CommandRegistry` (`m_application.commands()`): an id, a translated
title, a callback, and any number of shortcuts bound with `bind`. Registering an id again replaces its title and
callback and keeps its keys: `file.quit` is registered and bound to Ctrl+Q early in the constructor with an empty
callback, and registered again with its real work in `register_file_commands`.

The same registry feeds:

- **The menus.** `file_menu()`, `edit_menu()`, `view_menu()`, `tools_menu()` and `help_menu()` return lists of
  `MenuItem`: command ids as strings, separators, and submenus whose entries a function builds each time they open
  (recent files, View > Syntax, View > Theme, Tools > Text Commands). `fill_menus()` sets all five through
  `with_text_commands(menu, items)`, which adds after a menu's own entries the text commands whose `menuPath` starts
  with that menu: an entry of its own, or in a submenu named by the rest of the path, joined to one of OUI Code's own
  submenus when the name matches its title in English or as translated, made when none does. `fill_menus` runs again
  when the scripts are reloaded.
- **The palette** (Ctrl+Shift+P), OUI's `show_command_palette`, which lists every enabled command visible there.
- **The help**, whose last section, "All commands with keys", is read from the registry, so it never falls behind.
- **The status bar**, whose key hints are command ids, shown with their current shortcut.

A new command is a `register_command` in the `register_*_commands` of its menu, a `bind` for its keys, its id in the
menu's list, and its title in the Spanish catalog.

## Settings, the session and unsaved work

The `Settings` object made in `main.cpp` is shared by everything; each part reads and writes its own keys and saves
when it changes something. Beside the settings file live the user's scripts (`oui_code_scripts/`), the copy of unsaved
documents (`oui_code_recovery/`), and the task list when there is no project.

- [session.cpp](../examples/oui_code/session.cpp): recent files and folders, the project and the panels
  (`project_folder`, `explorer_folder`, their visibility), the window's place and size in a window, and
  `save_open_session`: the layout of the groups (`layout`), each group's tabs (`layout.group.N.files`), the one each
  shows, and the active group. Saved on quitting and when the window closes.
- [session_entry.cpp](../examples/oui_code/session_entry.cpp): how a tab that is not a file is written in that list:
  `kind:` and its parts, separated by tabs and escaped, for `tasks`, `calculator`, `interactive`, `preview`, `compare`,
  `compare-saved`, `terminal` and `interactive-ui`. `Workbench::restore_tab` reads them back.
- `Workbench::restore_session` brings back files and task lists first, then the tabs that refer to them (a preview,
  a comparison), then opens the files given on the command line over them.
- [recovery.cpp](../examples/oui_code/recovery.cpp): `UnsavedBackup` writes a copy of the modified documents every few
  seconds (10 by default, a setting), only when one changed since the last copy. It is also written when the terminal
  goes away or the system shuts OUI Code down (`Application::on_shutdown`), and on quitting with "keep unsaved
  changes" (`hot_exit`) on. `reopen_unsaved_documents` brings them back, still unsaved, on the next start, and says why
  when it was an emergency.
- [preferences.cpp](../examples/oui_code/preferences.cpp): Edit > Preferences writes each choice as it is made. The
  interface language takes effect at the next start, since `main.cpp` installs it before anything shows a text.
- `remember_dialog_places` keeps where each dialog was moved and how big it was made, as `window.<key>.*`.

## Scripting

OUI Code runs JavaScript in four places, each with an engine of its own (see [OUIScript](ouiscript.md) for the engine
and `SystemApi`):

| What | Class | File | Engine's life | What scripts see |
|------|-------|------|---------------|------------------|
| Languages and text commands | `Scripting` | [scripting.cpp](../examples/oui_code/scripting.cpp) | from load to the next reload | `oui.languages.register`, `oui.commands.register`, `oui.words` |
| Tools > Run Script | `ScriptRun` | [script_run.cpp](../examples/oui_code/script_run.cpp) | one run | `SystemApi`'s calls |
| Tools > Interactive Script | `LiveScript` | [live_script.cpp](../examples/oui_code/live_script.cpp) | one run, a new one at each change | `SystemApi`'s, read only |
| Tools > Interactive UI | `UiPreviewView` | [ui_preview.cpp](../examples/oui_code/ui_preview.cpp) | no script: a description built | (none) |

### Languages and text commands

`Workbench::load_scripts` makes a new `Scripting` and loads the built-in scripts (`built_in_scripts()`: `prelude.js`,
which defines `oui.words`, then one script per group of languages, then `commands.js`), then every `.js` file of the
user's scripts folder in name order. A script that fails stops there; what it registered before stays, and the
problems are shown together in one message (`report_script_problems`). Then it unregisters the commands of the last
load, registers each `TextCommand` as an application command whose callback runs it on the selection or the whole
document and pastes what it gives back, binds its keys, and hands the languages to `Editors`. Reloading
(`tools.reload_scripts`) does all of it again, and `fill_menus` after.

- `Languages::add` ([languages.cpp](../examples/oui_code/languages.cpp)) reads a language: id, name, extensions,
  whole file names, comment, brackets, suggestions, and `states`, which `highlighter_of` turns into a
  `RuleHighlighter`. `Languages::for_path` matches whole names before extensions.
- `Scripting::add_command` reads a command: a title, or one per language (`{en, es}`), keys parsed as a shortcut,
  and `menuPath`, whose first name must be `file`, `edit`, `view`, `tools` or `help`.
- These scripts have no `SystemApi`: they define, and their commands transform text. What they print with
  `console.log` is kept and shown by Tools > User Scripts > User Scripts Output.
- Tools > User Scripts > Open User Script creates `user.js` in the folder from `user_script_template()` the first
  time.

### Run Script

`Workbench::run_script` ([run_script.cpp](../examples/oui_code/run_script.cpp)) picks the script (kept in the
`run_script` setting; F5 runs it again as a dry run, Shift+F5 for real, Run Script... asks for another) and the
document: the current tab's, or, from the output tab, the one the last run was on. A `ScriptRun` loads it as a module
and finds which of `processFile(file, dryRun)`, `process(dryRun, folder)` and `processGeneratedCode(code, {language,
file})` it exports: exactly one, with `dryRun` declared. A dry run of `processFile` works on a copy of the document,
and what the script made of it opens in a new tab. The output goes to the output tab (`ScriptOutput`), which has a
button to stop the programs the script started; closing that tab drops the run. After a run for real,
`show_script_changes` reloads what changed in open documents and the panels.

`processGeneratedCode` is also what runs after an Interactive UI tab's Generate, through `run_generate_script`, with
the script chosen for that language (`ui_generate_script_javascript`, `ui_generate_script_cpp`). Tools > Script
Templates writes a starting script of each kind (`process_file_template` and the others, in script_run.cpp).

### Interactive Script

`open_interactive_script` and `start_interactive_script` (tools_commands.cpp) make a tab with an `InteractiveView`
([interactive_view.cpp](../examples/oui_code/interactive_view.cpp)): the script's code on the left, the result on the
right, the log under it, the error along the bottom. An `InteractiveSession` in the tab's `Document` connects the
code's and the input's changes to a 300 ms timer, which runs a new `LiveScript`. The input is another open document,
text pasted in the tab, or nothing.

`LiveScript::run` appends a footer to the script that finds `transform`, `processFile`, `process` or
`processGeneratedCode` whether exported or not, and calls the first that applies, as a dry run: `transform(text)` gives
the result; `processFile` works on a copy written to a temporary folder (`SystemApi::Hooks::writable`), which is read
back as the result; `process` only logs. Its `SystemApi` is `read_only`, its time limit is a second, and a run that
waits more than two seconds in all is ended. Only the last run's result is shown: starting a run lets the previous one
go. Apply puts the result in the input document.

### Interactive UI

`start_interactive_ui` makes a tab with a `UiPreviewView` ([ui_preview.cpp](../examples/oui_code/ui_preview.cpp)): a
`.oui` description on the left, built with `oui::build_ui` 300 ms after each change and shown on the right to try, the
error along the bottom. Generate writes code for it, an oui_run script or C++, from
[ui_skeleton.cpp](../examples/oui_code/ui_skeleton.cpp) (`script_skeleton`, `cpp_skeleton`), in a new tab, then runs
the language's `processGeneratedCode` script if one is chosen. The language chosen is kept in `ui_generate_language`.

### Suggestions

[completion.cpp](../examples/oui_code/completion.cpp) works out what the cursor is writing (`writing_at`), which call it
is inside (`call_at`), and what to suggest: words of the open documents, the language's suggestions, and in a JavaScript
document what `oui/oui.d.ts` declares, through `ScriptApi`. [suggester.cpp](../examples/oui_code/suggester.cpp) is the
list that follows the typing, and [signature_tip.cpp](../examples/oui_code/signature_tip.cpp) the call's signature over
the line. The `Workbench` gives the `Suggester` a function that returns its `Context` (the focused document, its
language, every open text) and calls `after_event` after each event, from its event observer (`watch_events`).

## Other tabs

- **Terminal** (`tools.terminal`, Ctrl+`): `Workbench::open_terminal` makes an `oui::TerminalView` running the user's
  shell in a pseudo-terminal, in the project's folder. The program's title becomes the tab's; when the shell ends,
  the tab closes, posted, as the terminal's own callback cannot destroy it. The tab finds itself again each time, as
  the user may have moved it to another group.
- **Comparison** ([comparison.cpp](../examples/oui_code/comparison.cpp)): an `oui::DiffView` of two
  `ComparedSide`s, each an open document followed as it changes or a file's text. Compared again 300 ms after a
  change; choosing a line goes to it in its document.
- **Markdown preview** (view_commands.cpp): a `MarkdownView` following a document. A link to another Markdown file is
  read in the same tab, with back and forward (`PreviewReading`); a web address goes to the browser.
- **Task list** ([task_list.cpp](../examples/oui_code/task_list.cpp)): `TasksView` over a Markdown file of `- [ ]`
  lines, `tasks.md` at the project's top, saved at every change. `view.code_todos` lists the code's TODO, FIXME, XXX
  and HACK comments as Find in Files lists matches.
- **Calculator** (comparison.cpp): OUI's `CalculatorView`, its entries kept in the session.
- **New Tab** (Ctrl+T, search_commands.cpp): a list of every kind of tab there is.

## Search and navigation

- The find bar ([find_bar.cpp](../examples/oui_code/find_bar.cpp)) sits under the documents and follows the tab shown;
  replace and go to line are dialogs ([dialogs.cpp](../examples/oui_code/dialogs.cpp)).
- Go to File (Ctrl+P) and Find in Files (Ctrl+Shift+F) take their files from `project_files`
  ([project_search.cpp](../examples/oui_code/project_search.cpp)), which leaves out what git ignores
  ([ignore_rules.cpp](../examples/oui_code/ignore_rules.cpp), read from the `.gitignore` files) or, outside a
  repository, hidden and build folders. `ProjectSearch` searches on other threads, the unsaved text of open documents
  in place of their files, and posts the matches back; a new search stops the one running. The results fill one tab
  (`holds_find_results`).
- Go Back and Go Forward ([navigation.cpp](../examples/oui_code/navigation.cpp)): `NavigationHistory::remember` runs
  before each event and records a jump when the cursor is in another document, or ten lines away, whatever moved it.
- The panels ([panels.cpp](../examples/oui_code/panels.cpp), panel_commands.cpp) are two `FolderView`s: the project
  tree, held inside the project, and the explorer, free to go anywhere; copying goes from one to the other.

## Help, dialogs and translations

- **Help** ([help.cpp](../examples/oui_code/help.cpp)): `help_markdown` takes help.md or help.es.md by the interface
  language, appends OUI's calculator help and the table of commands with keys, and `show_help` shows a tab per `##`
  section. F1 opens it at the section about the tab on show (`Workbench::help_topic`), found by its English title, so
  both files must keep their sections in the same order.
- **Dialogs**: OUI Code's own windows and panels are `.oui` descriptions in `examples/oui_code/ui` (find in files,
  preferences, replace, about, the find bar, the task list...), built by `build_oui_code_ui` and wired in C++ by their
  widgets' ids. Changing a dialog's layout is editing its `.oui`; see [user interface
  descriptions](oui_descriptions.md). The rest of [dialogs.cpp](../examples/oui_code/dialogs.cpp) wraps OUI's
  dialogs with OUI Code's options, each with a `placement_key` so it reopens where it was left.
- **Translations**: every text goes through `oui::translate(context, text)`, and `.oui` descriptions name their
  context with `context=`. The Spanish catalog is `examples/oui_code/translations/es.json`, embedded in the program; a
  test (`OuiCodesCatalogsCoverItsTexts`) fails when a text of the sources or the descriptions is missing from it, or
  when it has one no source uses. Script commands give their titles per language instead (`{en, es}`). See
  [translations](translations.md).

## Where it shows: terminal, window, console, desktop

The same sources build `oui_code` and, with `OLAGARRO_OUI_WINDOW=ON`, `oui_code_gui`, which links
`olagarro::OUIWindow` and defines `OLAGARRO_OUI_CODE_WINDOW`. Both use the settings named `oui_code`, so they share
them when they sit side by side. The window build goes to its window with `--window`, and without it when nothing
else says where and it was not started from a terminal (`oui::started_from_terminal`): from the desktop's menu. `OLAGARRO_OUI_DRM` adds `--drm` and `OLAGARRO_OUI_CODE_DRM`; on Linux, the build
with OUI's desktop adds `--desktop` (`OLAGARRO_OUI_CODE_DESKTOP`, and `OLAGARRO_OUI_CODE_DESKTOP_PIXELS` when the
desktop can draw in pixels).
See [building](building.md).

The `#ifdef`s are few and in few places: main.cpp (the options and backends), session.cpp (the window's geometry),
workbench.cpp and view_commands.cpp (the window's themes and zoom). Everything else is the same code, since OUI lays
out in cells in both and only the painter changes ([rendering](oui_rendering.md)). What differs at run time follows
`WorkbenchOptions`:

- `in_cells`: View > Theme lists the terminal themes, kept as `terminal_theme`.
- `window_screen`, set only in a desktop window: View > Theme lists the graphical themes (`window_theme`), the
  application's zoom commands are in the View menu and the zoom is kept as `window_zoom`, and the window's place and
  size are saved and given back through `window_geometry_from`.
- `legacy_render`: the Turbo theme with buttons in plain green, for the classic Windows console.

## Crashes

`main` calls `oui::install_crash_reporter` first, with the product name "OUI Code". On a crash signal it writes
`<executable>_crash.log` beside the executable, as `oui_code_crash.log` (the signal and, where the C library has
`backtrace`, a stack of offsets to resolve with `addr2line` against an unstripped build; on Windows, a minidump beside
it), runs the cleanups OUI's backends registered with `add_crash_cleanup` so the terminal or the console is usable
again, and lets the crash go on. What the user loses is bounded by the copy of unsaved documents, written every few
seconds, which the next start reopens.

## Tests

- `olagarro_oui_code_tests` (gtest, `tests/oui_code/*_test.cpp`) builds the parts that do not need the `Workbench`:
  completion, the find bar, ignore rules, scripting and the built-in scripts, `LiveScript`, `ScriptRun`, the task list,
  help, session entries, the UI preview and skeletons.
- The program's own: `--smoke-test`, `--stress-test`, `--input-test` and `--help`, as CTest cases.
- `tests/oui_code/end_to_end.js`, run by oui_run, drives OUI Code in a pseudo-terminal from a copy of its own, with
  keys, and reads the screen.

See [testing](testing.md). A change to OUI Code that adds a text needs its translation, one that adds a command needs
its menu entry and translation, and one that changes what scripts can call needs its `api` comment and
`oui_run tools/extract_api.js` run again ([OUIScript](ouiscript.md#the-api-comments-and-the-generated-reference)).
