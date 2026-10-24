# OUI for C++ programs

OUI is the toolkit under OUI Code, oui_run, oui_calc and the desktop: widgets laid out in cells, the same whether
they show in a terminal, in a desktop window or on the whole screen. This document is for a C++ program that uses
it: how the application runs, how widgets are held and laid out, how keys and the mouse reach them, and what
ready-made widgets there are. How they are drawn is in [rendering](oui_rendering.md), their colours in
[themes](themes.md), and windows written as text rather than code in [descriptions](oui_descriptions.md).

- [The application and its loop](#the-application-and-its-loop)
- [Widgets and containers](#widgets-and-containers)
- [Measure and arrange](#measure-and-arrange)
- [Layout widgets](#layout-widgets)
- [Events](#events)
- [How events reach widgets](#how-events-reach-widgets)
- [Focus](#focus)
- [Commands and shortcuts](#commands-and-shortcuts)
- [Menus and context menus](#menus-and-context-menus)
- [Dialogs](#dialogs)
- [Popups and tooltips](#popups-and-tooltips)
- [Enabled and visible](#enabled-and-visible)
- [Ready-made widgets](#ready-made-widgets)
- [A complete program](#a-complete-program)

Everything is in the namespace `olagarro::oui`; `#include <oui/oui.h>` brings every public header, each of which is
also in `oui/include/oui` on its own. A program links `olagarro::OUI` (see [building](building.md)). The examples
here follow the repository's convention of `using namespace olagarro;` and names written as `oui::label`.

## The application and its loop

`oui::Application` ([application.h](../oui/include/oui/application.h)) owns what shows, the backend it shows on, the
command registry and the clipboard. `Application()` runs on the terminal it was started in;
`Application(std::unique_ptr<Backend>, Theme)` on any other backend, as a desktop window or a test's
`scripted_backend`. `set_backend` changes it later, as oui_run does when a script that started without a screen
opens a window.

```cpp
oui::Application application;
auto window{oui::window("Hello")};
window->add(oui::label("Hello from OUI"));
application.set_window(std::move(window));
return application.exec();
```

`set_content` puts any widget on the screen; `set_window` puts a `Window` there and quits the application when the
window closes. `exec()` runs frames until `quit()`. A frame, in `Application::run_frame`, does this in order:

1. Runs the timers that are due.
2. Runs the work other code posted.
3. Takes the events the backend has, up to 1024, and dispatches each.
4. Renders, if any of that happened or something asked for it: the content is arranged in the backend's size and
   painted, then the popup, the tooltip, the key mode's hint and a dragged image over it.
5. If nothing ran and nothing is posted, waits in the backend until input comes or the next timer is due.

So an idle program uses no processor: it sleeps in `Backend::wait_for_input` until there is something to do, and a
frame that changed nothing draws nothing. A widget that changes by itself, outside an event, does it from a timer or
posted work, both of which make the next frame render.

`post(callback)` queues a function to run on the application's thread at the start of the next frame. It takes a lock
and wakes the backend, so it is the one call other threads may make: a child process's output and a terminal's
screen reach their widgets this way. Work posted while a frame runs waits for the next one. Posting is also how a
callback defers something that must not happen inside the event being handled, such as opening a dialog from a
combo box's choice while its list is still open.

`start_timer(interval, callback)` runs the callback every interval, first one interval from now. It returns a
`Timer`, which is move-only: the timer lives as long as the object, and `stop()` or destroying it ends it. A timer due
while the application is busy runs once when it is free, not once for each interval missed. A one-shot timer stops
itself in its callback:

```cpp
// The preview is built again a moment after the last change, not at every key.
m_pending = application.start_timer(std::chrono::milliseconds{300}, [this] {
    m_pending.stop();
    rebuild();
});
```

Other things the application tells:

- `on_shutdown`: the terminal went away or the system asked the program to stop. Nobody is left to ask anything;
  this is where unsaved work is kept, without dialogs. Every loop then ends, an open dialog as cancelled.
- `on_close_requested`: the person asked to close the program from outside it, as with the close button of its
  window on a desktop. Without a callback the application quits; with one it decides, and can ask first.
- `on_terminal_focus_changed`, `on_modifiers_changed` (where the backend can tell, `reports_modifiers()`) and
  `on_zoomed`, after Ctrl and the wheel or Ctrl and plus or minus changed the size, where the backend can zoom.
- `set_event_observer` sees every event before anything else does, for logging.
- `set_title` names the application where the backend shows a title.

## Widgets and containers

Every widget derives from `oui::Widget` ([widget.h](../oui/include/oui/widget.h)). It cannot be copied or moved: it
is made on the heap and held by a `std::unique_ptr`, and whoever holds that pointer owns it. A widget in a container
is owned by the container; the top one is owned by the application.

Each widget has a factory that makes it, and a second form that makes it inside a container and returns a reference
to it there:

```cpp
auto content{oui::column()};                          // std::unique_ptr<Column>: yours
auto& title{oui::label("Files", *content)};           // Label&: the column owns it
auto& open{oui::button("&Open", *content, [] { /* clicked */ })};
content->add(oui::line_edit());                       // add returns the child as what it is: LineEdit&
```

`Container` ([container.h](../oui/include/oui/container.h)) holds its children in the order added. `add` takes a
`std::unique_ptr` and returns a reference to the child; `remove(child)` and `remove_at(index)` hand the child back as
a `std::unique_ptr<Widget>`; `replace(child, replacement)` puts another in its place and hands the first back. A
widget that is not a child is a `std::logic_error`. A widget taken out can be added elsewhere, as OUI Code moves a
document between groups of tabs.

References to children stay valid as long as the child is in the tree. A callback that runs later, after the widget
may have gone, holds a `WidgetRef<Kind>` instead: `get()` gives nothing once the widget is destroyed, even when
another was made at the same address since. `Widget::lifetime()` is the `std::weak_ptr` it uses.

A widget can carry an `id`, set with `set_id`; `find<Kind>(id)` searches it and everything under it and gives an
`OptionalRef`, and `get<Kind>(id)` throws when there is none. Descriptions give their widgets ids this way.

A widget of a program's own derives from `Widget`, or from `Container` when it holds others, and overrides what it
needs: `measure` (required), `arrange`, `paint`, `handle_event`, `is_focusable`, and for its children
`collect_focusable` and `hit_test`. Painting is `const`: state the paint changes, as where a field scrolls to, is
`mutable`.

## Measure and arrange

Layout runs before every paint, from the top: the application arranges its content in the whole screen, and each
container arranges its children inside its own bounds. Everything is in cells, columns and rows, also when a
graphical backend draws them in pixels.

- `Size measure(const Constraints& constraints) const` says how big the widget would like to be. `Constraints`
  ([geometry.h](../oui/include/oui/geometry.h)) has only maximums, `max_width` and `max_height`; a measure returns
  what it wants, clamped with `constraints.constrain(size)`. A widget may be measured many times, with different
  constraints, before it is arranged.
- `void arrange(const Rect& bounds)` gives it its place. `Widget::arrange` keeps the rectangle, which `bounds()`
  returns afterwards; a container calls it and then arranges each child.

What widgets ask for differs, and decides how they behave in a layout:

| Asks for | Widgets |
|----------|---------|
| Its content: the text and its frame | `Label` (one row, or as many as it wraps to), `Button` (two rows with its shadow, one compact), `CheckButton`, `LineEdit` (its text, at least 4 columns), `ComboBox`, `Table`, `TreeView` |
| All it is offered | `TextEdit`, `DockLayout`, `SplitView`, `StatusBar` (all the width, one row) |
| A fixed size | `VSpacer`, `HSpacer` |

A widget that asks for all it is offered fills whatever it is put in, and in a `Column` it leaves nothing to the
widgets after it: such widgets go in a `Grid` row that is `flex`, a `DockLayout`'s `fill`, or a `SplitView`.

Sizes in a grid's columns and rows are `Length`s:

| Length | Means |
|--------|-------|
| `Length::content()` | As wide as its widest child measures, or as tall as its tallest |
| `Length::cells(n)` | `n` cells |
| `Length::percent(p)` | `p` percent of the width the grid has (columns only) |
| `Length::fill()` | A share of what the others leave, `flex(1.0f)` |
| `Length::flex(f)` | A share of what the others leave, in proportion to `f` |

Gaps between children are a `Spacing`: `none`, `tight`, `normal` and `loose`, which are 0, 1, 2 and 3 cells.

## Layout widgets

### Row and Column

`oui::row()` lays its children side by side, `oui::column()` one under the other
([layout.h](../oui/include/oui/layout.h)), with `set_gap` between them. Each child gets what it measures along the line,
and the whole of the other side: in a row every child is as tall as the row, in a column as wide as the column. There is
no stretching along the line; a field that should take the width left goes in a `Grid` instead. `Row::set_centered` puts
the children in the middle of the row, as a dialog's buttons.

A hidden child of a row, a column or a grid is not painted, but it is still measured and keeps its room.

### Grids

`oui::grid()` ([grid.h](../oui/include/oui/grid.h)) places its children a row at a time in the columns
`set_columns` gives: with three columns, children 0, 1 and 2 are the first row, 3, 4 and 5 the second. A grid with no
columns shows nothing. A form is a grid of a content column and a fill column:

```cpp
auto& form{oui::grid(*content)};
form.set_columns({oui::Length::content(), oui::Length::fill()});
form.set_gap(oui::Spacing::tight);
form.add(oui::label("Project"));
form.add(oui::line_edit());
form.add(oui::label("Description"));
form.add(oui::line_edit());
```

Columns are worked out in `Grid::column_widths`: cells and percent columns first, content columns as their widest
child, and what is left of the width is shared among the fill and flex columns by their factors. When the grid is
only measured, a fill or flex column counts as its widest child, which is what it would take with no room to fill;
so a grid inside a row is as wide as its content, not nothing.

Rows are as tall as their tallest child, unless `set_rows` makes one `flex`: a flex row takes the height the others
leave, shared by the factors. A text editor over a line of buttons is a grid of one fill column and rows
`content, flex, content`. In rows only `flex` changes anything: a row given `fill`, `cells` or `percent` is measured
as its content.

`oui::dynamic_grid()` ([dynamic_grid.h](../oui/include/oui/dynamic_grid.h)) is the other kind: every cell the size of
the largest child, or `set_column_width(Length::cells(n))` wide, and as many columns as fit the width, the children
flowing into them.

### DockLayout

`oui::dock_layout()` ([dock_layout.h](../oui/include/oui/dock_layout.h)) lays children against the edges in the
order they were added: each takes a strip along its edge of what the earlier ones left, and `fill` children take
what remains. A hidden child takes no strip. The window of a program is usually one:

```cpp
auto layout{oui::dock_layout()};
layout->add(oui::Dock::top, oui::menu_bar(application.commands()));
layout->add(oui::Dock::bottom, oui::status_bar());
auto& tree{layout->add(oui::Dock::left, std::make_unique<oui::TreeView>(), 30)};
layout->set_resizable(tree, 12);
layout->add(oui::Dock::fill, oui::text_edit());
```

The third argument of `add` is the strip's size, its height for top and bottom and its width for left and right; 0
uses what the child measures. `set_resizable` gives a left or right strip a line along its inner edge that the mouse
drags, keeping at least `minimum` cells on each side; `on_resized` fires when a drag ends, so the program can
remember the size and give it back with `set_size`.

### SplitView

`oui::split_view(orientation)` ([split_view.h](../oui/include/oui/split_view.h)) shows two widgets, side by side
(`horizontal`) or one over the other (`vertical`), with a line between them that the mouse drags. A third `add` is a
`std::logic_error`. `set_ratio` is the first one's share, half unless set, and `move_divider(cells)` moves the line
from the keyboard. When either widget is hidden the other takes all the room and the line goes. Split views nest.

### TabView

`oui::tab_view()` ([tab_view.h](../oui/include/oui/tab_view.h)) shows one of its children under a row of titles.
`add_tab(title, widget)` adds a page; `select`, `close_tab`, `take_tab` (to move a page to another tab view),
`move_tab` and `recent_order` (the pages as recently used, for Ctrl+Tab) manage them. A tab shows a mark that asks to
close it unless `set_closable(false)`; asking only fires `on_close_requested`, and the owner closes it, after asking
about unsaved work if it must. `on_selection_changed` fires whenever another page shows; `on_tab_activated` when the
person clicks a title, for the owner to move the focus into the page. Only the selected page is hit-tested and
takes the focus.

### ScrollView

`oui::scroll_view()` ([scroll_view.h](../oui/include/oui/scroll_view.h)) holds one widget, measures it with no limit
on its height, and shows the part that fits, with a scrollbar at the right when it is taller. The wheel, Up and Down
and the scrollbar scroll it. It scrolls up and down only. Widgets with long contents of their own, as `TextEdit`,
`Table` and `MarkdownView`, scroll themselves and need none.

### Panel and spacers

`oui::panel()` ([panel.h](../oui/include/oui/panel.h)) holds one widget with `set_padding(Insets)` round it, on the
theme's panel background. `oui::vspacer(rows)` and `oui::hspacer(cells)` ([spacer.h](../oui/include/oui/spacer.h))
take room and show nothing: a blank row in a column, a column between a label and its field.

### Window

`oui::window(title)` ([window.h](../oui/include/oui/window.h)) is a frame with a title round one widget: it lays
out its first child only, so several go in a column inside it. A window with a border measures its child plus the
frame, at least as wide as its title, and centres itself in what it is given; `set_border(WindowBorder::none)` makes
it as big as what it is given, with no frame, as a program's main window is.

Its size can be shaped with `set_preferred_size` (the size it opens at even when its content measures smaller,
which a dialog built of flexible columns needs, as they measure as their content), `set_minimum_size`,
`set_maximum_size` and `set_height_share` (a share of the screen's height, which a long list then scrolls in).
`set_resizable(true)` lets the mouse drag its right and bottom edges; the title bar moves it. `set_placement_key`
names it so the application remembers where the person left it, through `set_window_placement_store`.

A window closes with `close(DialogResult)`, which fires `on_closed`. Escape closes it as cancelled, and so does its
close button, unless `set_close_on_escape(false)`. Enter presses its default button (`Button::set_default`) when
the widget with the focus does not take Enter itself, as a field does; `set_close_on_enter(true)` makes Enter close
it as accepted when there is no default button.

## Events

An `Event` ([event.h](../oui/include/oui/event.h)) is a `std::variant` of:

| Event | What it is |
|-------|------------|
| `KeyEvent` | A named key, `Key::enter`, `Key::f5`, `Key::page_down`, Space and Tab too, with `control`, `alt` and `shift` |
| `TextEvent` | A character typed, as a code point, with the same modifiers: Ctrl+S is a `TextEvent` of `s` with `control` |
| `MouseEvent` | The pointer's cell, `pressed`, `released`, `dragging`, `hover`, `right`, `wheel_delta`, the modifiers, and `clicks`: 1, 2 or 3 on a press |
| `ResizeEvent` | The screen's new size |
| `FocusEvent` | The terminal or window gained or lost the keyboard |
| `ModifiersEvent` | The modifiers held changed, where the backend can tell; a terminal cannot |
| `CloseRequestEvent` | Asked to close from outside, as by a window's close button |
| `ShutdownEvent` | The terminal went away, or the system asked the program to stop |
| `DropEvent` | Files carried over the window from the system: `moving` with the cell (and the pixel) they are over, `dropped` with every one's absolute path, `left` when they go away; a window can tell, a terminal cannot |

Letters arrive as text, in lowercase with `shift` set when Shift was held. Terminals send some Ctrl and letter
combinations as the bytes of other keys, so Ctrl+M is Enter, Ctrl+I Tab and Ctrl+[ Escape: shortcuts bound to those
never arrive as themselves. The application counts double and triple clicks itself, presses on the same cell within
half a second, so every backend has them.

A widget handles an event in `handle_event`, returning true when it used it. What is not used goes on to the
widget's parent, and on up.

## How events reach widgets

`Application::dispatch_event` decides who hears an event. The order matters when two parts of a program want the
same key, so here it is, as the code does it:

1. The event observer sees it. Ctrl and the wheel zoom, where the backend can; a mouse event moves the pointer, and
   any key hides the tooltip.
2. `ShutdownEvent`, `ModifiersEvent` and `CloseRequestEvent` are the application's, and go no further. A
   `DropEvent` goes to the widget under it and up through its parents until one takes it, whatever is focused or
   shown as a popup; the one that took the files while they moved hears `left` when another takes them, they are
   dropped elsewhere or they leave the window.
3. A key mode, while one runs (see [Focus](#focus)), takes every key; in a dialog, Ctrl+L starts placing it from the
   keyboard.
4. The popup, if one is shown, sees keys and the mouse over it first; a press elsewhere closes it and goes on.
5. In a dialog, Alt and a letter press the button, turn the check box or focus the field of the label that marks
   that letter.
6. A mouse event goes to the widget under the pointer (below), and stops there.
7. Tab and Shift+Tab without Ctrl or Alt go to the focused widget, and move the focus if it does not take them.
8. An open overlay that takes the keyboard, as an open menu, hears the key, up through its parents.
9. Reserved commands run (outside dialogs).
10. A key the focused widget claims as its own, through `Widget::claims_key`, goes to it: a file panel's F5, a
    terminal's Ctrl+W.
11. The application's shortcuts run, outside dialogs and while no overlay holds the keyboard.
12. The focused widget hears it: first its key handler and then `handle_event`, then each of its parents the same
    way.
13. Ctrl+A, C, X and V that a focused `LineEdit` did not take select, copy, cut and paste through the clipboard.
14. In a dialog, a letter alone that nothing took works as Alt and the letter, as in Turbo Vision; an Up or Down that
    nothing took moves the focus to the field above or below.
15. Outside a dialog, Alt and a letter that nothing took go to the letters marked in the window.

Shortcuts come before a widget's own use of a key (11 before 12), so a widget does not have to know which keys the
program binds; a widget that must keep a key whatever the program binds says so in `claims_key`. A terminal claims
almost every key, which is why a few commands are reserved (9): they are the way out of it.

**Key handlers.** `Widget::set_key_handler(handler)` lets a program hear keys and text before a widget does, while
it or anything inside it has the focus; returning true keeps the key. It gives a widget keys of its own without a
new class:

```cpp
list.set_key_handler([&](const oui::Event& event) {
    const auto* const key{std::get_if<oui::KeyEvent>(&event)};
    if(key != nullptr && key->key == oui::Key::delete_)
    {
        remove_selected();
        return true;
    }
    return false;
});
```

**Typing elsewhere.** `delegate_typing_to(target)` sends characters and Backspace typed while the widget has the
focus to another widget, and the focus with them: a list under a filter field sends typing to the field and keeps
the arrows and Enter.

**The mouse.** A press is dispatched to the widget `hit_test` finds under the pointer, then up through its parents
until one handles it. A press also gives the focus to the first widget from there up that takes it, as a list takes
it for a click on one of its rows. From the press to the release the mouse is captured: drags and the release go to
the widget pressed, wherever the pointer is. If that widget goes away mid-click, as the editor under a dialog a
double click just closed, the rest of the click is dropped instead of landing on whatever is now under the pointer.
`Widget::set_pointer_handler(handler)` hears each of those events before the widget does, as a key handler hears
keys: returning true keeps it. A number field so drags its value along its text field, which would select the text.
`pointer_at` says what the pointer looks like over a widget, an arrow or a line to drag between columns or rows.

**Hit testing.** `Container::hit_test` asks its children from the last added to the first, so later children are
over earlier ones; a disabled widget is not there for the pointer, and what is under it is. Overlays come first.

**Overlays.** A widget whose `is_overlay()` is true spills over its neighbours, as an open combo box's list or an
open menu: containers paint it after all their other children, however deep in the tree it is, and ask it first in
`hit_test`, even outside their own bounds. `keyboard_target()` names the overlay that takes the keyboard while it is
open. A press anywhere calls `dismiss_overlays_at`, which closes those it did not fall in.

## Focus

One widget has the keyboard focus; `Application::focused()` says which, and `is_focused()` asks a widget. A widget
takes the focus when `is_focusable()` is true, it is enabled, and it and its containers are visible.
`collect_focusable` lists them in the order of the tree, which is the order Tab goes through, wrapping round.

- `Application::focus(widget)` gives it the focus; `reset_focus()` gives it to the first that takes it.
- Setting new content, or showing a dialog, focuses its first focusable widget. A dialog that should start
  elsewhere posts the focus before `show_modal`, as `show_help` in OUI Code does:
  `application.post([&] { application.focus(field); });`.
- When the focused widget is removed, the focus goes to the first one there is, the next time an event comes.
- `focus_toward(dx, dy)` moves the focus to the nearest widget on that side, `(1, 0)` for the right, preferring one
  beside it to one nearer but off to a side: OUI Code moves between panels with it.
- In a dialog, Up and Down nobody used move to the previous and next field, stopping at the first and last.
- `TabView` lists only its selected page; a hidden or disabled container hides everything in it.

**Key modes.** `start_key_mode(hint, handler)` takes the keyboard for a while: every key goes to the handler, the
hint shows on the bottom line over whatever is there, and mouse events are dropped. The handler answers
`KeyModeAnswer::keep` to go on, `end` to stop, or `end_and_pass` to stop and let the key do what it does outside
the mode. Placing a dialog from the keyboard with Ctrl+L is one; OUI Code's pick of a side for a split is another.

## Commands and shortcuts

`CommandRegistry` ([command.h](../oui/include/oui/command.h)), from `application.commands()`, is the one place
menus, shortcuts and the command palette read from. A command has an id, a title and a callback:

```cpp
auto& commands{application.commands()};
commands.register_command("file.save", oui::translate("Commands", "&Save"), [&] { save(); });
commands.bind("file.save", {U's', true});                           // Ctrl+S
commands.bind("search.next", oui::KeyShortcut{oui::Key::f3});       // F3
commands.bind("search.previous", oui::KeyShortcut{oui::Key::f3, false, false, true});  // Shift+F3
```

A `Shortcut` is a character, in lowercase with `shift` for Shift, or a named key in `key`; `KeyShortcut` is the
short way to write a named one. `parse_shortcut("Ctrl+Shift+F3")` reads one from text and `format_shortcut` writes
one, translated. Binding to a command that is not registered throws, as a slip that should show at once. A command
can have several shortcuts; `shortcut(id)` gives the first.

- `set_enabled(id, false)`: the command does not run from its keys or anywhere, menus show it dimmed and the palette
  leaves it out.
- `set_checked(id, value)`: it shows a tick in menus and its state in the palette.
- `set_title`, `set_tooltip` and `set_palette_visible`.
- `execute(id)` runs it; `unregister_command` takes it and its keys away.
- `reserve(id)`: its keys work even over a widget that takes every key, as a terminal does.

A shortcut matches exactly; a binding with Ctrl and no Shift also answers Ctrl+Shift when nothing is bound to that,
so Ctrl+Shift+Z still undoes. Shortcuts run only when no dialog is shown: inside a dialog keys belong to it.

`show_command_palette(application)` ([command_palette.h](../oui/include/oui/command_palette.h)) lists the enabled
commands with their shortcuts in a filter, and runs the one picked once it closes.

## Menus and context menus

`MenuBar` ([menu_bar.h](../oui/include/oui/menu_bar.h)) is a widget, made with the registry, usually docked at the
top. Its menus are lists of `MenuItem`:

```cpp
auto& bar{layout->add(oui::Dock::top, oui::menu_bar(application.commands()))};
bar.add_menu("&File", {"file.open", "file.save", oui::menu_separator(),
                       oui::menu_submenu("Open &Recent", [&] { return recent_items(); }), "app.quit"});
```

An item is a registered command, written as its id, which brings its title, shortcut, tick and enabled state from
the registry; a `menu_action(title, function)`, for entries made on the fly such as recent files; a separator; or a
submenu, whose entries can be a function asked each time it opens. `with_shortcut` shows a key on an action, such as
one a widget handles itself. Titles mark their mnemonic with `&`, `&&` for an ampersand: Alt and the letter of a menu's
title open it from anywhere, through a hidden command the bar registers, and inside an open menu the letters pick
the entries. The bar removes those commands when it is destroyed, so it must not outlive the registry.
`set_menu_items` replaces a menu's entries.

A context menu opens through the bar: `bar.open_context_menu(items, at)` shows the items as a menu of their own with
its corner at `at`, the cell the right click fell on, and it works and closes as the bar's own menus do. A widget
that offers one fires a signal with where the click was, and the program opens the menu there; OUI Code's
`Workbench` does this for its editors and panels.

`Menu` ([menu.h](../oui/include/oui/menu.h)) is a plain vertical list of commands, as a widget, for a menu shown in
place rather than dropped down.

## Dialogs

A dialog is a `Window` shown modally: `Application::show_modal(std::move(window))` puts it over what shows, runs
frames until it closes, and returns its `DialogResult`. The call does not return until then, so the code after it
goes on with what the person chose. The window is destroyed as the call returns, and its widgets with it, so what is
wanted from them is taken before, in the callback that closes it:

```cpp
auto dialog{oui::window("Rename")};
auto& dialog_reference{*dialog};
auto& column{oui::column(*dialog)};
auto& name{oui::line_edit(column)};
auto& buttons{oui::row(column)};
buttons.set_centered(true);
buttons.set_gap(oui::Spacing::tight);
std::string chosen;
auto& ok{oui::button("OK", buttons, [&] {
    chosen = std::string{name.text()};
    dialog_reference.close(oui::DialogResult::accepted);
})};
ok.set_default(true);
oui::button("Cancel", buttons, [&] { dialog_reference.close(oui::DialogResult::cancelled); });
if(application.show_modal(std::move(dialog)) == oui::DialogResult::accepted)
{
    rename_to(chosen);
}
```

Callbacks inside a dialog can show other dialogs; they stack, each call waiting for its own.

`show_modal_and_keep(window)` does the same and gives the window back in the same `std::unique_ptr` when it closes,
to read its fields afterwards or show it again as the person left it: oui_run's `Dialog.show()` is built on it.

`push_overlay(window)` shows a window over the content without waiting, and `pop_overlay()` takes it away; it goes
by itself when the window closes. `TabSwitcher` shows its list this way.

Ready-made dialogs, each a function that shows it modally and returns the answer:

| Function | Header | Asks |
|----------|--------|------|
| `message_box(application, title, message, buttons)` | [dialogs.h](../oui/include/oui/dialogs.h) | A message and a row of buttons; the index pressed, or none on Escape |
| `prompt(application, PromptOptions)` | [dialogs.h](../oui/include/oui/dialogs.h) | One line of text, with a check before accepting and room for more under the field |
| `file_dialog(application, FileDialogOptions)` | [file_dialog.h](../oui/include/oui/file_dialog.h) | A file to open or save, or a folder, with filters |
| `quick_pick(application, items, QuickPickOptions)` | [quick_pick.h](../oui/include/oui/quick_pick.h) | One of many items, filtered fuzzily or by a wildcard as typed |
| `show_command_palette(application)` | [command_palette.h](../oui/include/oui/command_palette.h) | A command, which then runs |

A modal window with a `placement_key` reopens where the person left it when the application has a placement store
(`set_window_placement_store`, two functions that load and save a `WindowPlacement` by key). While a dialog shows,
Ctrl+L, or the key `set_placement_shortcut` gives, moves it with the arrows and resizes it with Ctrl and the arrows,
for a terminal with no mouse.

## Popups and tooltips

`show_popup(widget, closed)` shows a widget over everything, as suggestions at the cursor, without taking the focus:
typing goes on where it was. The popup sees keys and the mouse first, keeps what it handles, and lets the rest go
on as if it were not there; a press outside it closes it. It is arranged in the whole screen and places itself in
it, as `PopupList` ([popup_list.h](../oui/include/oui/popup_list.h)) does under its anchor cell. There is one at a
time: another popup or a dialog closes it, and `closed` is called however it goes. `close_popup()` is safe from
inside the popup's own handling of an event: the widget is kept until the next frame.

A widget's tooltip is the text `set_tooltip` gave it, or what `tooltip_at(point)` answers for widgets that say
something about each part, as a tab view about each title. It shows once the pointer has rested 600 ms, beside the
pointer and never under it, broken into lines when wider than the screen; the pointer moving, a press or a key hides
it. Tooltips do not show while a dialog does.

## Enabled and visible

`set_visible(false)` hides a widget: it is not painted, not hit by the mouse and takes no focus. Rows, columns and
grids still give it its room; dock layouts and split views give it away.

`set_enabled(false)` keeps it showing, dimmed, and makes it answer nothing: no click (it is not there for
`hit_test`, so the click falls on what is under it), no key, no letter with Alt, no focus. `is_enabled()` is false
also for anything inside a disabled container, so disabling a panel disables what it holds. A widget disabled while
it has the focus hears nothing more.

## Ready-made widgets

| Widget | Header | What it is |
|--------|--------|------------|
| `Label` | [label.h](../oui/include/oui/label.h) | Text on one line, or wrapped; bold, coloured, centred or to the right; a mark before it; `set_buddy` names the field its `&` letter focuses |
| `Button` | [button.h](../oui/include/oui/button.h) | A push button with a shadow, or compact on one row; `on_clicked`; `set_default` for the one Enter presses |
| `CheckButton` | [check_button.h](../oui/include/oui/check_button.h) | A box to tick with its text; `on_toggled` |
| `LineEdit` | [line_edit.h](../oui/include/oui/line_edit.h) | A one-line field with selection and a placeholder, or for a password with `set_password`; `on_changed` and `on_submitted` on Enter |
| `NumberField` | [number_field.h](../oui/include/oui/number_field.h) | A number to type, in a range and on a step, with arrows, keys and the wheel to step it; with `set_draggable`, dragged along to change it as in Blender (Shift finer, Ctrl coarser, a double click to type over it), `on_drag_started` and `on_drag_finished` around a drag; `set_mixed` shows "—" for several numbers that differ |
| `ProgressBar` | [progress_bar.h](../oui/include/oui/progress_bar.h) | How far something has gone, from 0 to 1, as a bar filling from the left, with a text after it such as "4 of 8"; by eighths of a cell in a terminal |
| `TextEdit` | [text_edit.h](../oui/include/oui/text_edit.h) | A multi-line editor: undo, several cursors, search and replace with regular expressions, line operations, bracket matching, word wrap, line numbers, syntax colours |
| `ComboBox` | [combo_box.h](../oui/include/oui/combo_box.h) | One option of a list, which drops down; `on_selected` |
| `Table` | [table.h](../oui/include/oui/table.h) | Rows of cells, each any widget, with headers that sort and resize, selection, typing to find a row, scrolling both ways |
| `List` | [list.h](../oui/include/oui/list.h) | A table of one column and no headers |
| `CheckList` | [check_list.h](../oui/include/oui/check_list.h) | A list with a box to tick on each row |
| `FolderView` | [folder_view.h](../oui/include/oui/folder_view.h) | A folder's contents as a list, folders first; an explorer, or a project tree kept inside a folder; asks the program to rename, copy, delete |
| `TreeView` | [tree_view.h](../oui/include/oui/tree_view.h) | Nodes that open and close, with a header and an empty message |
| `TabView` | [tab_view.h](../oui/include/oui/tab_view.h) | Pages under a row of titles that close, drag and reorder |
| `MarkdownView` | [markdown_view.h](../oui/include/oui/markdown_view.h) | Markdown shown formatted and read-only, with links to sections and out |
| `DiffView` | [diff_view.h](../oui/include/oui/diff_view.h) | Two texts side by side, their differences in colour, F7 to the next |
| `TerminalView` | [terminal_view.h](../oui/include/oui/terminal_view.h) | A program in a pseudo-terminal of its own, its screen shown as xterm would, history included |
| `CalculatorView` | [calculator_view.h](../oui/include/oui/calculator_view.h) | oui_calc in a widget: an expression line, its result and a log |
| `StatusBar` | [status_bar.h](../oui/include/oui/status_bar.h) | A row of text at both ends, and keys with what they do; a provider can say it before each paint |
| `MenuBar`, `Menu` | [menu_bar.h](../oui/include/oui/menu_bar.h), [menu.h](../oui/include/oui/menu.h) | Menus of commands; context menus |
| `PopupList` | [popup_list.h](../oui/include/oui/popup_list.h) | A list at a cell, for `show_popup`: suggestions as typed |
| `Suggester` | [suggester.h](../oui/include/oui/suggester.h) | Suggestions as one writes in a `TextEdit`, from its words, a language's and an API's declarations |
| `SignatureTip` | [signature_tip.h](../oui/include/oui/signature_tip.h) | How the call the cursor is in is called, over its line |
| `Row`, `Column` | [layout.h](../oui/include/oui/layout.h) | Children side by side, or one under the other |
| `Grid`, `DynamicGrid` | [grid.h](../oui/include/oui/grid.h), [dynamic_grid.h](../oui/include/oui/dynamic_grid.h) | Children in columns, a row at a time |
| `DockLayout` | [dock_layout.h](../oui/include/oui/dock_layout.h) | Children against the edges, the rest filling |
| `SplitView` | [split_view.h](../oui/include/oui/split_view.h) | Two children with a line the mouse drags |
| `ScrollView` | [scroll_view.h](../oui/include/oui/scroll_view.h) | One child, scrolled up and down |
| `Panel` | [panel.h](../oui/include/oui/panel.h) | One child with padding, on the panel colour |
| `VSpacer`, `HSpacer` | [spacer.h](../oui/include/oui/spacer.h) | Empty room |
| `Window` | [window.h](../oui/include/oui/window.h) | A frame with a title, which moves, resizes and closes |

And beside them, things that are not widgets but work with them: `TabSwitcher`
([tab_switcher.h](../oui/include/oui/tab_switcher.h)), the pop-up list of tabs for Ctrl+Tab; `TypeAhead`
([type_ahead.h](../oui/include/oui/type_ahead.h)), finding a row by what is typed; `ScrollbarDrag`
([scrollbar.h](../oui/include/oui/scrollbar.h)), the mouse on a scrollbar, for a widget that draws one;
`start_process` ([process.h](../oui/include/oui/process.h)) and `start_pty` ([pty.h](../oui/include/oui/pty.h)),
child processes whose output arrives on the application's thread.

## A complete program

A window with a field and a button that greets whoever is named in it. Alt+N goes to the field, Alt+G or Enter in
the field greets, Escape or Ctrl+Q ends it.

```cpp
#include <oui/oui.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

using namespace olagarro;

int main()
{
    oui::Application application;

    auto window{oui::window("Greeter")};
    auto content{oui::column()};
    content->set_gap(oui::Spacing::tight);

    // The field takes the width the label leaves.
    auto& form{oui::grid(*content)};
    form.set_columns({oui::Length::content(), oui::Length::cells(1), oui::Length::fill()});
    auto& caption{oui::label("&Name:", form)};
    oui::hspacer(1, form);
    auto& name{oui::line_edit(form)};
    name.set_placeholder("Who to greet");
    caption.set_buddy(name);

    auto& greeting{oui::label("", *content)};

    auto& buttons{oui::row(*content)};
    buttons.set_centered(true);
    buttons.set_gap(oui::Spacing::tight);
    auto& greet{oui::button("&Greet", buttons,
                            [&greeting, &name] { greeting.set_text("Hello, " + std::string{name.text()} + "!"); })};
    greet.set_default(true);
    auto& window_reference{*window};
    oui::button("Close", buttons, [&window_reference] { window_reference.close(oui::DialogResult::accepted); });
    // A field keeps Enter for itself, so it greets from there too.
    name.on_submitted([&greet](std::string_view) { greet.click(); });

    window->add(std::move(content));
    application.set_window(std::move(window));
    application.focus(name);

    application.commands().register_command("greeter.quit", "&Quit", [&application] { application.quit(); });
    application.commands().bind("greeter.quit", {U'q', true});
    return application.exec();
}
```

The references `greeting`, `name` and `greet` stay good while the window shows, as the window owns what they name
and the window lives until `exec()` returns. Closing the window, with the Close button, Escape or its close mark,
quits, as `set_window` connects it. The same window written as a description is in
[descriptions](oui_descriptions.md#building-one-from-c).
