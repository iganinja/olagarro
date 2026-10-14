# User interface descriptions

A `.oui` file describes a window or a part of one as text: what widgets there are, where, and how they start. The
code that uses it finds each widget by its id and says what it does. OUI Code's own dialogs are written this way,
and so can a C++ program's or an oui_run script's. This document covers the language, every widget and property it
takes, how C++ and scripts build one, how its texts are translated, how errors are reported, and OUI Code's
Interactive UI tab, which shows a description as it is written. The widgets themselves are in
[OUI for C++ programs](oui_layout.md).

- [Syntax](#syntax)
- [Widgets and their properties](#widgets-and-their-properties)
- [Ids, buddies and slots](#ids-buddies-and-slots)
- [Colours](#colours)
- [Building one from C++](#building-one-from-c)
- [Using one from a script](#using-one-from-a-script)
- [Translations](#translations)
- [Errors](#errors)
- [Interactive UI in OUI Code](#interactive-ui-in-oui-code)
- [For editors](#for-editors)

The parser is `parse_oui` in [declarative.cpp](../oui/src/declarative.cpp); what builds the widgets is `build_ui` in
[ui.cpp](../oui/src/ui.cpp), and [ui.h](../oui/include/oui/ui.h) summarises the language at its top.

## Syntax

```
# Edit > Find in Files: what to look for, where, and how.
Window "Find in Files" context=FindInFiles width=60
  Label "&Find:" buddy=query
  LineEdit id=query placeholder="Text to find"
  Label "&In files:" buddy=files
  LineEdit id=files placeholder="Every file, or patterns such as *.cpp *.h"
  CheckButton "&Whole word" id=whole_word
  Row centered gap=tight
    Button "Find" id=find default
    Button "Cancel" id=cancel
```

- **A widget a line.** A line is the widget's type, its text in double quotes when it has one, and its properties,
  each separated from the next by spaces.
- **Indentation.** What goes inside a widget is indented under it, with spaces; how many does not matter, only that
  a line is indented further than the widget it belongs to. A line indented as far as an earlier one, or less, is
  that one's sibling or belongs further out. A tab in the indentation is an error. There is one root widget.
- **Types** start with a capital letter and go on with letters and digits: `Window`, `LineEdit`.
- **The text** goes right after the type, in double quotes: `Button "Find"`. Inside it, `\"` writes a quote, `\\` a
  backslash and `\n` a line break.
- **Properties** are `name=value`, or a name alone for one that is on: `default`, `hidden`. A name starts with a
  lowercase letter or `_` and goes on with lowercase letters, digits, `_` and `-`. A value runs to the next space, or
  is in double quotes when it has spaces, with the same escapes as the text. A property given twice is an error.
- **Comments** are lines whose first character after the indentation is `#`. A `#` later in a line is not a comment.
  Blank lines are skipped, and Windows line ends are read as well.
- **Mnemonics.** In a text, `&` marks the letter that, with Alt, presses a button, turns a check box or goes to the
  field a label names, and inside a dialog the letter alone does the same when the focused widget does not take it.
  `&&` writes an ampersand.

## Widgets and their properties

| Widget | Text | Properties | Holds |
|--------|------|------------|-------|
| `Window` | its title | `width`, `max_width`, `min_width`, `min_height`, `height_share`, `resizable`, `border`, `placement`, `keep_on_escape` | widgets: one alone fills it, several go one under the other |
| `Column` | | `gap` | widgets, one under the other |
| `Row` | | `gap`, `centered` | widgets, side by side |
| `Grid` | | `columns`, `rows`, `gap` | widgets, a row at a time |
| `Split` | | `vertical`, `ratio` | two widgets |
| `Dock` | | | nothing: the code fills it |
| `Tabs` | | `fixed` | `Tab`s |
| `Tab` | its title | | one widget, or none |
| `Label` | what it says | `buddy`, `bold`, `heading`, `wrap`, `background` | |
| `LineEdit` | | `text`, `placeholder`, `background`, `password` | |
| `ProgressBar` | | `value`, `text` | |
| `TextEdit` | | `read_only`, `wrap` | |
| `MarkdownView` | | `background`, `foreground` | |
| `CheckButton` | what it says | `checked`, `background` | |
| `Button` | what it says | `default`, `compact`, `flat` | |
| `ComboBox` | | | |
| `VSpacer` | | `height` | |
| `HSpacer` | | `width` | |
| `Slot` | | | nothing: the code puts a widget there |

Every widget also takes `id=name`, how the code finds it; `hidden`, so it starts hidden; and `tooltip="..."`, what a
pointer resting on it is told. A widget with a text takes `untranslated`, and the root takes `context=Name` (see
[Translations](#translations)).

What each builds, and what its properties do, as `Builder::make` in ui.cpp does it:

| Widget | Builds | Properties |
|--------|--------|------------|
| `Window` | `Window` | `width=cells` the width it opens at even when its content is narrower (`set_preferred_size`); `max_width=cells`, `min_width=cells`, `min_height=rows`; `height_share=0.9` the share of the screen's height it opens at; `resizable` the mouse resizes it; `border=none\|single\|double`, double unless given; `placement=key` where the person left it is remembered under the key; `keep_on_escape` Escape does not close it |
| `Column`, `Row` | `Column`, `Row` | `gap=none\|tight\|normal\|loose`, none unless given; a row's `centered` puts its widgets in the middle |
| `Grid` | `Grid` | `columns=content,fill,12` and `rows=content,flex`, each `content`, `fill`, `flex` or a number of cells; one `fill` column unless given. In rows only `flex` takes the room left; the others are as tall as their content |
| `Split` | `SplitView` | `vertical` one over the other rather than side by side; `ratio=0.4` the first one's share, between 0 and 1 |
| `Dock` | `DockLayout` | none; the code adds its widgets, saying where each goes |
| `Tabs` | `TabView` | `fixed` its tabs cannot be closed |
| `Tab "title"` | a page of the `Tabs` | a tab with no widget shows nothing under it: a choice the code follows, as over a list it fills |
| `Label "text"` | `Label` | `buddy=id` the widget its letter goes to; `bold`; `heading` bold in the dialog's heading colour; `wrap` the text breaks into lines; `background=colour` |
| `LineEdit` | `LineEdit` | `text="..."` what it starts with; `placeholder="..."` what it says while empty; `background=colour`; `password` a dot shows for each character typed |
| `ProgressBar` | `ProgressBar` | `value=0.25` how much is done, from 0 to 1; `text="..."` what it says after the bar |
| `TextEdit` | `TextEdit` | `read_only`; `wrap` long lines break |
| `MarkdownView` | `MarkdownView` | `background=colour`, `foreground=colour` |
| `CheckButton "text"` | `CheckButton` | `checked`; `background=colour` |
| `Button "text"` | `Button` | `default` Enter presses it; `compact` on one row; `flat` without its shadow |
| `ComboBox` | `ComboBox` | none; its options are the code's to give |
| `VSpacer`, `HSpacer` | `VSpacer`, `HSpacer` | `height=rows`, `width=cells`, 1 unless given |
| `Slot` | a placeholder that takes no room | needs an `id`; cannot be the root |

Numbers are whole and not negative. Shares, `ratio` and `height_share`, are written `0`, `1`, `0.4` or `1.0`, read by
ui.cpp itself rather than the C library, whose decimal point is the locale's. A `Grid` column of a percentage, which
C++ can give, has no spelling in a description.

How these widgets lay out, and why a `TextEdit` goes in a `flex` row rather than straight in a column, is in
[measure and arrange](oui_layout.md#measure-and-arrange). A pattern that comes back in OUI Code's descriptions is a
grid of one column whose rows are `content,flex`: a title over something that takes the rest.

```
# The interactive script's result, under a title with its buttons at the right.
Grid rows=content,flex
  Grid columns=fill,content,content gap=tight
    Label "Result" id=result_title bold
    Button "Apply" id=apply compact flat hidden
    Button "Log" id=show_log compact flat
  TextEdit id=result read_only
```

## Ids, buddies and slots

**Ids.** `id=name` names a widget for the code. An id is given once in a description; the same id twice is an error.
On a `Tab`, the id names the page's widget, the one under the tab.

**Buddies.** `buddy=id` on a label names the widget its `&` letter goes to: Alt and the letter, or the letter alone
in a dialog, give that widget the focus. The buddy may be written before or after the label, as buddies are named
once every widget is made. A label with no buddy has nowhere to send its letter, so its `&` is taken out of the text
it shows, and `&&` shows as one `&`.

```
Grid columns=content,1,fill
  Label "&Name:" buddy=name
  HSpacer
  LineEdit id=name
```

**Slots.** A description writes only the widgets in the table above. A `Slot` with an id holds the place of any other
widget, a `Table`, a `TreeView`, a program's own, which the code puts there; until then it takes no room.

**Docks.** A `Dock` is written empty: a `DockLayout` needs to know where each widget goes, so the code adds them.

## Colours

`background=` and `foreground=` take one of the theme's colours, by its name in `Theme`
([theme.h](../oui/include/oui/theme.h)): `background=table_background`, `foreground=reading_foreground`. Every
colour of the theme has its name there, from `background` and `text` to `syntax_keyword` and `warning_foreground`.
The widget keeps the reference, not the colour, so it follows the application when it changes its theme. Fixed
colours, as `#20a0ff`, are for code, not descriptions. `named_theme_color(name)` finds a colour by the same names, for
code that reads them from elsewhere. [Themes](themes.md) lists what each colour is for.

## Building one from C++

`build_ui(description)` builds what a description says and returns a `Ui`, which holds the root and finds the widgets
by their ids:

```cpp
auto ui{oui::build_ui(description)};
ui.get<oui::Button>("find").on_clicked([&] { find(ui.get<oui::LineEdit>("query").text()); });
static_cast<void>(application.show_modal(ui.take_window()));
```

| `Ui` | Does |
|------|------|
| `get<Kind>(id)` | The widget with the id, as what it is. An id that is not there, or a widget that is not a `Kind`, is a mistake in the code, and throws `std::logic_error` |
| `root()` | The root widget |
| `take_window()` | The root, a `Window`, to show; nothing when the root is another widget |
| `take()` | The root, whatever it is, to put somewhere: in a window, a tab, a dock |
| `put(id, std::unique_ptr<Kind>)` | Puts the widget in the `Slot` with the id, and returns it as a `Kind&`; `get` then finds it by that id. A widget that is not a `Slot` throws |
| `is_slot(id)` | Whether the id is a `Slot` still empty |
| `take(id)` | The widget with the id, out of the description, for the code to put somewhere of its own; `get` still finds it |
| `widgets()` | Every widget with an id, in the order written |

The `Ui` holds pointers to the widgets; once the root is taken it does not own them. `get` is good for as long as the
widgets are, so the code wires them before showing the window, and reads them in its callbacks while it shows:
`show_modal` destroys the window as it returns (see [dialogs](oui_layout.md#dialogs)).

OUI Code's Interactive UI tab is itself a description with slots, filled in `UiPreviewView`'s constructor
([ui_preview.cpp](https://github.com/iganinja/oui_code/blob/master/oui_code/ui_preview.cpp)):

```cpp
auto ui{build_oui_code_ui(OuiCodeUi::interactive_ui)};
m_code = &ui.put("code", std::move(code));
m_area = &ui.put("preview", std::make_unique<PreviewArea>());
m_language = &ui.get<oui::ComboBox>("language");
m_language->set_options({"JavaScript", "C++"});
add(ui.take());
```

**Building it into the program.** A description can be read from disk as the program runs, with `oui::read_file`, but
OUI's programs build theirs in, so the binary carries them. `olagarro_embed_files`
([OlagarroEmbedFiles.cmake](../cmake/OlagarroEmbedFiles.cmake)) compresses files into a target and defines a function
that returns their contents, in the order given:

```cmake
olagarro_embed_files(my_program FUNCTION embedded_my_program_ui
    FILES ui/settings.oui ui/about.oui)
```

```cpp
// Defined in the source CMake generates, in the order the files are given.
std::vector<std::string_view> embedded_my_program_ui();

auto ui{oui::build_ui(embedded_my_program_ui().at(0))};
```

A file comes back whole, uncompressed the first time the function is called; editing it rebuilds the source on the next
build. OUI Code names its descriptions with an enum in the same order, `OuiCodeUi` in
[ui_descriptions.h](https://github.com/iganinja/oui_code/blob/master/oui_code/ui_descriptions.h), and builds one with
`build_oui_code_ui(which)`.

The Greeter of [OUI for C++ programs](oui_layout.md#a-complete-program), described:

```
Window "Greeter" context=Greeter
  Grid columns=content,1,fill
    Label "&Name:" buddy=name
    HSpacer
    LineEdit id=name placeholder="Who to greet"
  Label id=greeting
  Row centered gap=tight
    Button "&Greet" id=greet default
    Button "Close" id=close
```

```cpp
auto ui{oui::build_ui(embedded_greeter_ui().front())};
auto& name{ui.get<oui::LineEdit>("name")};
auto& greeting{ui.get<oui::Label>("greeting")};
auto& greet{ui.get<oui::Button>("greet")};
greet.on_clicked([&] { greeting.set_text("Hello, " + std::string{name.text()} + "!"); });
name.on_submitted([&greet](std::string_view) { greet.click(); });
auto window{ui.take_window()};
auto& window_reference{*window};
ui.get<oui::Button>("close").on_clicked([&] { window_reference.close(oui::DialogResult::accepted); });
application.set_window(std::move(window));
application.focus(name);
```

## Using one from a script

oui_run builds descriptions too
([scripting](https://github.com/iganinja/oui_code/blob/master/doc/scripting.md#described-in-a-oui-file)). `oui.ui(text)`
takes one in a string, and `oui.uiFromFile(path)` reads one, a relative path found from the script's folder wherever the
script is run from. Each widget with an id is a property of the result, as the script object of its kind; `ui.root` is
the root, and `ui.put(id, widget)` fills a slot. An id cannot be `root` or `put`.

```js
const ui = oui.uiFromFile("ui/ask.oui");
ui.go.onClick = () => { ui.greeting.text = `Hello, ${ui.name.text}!`; };
ui.close.onClick = () => oui.quit();
oui.window(ui);
```

`oui.window(ui)` opens a description whose root is a `Window`, with its title, and on the desktop as wide as its
`width=` says; `new oui.Dialog(ui)` shows it as a dialog. A script's descriptions are built with `UiTexts::as_written`:
their texts show as written, never translated, and need no `context`. A description that is wrong throws in the script,
with the file's name, the line and the column. [The API
reference](https://github.com/iganinja/oui_code/blob/master/doc/oui_api.md#ouiui) has the details.

## Translations

`build_ui(description, UiTexts::translated)`, the default, translates a description's texts as it builds it:
`translate(context, text)` with the context the root names in `context=`. These are translated:

- the text in quotes of a `Window`, `Tab`, `Label`, `CheckButton` and `Button`, unless the widget says
  `untranslated`;
- the values of `text=`, `placeholder=` and `tooltip=`.

`untranslated` is for a text that shows as it is written in every language, as a program's name or an arrow on a
button. A description with texts to translate and no `context=` does not build: it is an error, as its texts would
have nowhere to be looked up. `context=` goes on the root only.

`UiTexts::as_written` shows every text as written and needs no context, as scripts' descriptions do.

The catalogs are JSON, under the same contexts as the code's texts ([translations](translations.md)). A text marks its
mnemonic in the catalog too, `"&Find:": "&Buscar:"`, so each language picks its own letter. `ui_texts(root)` lists a
description's texts with their context and place, for a check that the catalogs cover them: OUI Code's translation
test reads every `.oui` file that names a context and fails when a catalog lacks one of its texts. Translation happens
when the description is built, so the language is installed first, as for every other text.

## Errors

A description that is wrong does not build: `build_ui` throws `UiError` ([ui.h](../oui/include/oui/ui.h)), whose
`line()` and `column()` say where, from 1, `message()` what, and `what()` all three: `2:3: There is no widget called
Slider.` A mistake in the code that uses a description, an id it does not have or a kind it is not, is a
`std::logic_error` instead.

What the parser reports, `parse_oui` in [declarative.cpp](../oui/src/declarative.cpp):

| Message | When |
|---------|------|
| Expected a widget type, as Label or Button. | A line that does not start with a capital |
| Expected a space here. | Something stuck to what is before it |
| The text goes right after the type. | A quoted text after a property |
| Expected a property, as name=value or a name alone. | A word that is not a property name |
| The property *name* is given twice. | |
| A quote is not closed. | |
| Tabs are not allowed for indentation. | |
| A document can only have one root widget. | A second line with no indentation |
| Expected a root widget. | Nothing but comments |

The parser goes on after an error and gathers every one in `ParseResult::diagnostics`; `build_ui` reports the first.
What building reports, from `Builder` in [ui.cpp](../oui/src/ui.cpp):

| Message | When |
|---------|------|
| There is no widget called *type*. | |
| A *type* has no property *name*. | |
| A *type* has no text of its own. | A quoted text on a widget that shows none |
| A *type* holds no other widgets. | A widget indented under one that holds none |
| Only the root names a context. | |
| Texts to translate need context=, the place they belong to. | Translated texts and no context |
| The id *id* is given twice. | |
| No widget has the id *id*. | A buddy that names nothing |
| A gap is none, tight, normal or loose, not *value*. | |
| A border is none, single or double, not *value*. | |
| The *name* is a whole number, not *value*. | |
| The *name* is a share between 0 and 1, as 0.4, not *value*. | |
| A column is content, fill, flex or a number of cells, not *value*. | And the same for a row |
| There is no colour *name* in the theme. | |
| A Split holds two widgets. | |
| A Dock is filled by the code, which says where each widget goes. | |
| A Tab goes in a Tabs. / A Tabs holds Tab, not *type*. / A Tab holds one widget. | |
| A Slot goes inside another widget. / A Slot needs an id, for the code to fill it. | |

The column points at the widget, or at the property that is wrong.

## Interactive UI in OUI Code

**Tools > Interactive UI** in OUI Code opens a tab with a description on the left and what it builds on the right, built
again 300 ms after the last change, to try as it would be used: type in its fields, tick its boxes, press its buttons.
On a `.oui` document it works on that file, in the one tab there is for it; from anything else it starts a new
description from an example, `interactive_ui_template` in
[ui_preview.cpp](https://github.com/iganinja/oui_code/blob/master/oui_code/ui_preview.cpp). The tab is `UiPreviewView`,
opened by `Workbench::start_interactive_ui` in
[tools_commands.cpp](https://github.com/iganinja/oui_code/blob/master/oui_code/tools_commands.cpp).

- **Building.** `UiPreviewView::show` builds the description translated when its root names a context, as a
  program's would be, and as written when not, as a script's. Each `Slot` shows as a label with its id in brackets,
  where the code would put its widget. The preview is drawn on the desktop's colour, a window centred in it.
- **Errors.** When it does not build, a line along the bottom says `Line 3, column 7: ...`, and a click on it takes
  the cursor to that line. What built last stays on the right until the description builds again.
- **Suggestions** come from `ui_suggestions` (see [For editors](#for-editors)) as the description is written.

**Generate**, over the preview, writes code to start from in a new tab, in the language chosen beside it, which is
remembered: `script_skeleton` or `cpp_skeleton` in
[ui_skeleton.cpp](https://github.com/iganinja/oui_code/blob/master/oui_code/ui_skeleton.cpp). Both load the description
by its file's name, so one with no file yet is saved first; one that does not read gives no code. Each widget with an id
is wired to what it is usually wanted for:

| Widget | JavaScript | C++ |
|--------|------------|-----|
| `Button` | `ui.go.onClick = () => {}` | `on_clicked` |
| `LineEdit` | `onChange = (text) => {}` | `on_changed` with the text |
| `TextEdit` | `onChange = (text) => {}` | `on_changed` |
| `CheckButton` | `onToggle = (checked) => {}` | `on_toggled` |
| `ComboBox` | `items = []` and `onSelect` | `set_options({})` and `on_selected` |
| `Tabs` | `onSelect` | `on_selection_changed` |
| `MarkdownView` | `markdown = ""` | `set_markdown("")` |
| `Label` | its text, commented out | `set_text`, commented out |

The JavaScript loads the file with `oui.uiFromFile` and opens it with `oui.window(ui)`, or, for a root that is not a
`Window`, `oui.window("Window", ui.root)`. An id that is not a JavaScript name is written `ui["save-as"]`.

The C++ is a function named after the file, `show_rename_files` for `rename_files.oui`, which builds the window from
`embedded_rename_files_ui()`, wires it and shows it with `show_modal`; a comment over it gives the
`olagarro_embed_files` line that defines that function, and one under it a version that reads the file from disk
instead. Ids become variables, with `_` for what a C++ name cannot have and after a keyword: `case_` for `case`.
`tests/oui_code/generated_window.cpp`, in OUI Code's repository, is what comes out for
`tests/oui_code/rename_files.oui`, and it is compiled with the tests, so what Generate writes builds. The generated
function builds with `UiTexts::translated`, the default, so a description with texts needs `context=` on its root for it
to build at run time.

**After Generate.** Beside Generate, a script can be chosen to run after it, one for each language, remembered in OUI
Code's settings: a module that exports `processGeneratedCode(code, {language, file, dryRun})`, `language` being
`"javascript"` or `"cpp"` and `file` the description's whole path. It gets what Generate wrote, to save it where the
project wants it or build with it, and what it returns shows at the end of its output. **New...** in the same list
writes one in an Interactive Script tab, where it runs as a dry run on the code Generate would write now; saved, it is
the script for that language. OUI Code's help has the rest, and [OUI Code's
architecture](https://github.com/iganinja/oui_code/blob/master/doc/oui_code.md) how its tabs work.

## For editors

Two functions serve an editor of descriptions, as OUI Code's:

- `parse_oui(text)` reads one without building it: the tree of `DeclarativeNode`s, each with its type, its text, its
  properties in order and the line and column it was written at, and every problem found, in `diagnostics`.
- `ui_suggestions(description, cursor)` says what fits at the cursor and where what it replaces starts: at a line's
  start the widgets; after a widget and a space, the properties it takes and has not been given, its own first; after
  `name=`, its values: the choices of `gap` or `border`, the lengths of a grid, the theme's colours, or the ids the
  description gives, for `buddy`. Each suggestion has its text with `$0` where the cursor goes after, as
  `placeholder="$0"`. Nothing is suggested inside a text or a comment.
