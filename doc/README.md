# Olagarro's documentation

How olagarro works, for whoever uses its libraries or changes them. Each document stands on its own and links to the
others where they meet; this page says which to read for what. In OUI Code, F9 shows this page as it reads: a link
opens its document in the same view, and Alt+Left goes back.

## Start here

- [Overview](overview.md): every library and program, what each is for, how they depend on each other, and where
  everything is in the repository.
- [Building](building.md): CMake, its options, the terminal and window builds, small static binaries, Windows, and
  the tools that format the code and regenerate the script API.
- [Testing](testing.md): the test suites, the end-to-end tests that drive programs in a pseudo terminal, and how to
  run one case.

## OUI, the user interface toolkit

- [Layout and events](oui_layout.md): the application and its loop, widgets and containers, measuring and arranging,
  events and focus, commands, menus, dialogs, and the ready-made widgets.
- [Descriptions](oui_descriptions.md): `.oui` files, which say what a window holds and where, for C++ and for
  scripts, and OUI Code's Interactive UI tab.
- [Rendering](oui_rendering.md): the painter, the terminal, window and console backends, colours in a terminal,
  fonts, text in pixels, zoom, and how a widget paints.
- [Themes](themes.md): what a theme is, every colour in it, the themes OUI carries, and how to add one, with a whole
  example.
- [Translations](translations.md): translated texts, their catalogs, and how to add a language.

## Scripts

- [Scripting with oui_run](scripting.md): writing programs and tools in JavaScript, for script writers.
- [The script API](oui_api.md): every name scripts can use, with its signature; made from the sources.
- [OUIScript](ouiscript.md): the JavaScript engine for those who embed it in C++, and how to add a call or a widget
  to oui_run.

## OUI Code

- [OUI Code](oui_code.md): how the editor is put together, for whoever changes it. Its user manual is
  [its help](../examples/oui_code/help/help.md), which F1 opens.

## The other libraries

- [Signals and slots](signalslot.md): `olagarro::SignalSlot`.
- [ByteStream](bytestream.md): binary serialisation and files.
- [Tasks](tasks_library.md): tasks composed in sequence, in parallel, by priority, waiting and interpolating, for
  games and animations.
- [Concurrency](concurrency.md): `concurrent_for`, and CTTasks, tasks composed at compile time.
