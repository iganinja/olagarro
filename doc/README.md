# Olagarro's documentation

How olagarro works, for whoever uses its libraries or changes them. Each document stands on its own and links to the
others where they meet; this page says which to read for what. In OUI Code, F9 shows this page as it reads: a link
opens its document in the same view, and Alt+Left goes back.

## Start here

- [Overview](overview.md): every library and program, what each is for, how they depend on each other, and where
  everything is in the repository.
- [Building](building.md): CMake, its options, the terminal and window builds, static binaries, Windows, and the
  tool that formats the code.
- [Testing](testing.md): the test suites, widgets tested without a terminal, and how to run one case.

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

- [OUIScript](ouiscript.md): the JavaScript engine for those who embed it in C++: values, promises, time limits,
  the calls every scripting program shares, and highlighting rules.

The programs that run scripts, OUI Code and oui_run, are in [OUI Code's
repository](https://github.com/iganinja/oui_code), with their own documents: [scripting with
oui_run](https://github.com/iganinja/oui_code/blob/master/doc/scripting.md), for script writers; [the script
API](https://github.com/iganinja/oui_code/blob/master/doc/oui_api.md), every name scripts can use; [oui_run's
host](https://github.com/iganinja/oui_code/blob/master/doc/oui_run.md), how to add a call or a widget to it; and [OUI
Code's architecture](https://github.com/iganinja/oui_code/blob/master/doc/oui_code.md).

## The other libraries

- [Signals and slots](signalslot.md): `olagarro::SignalSlot`.
- [ByteStream](bytestream.md): binary serialisation and files.
- [Tasks](tasks_library.md): tasks composed in sequence, in parallel, by priority, waiting and interpolating, for
  games and animations.
- [Concurrency](concurrency.md): the task pool, cancelling, `concurrent_for`, and CTTasks, tasks composed at
  compile time.
