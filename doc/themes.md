# Themes

A theme is the set of colours OUI paints with: every widget takes its colours from it, so the same program reads as
Turbo C++ in blue, as a dark terminal, as a light one, or as a bevelled desktop window. This document describes the
`Theme` struct, how colours reach widgets, the themes OUI carries, how a program chooses one, and how to add a new one,
with a whole worked example.

- [What a theme is](#what-a-theme-is)
- [The Theme struct](#the-theme-struct)
- [ColorRef: colours resolved when painting](#colorref-colours-resolved-when-painting)
- [The built-in themes](#the-built-in-themes)
- [Setting a theme from C++](#setting-a-theme-from-c)
- [Terminal themes and the sixteen colours](#terminal-themes-and-the-sixteen-colours)
- [Adding a terminal theme](#adding-a-terminal-theme)
- [Worked example: the amber theme](#worked-example-the-amber-theme)
- [Graphical themes](#graphical-themes)
- [Adding a colour to Theme](#adding-a-colour-to-theme)
- [Colours by name: descriptions and scripts](#colours-by-name-descriptions-and-scripts)
- [Themes in oui_run scripts](#themes-in-oui_run-scripts)
- [Themes in OUI Code](#themes-in-oui-code)

How the colours become characters or pixels is in [rendering](oui_rendering.md).

## What a theme is

`olagarro::oui::Theme` ([theme.h](../oui/include/oui/theme.h)) is a plain struct of named `Color` fields, each a
24-bit RGB value ([color.h](../oui/include/oui/color.h)). It says nothing about shapes or characters: a terminal
paints cells in those colours, and the [painter](oui_rendering.md#the-painter) decides what a button or a frame looks
like.

The application owns one theme, `Application::theme()`, and hands it to every `Widget::paint(Painter&, const Theme&)`
on every frame. Changing it repaints everything on the next frame, with no widget to rebuild.

In pixels there is a second layer. A `GraphicalStyle`
([graphical_painter.h](../oui/include/oui/graphics/graphical_painter.h)) holds the shades of the bevelled shapes (faces,
edges, title bars, wells, the focus ring), which the graphical painter draws by itself; the `Theme` still colours what
widgets paint inside them: text, editors, lists, selections, syntax. A graphical look is therefore a style plus the
theme made from it, `theme_for(style)`. See [graphical themes](#graphical-themes).

## The Theme struct

Every field has a default; together the defaults are a Turbo Vision palette close to `TurboCTheme`, and
`Application` uses them when it is given no theme. The fields, by what they colour:

| Group | Fields | What they colour |
|-------|--------|------------------|
| Desktop and surfaces | `background`, `text`, `muted_text`, `control_background`, `panel_background`, `panel_foreground`, `side_panel_background` | The desktop behind windows and its text; quieter text, as a menu entry's shortcut; panels and their lists; panels beside the main content, as a project tree |
| Shadows | `shadow`, `shadow_foreground` | Turbo Vision shadows: the cells under one keep their characters, in `shadow_foreground` on `shadow` |
| Focus | `focus_background`, `focus_text` | What has the keyboard focus, where a widget marks it with a colour |
| Dialogs and windows | `dialog_background`, `dialog_foreground`, `dialog_border`, `dialog_heading_foreground`, `dialog_icon_foreground`, `dialog_border_placing` | A window's body and text, its frame, a heading over a group of controls, the marks on the frame (close, maximize), the frame while it is moved or resized from the keyboard |
| Tables | `table_header_background`, `table_header_foreground`, `table_background`, `table_foreground` | Column headers and rows |
| Lists | `list_background`, `list_foreground`, `item_selected_background`, `item_selected_foreground` | A list to pick from, and the item chosen in it or in a field of text (not the selection in an editor) |
| Clusters | `cluster_background`, `cluster_foreground`, `cluster_focused_foreground`, `cluster_shortcut_foreground` | Check boxes and radio buttons, the one with the focus, and the letter that turns one with Alt |
| Menus and status line | `menu_background`, `menu_foreground`, `menu_border`, `menu_selected_background`, `menu_selected_foreground`, `status_background`, `status_foreground` | The menu bar, drop-down menus, the entry under the cursor, the status bar |
| Buttons | `button_background`, `button_foreground`, `button_default_foreground`, `button_shortcut_foreground`, `button_focused_background`, `button_focused_foreground`, `button_pressed_background`, `button_pressed_foreground` | A button at rest, the default button's text (the one Enter presses from anywhere in its dialog), its Alt letter, focused, held down |
| Labels | `label_shortcut_foreground` | The Alt letter of a label that goes to the field it names |
| Tabs | `tab_active_background`, `tab_active_foreground`, `tab_inactive_background`, `tab_inactive_foreground` | The selected tab and the others |
| Selection and search | `selection_background`, `selection_foreground`, `search_match_background`, `search_match_foreground`, `occurrence_background`, `occurrence_foreground`, `bracket_match_background`, `bracket_match_foreground` | Selected text; every match of a search; the other places with the selected text; the bracket by the cursor and its pair |
| Comparison | `diff_removed_background`, `diff_removed_foreground`, `diff_added_background`, `diff_added_foreground`, `diff_changed_background`, `diff_changed_foreground`, `diff_filler_background` | Two texts side by side: lines only on the left, only on the right, changed lines, and the room facing a line the other side lacks |
| Scrollbars | `scrollbar_background`, `scrollbar_foreground` | The track and the thumb, where a widget passes these |
| Editor | `editor_background`, `editor_foreground`, `line_number_foreground` | A text editor and its line numbers |
| Reading | `reading_background`, `reading_foreground` | Text to read rather than edit, as help, set apart from documents |
| Syntax | `syntax_keyword`, `syntax_type`, `syntax_function`, `syntax_string`, `syntax_number`, `syntax_comment`, `syntax_preprocessor`, `syntax_operator` | Source text by kind, as a syntax highlighter tells it, on `editor_background` |
| Formatted text | `heading_foreground`, `code_foreground`, `code_background`, `link_foreground` | Markdown headings, code and links |
| Input | `input_background`, `input_foreground` | A one-line text field |
| States and messages | `disabled_foreground`, `hotkey_foreground`, `error_foreground`, `warning_foreground`, `info_foreground` | Disabled entries, the underlined letter of a menu entry, and messages by severity |

The comments beside each field in [theme.h](../oui/include/oui/theme.h) say the same with more detail. A field's
name is also its name for [descriptions and scripts](#colours-by-name-descriptions-and-scripts).

## ColorRef: colours resolved when painting

A widget that lets the program choose a colour, as `Label::set_color`, `Label::set_background`,
`Button::set_background` or `Window::set_background`, takes a `ColorRef`, which is either a fixed colour or a
pointer to one of the theme's fields:

```cpp
label.set_color(&oui::Theme::error_foreground);   // the theme's, whichever theme is set when it paints
label.set_background({0, 85, 85});                 // fixed: the same in every theme
panel->set_colors(&oui::Theme::side_panel_background, &oui::Theme::panel_foreground);
```

The widget keeps the `ColorRef`, not the colour, and calls `resolve(theme)` in `paint()`. That is why a theme
change needs nothing from the widgets: a label set to `error_foreground` is red in Turbo C++ and white in Monochrome,
with no code to run when the user switches. Had the widget copied the colour when it was set, every widget would have
to be told of each change.

Widgets that hold an optional colour use the free function `resolve(optional, theme, fallback)`: the colour given,
or `fallback` from the theme when none was:

```cpp
// Button::paint, in oui/src/button.cpp
resolve(m_background, theme, theme.dialog_background)
```

## The built-in themes

| Function | Where | Look |
|----------|-------|------|
| `TurboCTheme::make()` | [turbo_c_theme.h](../oui/include/oui/turbo_c_theme.h) | Turbo C++ / Borland C++ / Turbo Pascal: Turbo Vision's `cpAppColor` palette and their editor's yellow on blue. The comments name the palette entry each colour comes from. |
| `terminal_themes()` | [terminal_themes.cpp](../oui/src/terminal_themes.cpp) | The looks for characters: `turbo` (the one above), `night`, `light`, `monochrome`, `amber`, `phosphor`, `commander` |
| `classic_theme()` | [graphical_painter.cpp](../oui/src/graphics/graphical_painter.cpp) | The graphical look, `theme_for(classic_style())`: grey bevelled surfaces, a green title, dark wells, inspired by Transport Tycoon |
| `dark_theme()` | the same | `theme_for(dark_style())`: dark greys, flat shapes with thin edges, Noto Sans, one blue accent; for programs that show pictures, as ouivi |
| `graphical_themes()` | the same | The graphical looks: `classic`, `ocean`, `night`, `dark` |

`terminal_themes()` returns `TerminalTheme` values: an `id`, which is the same in every language and is what
settings keep; a `name`, translated, for a menu; and the `Theme`. The default comes first.

- **Turbo C++** (`turbo`): blue desktop and editor, grey dialogs, cyan lists, green buttons.
- **Night** (`night`): grey surfaces on black and a black editor, for a dark terminal.
- **Light** (`light`): black text on white and grey, for a light terminal or a bright room.
- **Monochrome** (`monochrome`): greys only, what is chosen or focused shown the other way round, for a terminal of
  few colours or eyes that tell colours apart badly.
- **Amber** (`amber`): yellow and brown on black, as an amber monitor; warmer than Night.
- **Phosphor** (`phosphor`): light green and green on black, as a green phosphor monitor.
- **Commander** (`commander`): light cyan on blue panels, cyan bars, grey dialogs and yellow where something stands
  out, as Norton Commander and Midnight Commander.

Every look but Turbo starts from `TurboCTheme::make()` and then sets every field, so a field added to `Theme` later has
a sensible value in them before anyone revisits them.

## Setting a theme from C++

`Application::set_theme(Theme)` replaces the theme and asks for a frame; the constructor takes one too.

```cpp
oui::Application application{oui::terminal_backend(), oui::TurboCTheme::make()};

// Later, as a menu entry would:
for(const auto& theme : oui::terminal_themes())
{
    if(theme.id == "night")
    {
        application.set_theme(theme.theme);
    }
}
```

The theme fits what shows the program, so programs choose it with the backend: OUI Code, oui_calc and oui_term take
`TurboCTheme::make()` in a terminal and `classic_theme()` in a window, on the whole screen or on a desktop of pixels
(see `main.cpp` in `examples/oui_term`, and in `oui_code` and `oui_calc` in [OUI Code's
repository](https://github.com/iganinja/oui_code)).

A `Theme` is a value: build one, change a field, set it. A program that wants a terminal theme with one difference
copies it and changes the field before `set_theme`, as OUI Code does under the legacy Windows console
(`Workbench::use_terminal_theme` in
[workbench.cpp](https://github.com/iganinja/oui_code/blob/master/oui_code/workbench.cpp)):

```cpp
if(m_options.legacy_render)
{
    // Under the classic console, buttons in plain green.
    theme.button_background = {0, 255, 0};
}
m_application.set_theme(theme);
```

## Terminal themes and the sixteen colours

The terminal themes use only the sixteen colours of a text console, as EGA and VGA showed them; `terminal_themes.cpp`
names them at its top (`black`, `blue`, ... `yellow`, `white`) and `TurboCTheme` has the same as static members.
Keeping to them is what makes a theme read the same everywhere, because a terminal does not always show the RGB
values OUI sends:

- A terminal that says it takes 24-bit colour (`COLORTERM` set, or `TERM` naming a 256-colour terminal) gets the
  exact RGB of each cell.
- Any other gets the nearest of the sixteen ANSI colours: each channel on or off at half its range, bright when the
  strongest channel is over 191.
- The Linux console shows sixteen foregrounds but only the eight dark backgrounds: a bright background shows as its
  dark version, and a white one as cyan, so what is marked with white does not vanish into light grey.
- The legacy Windows console (without virtual terminal sequences) takes its own four-bit attributes.

A colour from the sixteen survives all of these unchanged, or as its own console colour. A colour between them may
land on an unexpected one, or on the same as its neighbour: a theme meant for terminals should keep to the sixteen.
[Rendering](oui_rendering.md#colours-in-a-terminal) has the details. To see a program as it looks on the Linux
console from any terminal, run it with `OUI_LINUX_CONSOLE=1`.

## Adding a terminal theme

A new look for characters is a function in [terminal_themes.cpp](../oui/src/terminal_themes.cpp) and one entry in a
list; everything that offers themes reads that list.

1. **Write the theme.** In the anonymous namespace of `oui/src/terminal_themes.cpp`, beside `night()`, `light()` and
   `monochrome()`, add a function that starts from `TurboCTheme::make()` and sets every field from the sixteen
   colours, with a comment saying what the look is for. Setting every field, rather than only those that differ,
   makes the theme whole on its own: reading it tells all its colours.
2. **List it.** Add it to the vector `terminal_themes()` returns, with an id and a name:

   ```cpp
   {"amber", translate("TerminalTheme", "Amber"), amber()},
   ```

   The id is lower case, one word, and never changes once released: OUI Code keeps it in its settings and scripts
   name it in `oui.setTheme(...)`. The name goes through `translate` with string literals and the context
   `TerminalTheme`, so the [catalog check](translations.md#tests-that-check-catalogs) can find it. Keep `turbo` first:
   it is the default, and a test checks it.
3. **Translate the name.** Add it under `"TerminalTheme"` in every catalog of OUI's, today
   [oui/translations/es.json](../oui/translations/es.json):

   ```json
   "TerminalTheme": {
     "Amber": "Ámbar",
     "Light": "Claro",
     ...
   ```

   The test `OuiTranslationTests.OuisCatalogsCoverItsTexts` fails while any catalog lacks it.
4. **Name it for scripts.** The comment over `terminal_themes()` in
[terminal_themes.h](../oui/include/oui/terminal_themes.h) describes the looks in order. Scripts name themes in [OUI
Code's repository](https://github.com/iganinja/oui_code): the `api(oui_run)` comment of `oui.setTheme` in
[oui_run/prelude.js](https://github.com/iganinja/oui_code/blob/master/oui_run/prelude.js) lists the ids, and
[scripting](https://github.com/iganinja/oui_code/blob/master/doc/scripting.md#windows) lists them in a sentence. There
the new id goes in both, and `oui_run tools/extract_api.js` makes its API files again, once its submodule points at a
version of OUI with the theme; its test `OuiRun.ScriptApi` fails while they are out of date.
5. **Check it reads.** [terminal_themes_test.cpp](../tests/oui/terminal_themes_test.cpp) runs over every theme
   `terminal_themes()` returns, with nothing to add:
   - `TurboComesFirstAndEachHasANameOfItsOwn`: `turbo` first, ids unique, names not empty;
   - `EveryTextReadsOnWhatItIsPaintedOn`: each pair of text and background OUI paints (editor, line numbers,
     selection, menus, buttons, tabs, lists, check boxes, shadows, scroll bar...) has a WCAG contrast of at least 2.5,
     dark grey on black being about the least that still reads; a label's shortcut letter needs 2.0.

   When it fails, the message names the theme and the pair: `amber: menu shortcut`.

Nothing else changes. OUI Code's **View > Theme** submenu in characters and `oui.setTheme()` in oui_run both walk
`terminal_themes()`, so the new look shows in the menu and answers to its id at once.

## Worked example: the amber theme

Amber is a whole theme to copy from: an amber monitor, yellow and brown on black, with brown surfaces for menus and
dialogs. Every pair the contrast test checks passes (the lowest are 2.8, dark grey on black, and 4.0, black on brown).
Phosphor is the same theme in greens, with white where light green on green would not read.

In `oui/src/terminal_themes.cpp`, inside the anonymous namespace:

```cpp
// Yellow and brown on black, as an amber monitor: for a dark terminal, warmer than Night.
Theme amber()
{
    Theme theme{TurboCTheme::make()};
    theme.background = black;
    theme.text = yellow;
    theme.muted_text = brown;
    theme.control_background = black;
    theme.panel_background = black;
    theme.panel_foreground = brown;
    theme.side_panel_background = black;
    theme.shadow = dark_gray;
    theme.shadow_foreground = black;
    theme.focus_background = brown;
    theme.focus_text = black;

    theme.dialog_background = brown;
    theme.dialog_foreground = black;
    theme.dialog_border = yellow;
    theme.dialog_heading_foreground = yellow;
    theme.dialog_icon_foreground = yellow;
    theme.dialog_border_placing = white;
    theme.table_header_background = brown;
    theme.table_header_foreground = black;
    theme.table_background = black;
    theme.table_foreground = yellow;
    theme.list_background = black;
    theme.list_foreground = brown;
    theme.item_selected_background = yellow;
    theme.item_selected_foreground = black;
    theme.cluster_background = brown;
    theme.cluster_foreground = black;
    theme.cluster_focused_foreground = yellow;
    theme.cluster_shortcut_foreground = white;

    theme.menu_background = black;
    theme.menu_foreground = yellow;
    theme.menu_border = brown;
    theme.menu_selected_background = brown;
    theme.menu_selected_foreground = black;
    theme.status_background = brown;
    theme.status_foreground = black;

    theme.button_background = brown;
    theme.button_foreground = black;
    theme.button_default_foreground = yellow;
    theme.button_shortcut_foreground = white;
    theme.label_shortcut_foreground = white;
    theme.button_focused_background = yellow;
    theme.button_focused_foreground = black;
    theme.button_pressed_background = black;
    theme.button_pressed_foreground = yellow;

    theme.tab_active_background = black;
    theme.tab_active_foreground = yellow;
    theme.tab_inactive_background = brown;
    theme.tab_inactive_foreground = black;

    theme.selection_background = brown;
    theme.selection_foreground = black;
    theme.search_match_background = yellow;
    theme.search_match_foreground = black;
    theme.occurrence_background = dark_gray;
    theme.occurrence_foreground = yellow;
    theme.bracket_match_background = red;
    theme.bracket_match_foreground = yellow;
    theme.diff_removed_background = red;
    theme.diff_removed_foreground = yellow;
    theme.diff_added_background = green;
    theme.diff_added_foreground = black;
    theme.diff_changed_background = brown;
    theme.diff_changed_foreground = black;
    theme.diff_filler_background = dark_gray;
    theme.scrollbar_background = black;
    theme.scrollbar_foreground = brown;

    theme.editor_background = black;
    theme.editor_foreground = yellow;
    theme.line_number_foreground = dark_gray;
    theme.reading_background = black;
    theme.reading_foreground = yellow;
    theme.syntax_keyword = white;
    theme.syntax_type = yellow;
    theme.syntax_function = yellow;
    theme.syntax_string = brown;
    theme.syntax_number = brown;
    theme.syntax_comment = dark_gray;
    theme.syntax_preprocessor = white;
    theme.syntax_operator = yellow;
    theme.heading_foreground = yellow;
    theme.code_foreground = yellow;
    theme.code_background = dark_gray;
    theme.link_foreground = white;

    theme.input_background = black;
    theme.input_foreground = yellow;
    theme.disabled_foreground = dark_gray;
    theme.hotkey_foreground = white;
    theme.error_foreground = light_red;
    theme.warning_foreground = yellow;
    theme.info_foreground = white;
    return theme;
}
```

The list at the end of the file:

```cpp
std::vector<TerminalTheme> terminal_themes()
{
    return {{"turbo", translate("TerminalTheme", "Turbo C++"), TurboCTheme::make()},
            {"night", translate("TerminalTheme", "Night"), night()},
            {"light", translate("TerminalTheme", "Light"), light()},
            {"monochrome", translate("TerminalTheme", "Monochrome"), monochrome()},
            {"amber", translate("TerminalTheme", "Amber"), amber()},
            {"phosphor", translate("TerminalTheme", "Phosphor"), phosphor()},
            {"commander", translate("TerminalTheme", "Commander"), commander()}};
}
```

Its name has `"Amber": "Ámbar"` under `"TerminalTheme"` in `oui/translations/es.json`, and `"amber"` is in the
`oui.setTheme` comment of oui_run's `prelude.js`, from which OUI Code's repository makes the declarations. A script says
`oui.setTheme("amber")`, and OUI Code shows **Amber** under **View > Theme** when it runs in a terminal.

## Graphical themes

In pixels, a look is a `GraphicalStyle` and the `Theme` made from it:

```cpp
struct GraphicalStyle
{
    Color desktop{44, 76, 64};       // what shows where nothing is drawn
    Color face, light, shadow;       // raised surfaces and their edges, light at the top and left
    Color title, title_text;         // a window's title bar
    Color well, well_text;           // sunken surfaces: lists, text fields, editors
    Color focus;                     // the focus ring, and a window placed from the keyboard
    Color text, muted_text;          // text on raised surfaces
    Color selection, selection_text;
    Color folder, page;              // icons of a file list
    int text_margin{6};              // how far interface text keeps from its cells' edges
    int line{1};                     // the thickness of an edge, in pixels
    bool flat{false};                // edges of `shadow` all round rather than bevels; held down, `light`
    InterfaceFont interface_font{InterfaceFont::jersey};  // or noto_sans
};
```

(abridged; the defaults are in [graphical_painter.h](../oui/include/oui/graphics/graphical_painter.h)).

The two do different jobs. `GraphicalPainter` draws windows, buttons, tabs, scrollbars, menus and fields from the
style's shades and ignores most of the theme it is passed: it reads only the button, tab, dialog text, menu
background and disabled colours. `theme_for(style)` derives the theme widgets paint their content with from the same
style (dialogs and buttons on `face`, editors, tables, panels and inputs on `well`, the menu's selection on `title`),
and adds the colours a style does not have: search matches, occurrences, comparison, and syntax colours chosen to
read on the dark well.

A program that changes the look sets both, as OUI Code's `Workbench::use_window_theme` does:

```cpp
for(const auto& theme : oui::graphical_themes())
{
    if(theme.id == id)
    {
        m_options.window_screen->set_style(theme.style);       // the backend's shapes
        m_application.set_theme(oui::theme_for(theme.style));  // the widgets' colours
    }
}
```

`PixelBackend::set_style` alone changes the shapes but leaves the widgets' colours as they were; `set_theme` alone the
other way round. A window, a DRM screen and a desktop client take their first style in their options
(`WindowOptions::style`, `DrmOptions::style`, `desktop_backend(options, style)`), `classic_style()` by default.

`graphical_themes()` builds `ocean` and `night` by copying `classic_style()` and changing its shades, and sets
`desktop` to `face` in each, which is what shows at the right and bottom edges of a window that is not a whole number
of cells. `dark` is `dark_style()`, which also changes the shapes: `flat` draws every raised and sunken surface with a
one-pixel edge of `shadow` all round, a button held down filled with `light` rather than sunk, and `interface_font`
writes interface text in Noto Sans rather than Jersey 10. The backend loads the font a style names when `set_style`
changes it. Its `title`, `focus` and `selection` are the same blue, the accent; a program drawing its own pixels in
the same look, as ouivi's timeline, takes its accent from `dark_style().title`. Adding a graphical look is the same as a terminal one: a style in `graphical_themes()` with an id and
`translate("GraphicalTheme", "...")`, its name in the `"GraphicalTheme"` context of OUI's catalogs, and it shows in
OUI Code's **View > Theme** in a window, as `dark` does. `oui.setTheme()` in scripts knows only `classic` among the
graphical looks.

The graphical look need not be a dark-on-light Windows clone: the colours of a terminal theme also work in a window,
since a `Theme` is only colours, but the frames and bevels then stay in the style's shades.

## Adding a colour to Theme

When a widget needs a colour no field gives, add a field rather than a fixed colour, so every theme can choose it:

1. Add the field to `Theme` in [theme.h](../oui/include/oui/theme.h), in its group, with a default and a comment
   saying what it colours.
2. Give it a value in `TurboCTheme::make()`, in each function of `terminal_themes.cpp` and in `theme_for` in
   `graphical_painter.cpp`. Those that do not set it keep the struct's default, which suits only the Turbo palette.
3. Add it to `theme_colors()` in [ui.cpp](../oui/src/ui.cpp), the table of names: without it, descriptions and
   scripts cannot name it, and the description editor does not suggest it. No test catches a field missing there.
4. If it is text painted on a background, add the pair to `painted()` in
   [terminal_themes_test.cpp](../tests/oui/terminal_themes_test.cpp), so every terminal theme is checked for it.

## Colours by name: descriptions and scripts

`named_theme_color(name)` (declared in [theme.h](../oui/include/oui/theme.h), defined in
[ui.cpp](../oui/src/ui.cpp)) turns a field's name into a pointer to it, or nothing:

```cpp
if(const auto field{oui::named_theme_color("table_background")})
{
    label.set_background(*field);   // a ColorRef to the field, resolved when painting
}
```

The names are exactly the fields' names in `Theme`. [Descriptions](oui_descriptions.md) take them in
`background=` and `foreground=`, and only them:

```
Label "Results" background=table_background
MarkdownView id=help background=reading_background foreground=reading_foreground
```

A name that is not a field stops the description with "There is no colour ... in the theme.", at its line and
column. `background=` is taken by `Label`, `LineEdit`, `MarkdownView` and `CheckButton`, and `foreground=` by
`MarkdownView`; scripts set the colours of more widgets through their `background` and `color` properties.

Scripts take either a name or a fixed colour written `"#rrggbb"`, six hexadecimal digits; `color_of` in
oui_run's [host.cpp](https://github.com/iganinja/oui_code/blob/master/oui_run/host.cpp) tries the fixed form first:

```js
label.background = "table_background";   // follows the theme
label.color = "#20a0ff";                 // fixed
canvas.fill(0, 0, width, 1, "#0000aa");
```

A name a theme has follows theme changes, as a `ColorRef` to a field does; a fixed colour stays as it is. A script
meant to look right in every theme names the theme's colours.

## Themes in oui_run scripts

`oui.setTheme(name)` sets the look of the script's window by id: one of `terminal_themes()`, `turbo`, `night`, `light`
`monochrome`, `amber`, `phosphor` or `commander`, or `classic`, the graphical one, in a build with the window module
(`oui_run_gui`). Any other name throws, listing the ids. It may be called before or after `oui.window()`.

Without a call, the window opens with `TurboCTheme::make()` in a terminal and `classic_theme()` in a desktop window
(`Host` in [host.cpp](https://github.com/iganinja/oui_code/blob/master/oui_run/host.cpp) remembers whether the script
chose one, so opening the window does not undo it).

```js
oui.setTheme("night");
const win = oui.window("Notes");
```

`oui.setTheme("classic")` sets `classic_theme()` only; the window keeps the style it opened with, which is the classic
one. A terminal theme in a desktop window colours the widgets' content in its colours over the classic style's
frames.

## Themes in OUI Code

OUI Code ([oui_code](https://github.com/iganinja/oui_code/tree/master/oui_code)) offers a **View > Theme** submenu with
the looks for where it shows, and keeps the choice in its settings file, `oui_code.ini`, beside the executable when its
folder can be written and in the user's configuration folder when not:

| Where it shows | Submenu lists | Setting | Default |
|----------------|---------------|---------|---------|
| A terminal (characters) | `terminal_themes()` | `terminal_theme` | `turbo` |
| A desktop window | `graphical_themes()` | `window_theme` | `classic` |

The submenu is built each time it opens (`view_items` in
[view_commands.cpp](https://github.com/iganinja/oui_code/blob/master/oui_code/view_commands.cpp)), marks the current
one, and on a choice applies it at once and saves the setting. At start-up `Workbench` reads the setting and applies it
(`use_terminal_theme`, `use_window_theme` in
[workbench.cpp](https://github.com/iganinja/oui_code/blob/master/oui_code/workbench.cpp)); an id that is no longer among
the themes leaves the default theme `main.cpp` set. On the whole screen (DRM) or a desktop of pixels it uses
`classic_theme()` with no submenu.
