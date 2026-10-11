# Translations

OUI and its programs show every text through `translate(context, text)`, which looks the English text up in JSON
catalogs built into the program and gives it back translated, or as it is when no catalog has it. This document
covers the calls, the catalog format, where catalogs live and how they get into a program, how a language is chosen
and installed, how to add a language, and the tests that keep catalogs complete.

- [Translating a text](#translating-a-text)
- [Catalogs](#catalogs)
- [Where catalogs live](#where-catalogs-live)
- [Embedding catalogs in a program](#embedding-catalogs-in-a-program)
- [Choosing and installing a language](#choosing-and-installing-a-language)
- [Placeholders: format_text](#placeholders-format_text)
- [The other way round: untranslate](#the-other-way-round-untranslate)
- [Texts in descriptions and scripts](#texts-in-descriptions-and-scripts)
- [Adding a language](#adding-a-language)
- [Adding a text](#adding-a-text)
- [Tests that check catalogs](#tests-that-check-catalogs)

Everything here is in [translation.h](../oui/include/oui/translation.h) and
[translation.cpp](../oui/src/translation.cpp), namespace `olagarro::oui`.

## Translating a text

```cpp
button->set_text(oui::translate("FileDialog", "Cancel"));
application.set_title(oui::translate("Calculator", "Calculator"));
```

The **context** says where a text belongs, so the same English word can be translated differently in two places:
`"Open"` in a file dialog and in a menu may want different words. Contexts are short names, usually the class or the
part of the program: `FileDialog`, `Menu`, `Editor`, `TerminalTheme`.

The **text** is the English as the program shows it, mnemonic `&` included (`"&File"`, `"Zoom &In"`).

Both arguments are **string literals**, always. The catalog test finds texts by searching the sources for
`translate("...", "...")`; a text built at run time, or passed through a variable, is a text no test can see, and the
test reports any `translate(` whose arguments are not literals. Where a text is made of parts, translate a whole
sentence with placeholders and fill them with [format_text](#placeholders-format_text).

`translate` returns a `std::string`, translated by the first installed catalog that has the text, newest first, or
the text unchanged. An empty translation counts as none.

Widgets take their texts when they are made: a label keeps the string it was given. So catalogs are installed once,
at start-up, before the interface is built, and changing language takes a restart; OUI Code's preferences say so ("The
language changes the next time OUI Code starts."). The list of installed catalogs is a plain global, not guarded for
threads.

## Catalogs

A catalog is the translations of one language, as a JSON file:

```json
{
  "contexts": {
    "FileDialog": {
      "Cancel": "Cancelar",
      "Open": "Abrir"
    },
    "TerminalTheme": {
      "Light": "Claro",
      "Monochrome": "Monocromo",
      "Night": "Noche",
      "Turbo C++": "Turbo C++"
    }
  },
  "language": "es",
  "name": "Español"
}
```

- `language`: the language code, as `system_language()` gives it (`"es"`). Required.
- `name`: the language's name in that language, for a language chooser. The catalog test requires it.
- `contexts`: an object of contexts, each an object from English text to its translation.

`parse_translation_catalog(json, error)` reads one. It reads only the JSON a catalog uses (objects whose values are
strings or further objects, with every string escape, `\uXXXX` and surrogate pairs included) and rejects any other key,
a missing `language`, and text after the catalog, with the line and column in `error`: `line 3, column 7: expected
':'`. The catalogs in the repository keep keys in alphabetical order, contexts and texts, as the examples show.

A `Translation` is a parsed catalog ready for lookups: `language()`, `name()`, `find(context, text)` and
`source_of(context, translated)`.

## Where catalogs live

Each part of the repository has its own catalogs, one file per language, in a `translations` folder beside its
sources:

| Catalogs | Cover the texts of | Embedded as |
|----------|--------------------|-------------|
| [oui/translations](../oui/translations) | the library, `oui/include` and `oui/src` | `embedded_oui_catalogs()`, inside OUI |
| [examples/oui_code/translations](../examples/oui_code/translations) | OUI Code, its sources and `.oui` descriptions | `embedded_oui_code_catalogs()` |
| [examples/oui_run/translations](../examples/oui_run/translations) | oui_run | `embedded_oui_run_catalogs()` |
| [examples/oui_calc/translations](../examples/oui_calc/translations) | oui_calc | `embedded_oui_calc_catalogs()` |

Today each folder has `es.json`. OUI's catalog covers what OUI itself shows (dialogs, file dialogs, menus it makes,
key names in shortcuts, theme names), so a program built on OUI gets them translated without carrying them. A
program's catalog covers only its own texts; oui_term and oui_desktop have none and use OUI's alone.

## Embedding catalogs in a program

Catalogs are built into the binary, not read from disk: a program carries its languages with it, as a single file.
`olagarro_embed_files` ([cmake/OlagarroEmbedFiles.cmake](../cmake/OlagarroEmbedFiles.cmake)) generates a source with a
function that returns each file's contents, gzip-compressed in the binary and inflated the first time it is called:

```cmake
olagarro_embed_files(olagarro_oui NAMESPACE olagarro::oui FUNCTION embedded_oui_catalogs
    FILES oui/translations/es.json)
olagarro_embed_files(olagarro_oui_run FUNCTION embedded_oui_run_catalogs
    FILES examples/oui_run/translations/es.json)
```

The program declares the function and turns its files into translations:

```cpp
std::vector<std::string_view> embedded_oui_run_catalogs();
...
oui::install_language(oui::system_language(), oui::embedded_translations(embedded_oui_run_catalogs()));
```

`embedded_translations(catalogs)` parses each one and throws `std::runtime_error` naming the catalog and the error
when one does not parse: a broken built-in catalog is a broken build, not something to carry on without.
`oui_translations()` gives OUI's own, parsed once. Editing a catalog regenerates the source on the next build.

## Choosing and installing a language

`system_language()` gives the user's language as a code such as `"es"`:

- POSIX ([language_posix.cpp](../oui/src/language_posix.cpp)): the first of `LANGUAGE`, `LC_ALL`, `LC_MESSAGES`,
  `LANG` that is set, as gettext reads them, cut at the first `_`, `.`, `@` or `:` (`es_ES.UTF-8` gives `es`); `C`
  and `POSIX` give `"en"`.
- Windows ([language_win32.cpp](../oui/src/language_win32.cpp)): the user's default locale name, cut at `-`.
- `"en"` when nothing says.

`install_language(language, application_translations)` replaces whatever was installed with OUI's catalog for the
language and then the application's, in that order. Since lookups go newest first, a program's catalog can override
one of OUI's texts in the same context. English, or a language with no catalogs, installs nothing, and texts show as
the sources write them.

`installed_language()` says the language of the catalog installed last, `"en"` when none is. Code that has a whole
text per language rather than a catalog entry picks it with `text_for_language`, by the names of the files
`olagarro_embed_files` built in: the one named as English's with the language before its extension, `help.es.md`
beside `help.md`, or English's when the language has none.

```cpp
// oui/src/calculator_view.cpp: calculator.md, calculator.es.md... built in, and the one in the language installed
std::string markdown{
    text_for_language(embedded_oui_calculator_help(), embedded_oui_calculator_help_names(), installed_language())};
```

The lower-level `install_translation(Translation)` adds one catalog on top, and `remove_translations()` removes all;
tests use them to install a catalog of their own.

Programs install at the start of `main`, before anything shows or makes a text:

- oui_run, oui_calc, oui_term and oui_desktop install `system_language()`.
- OUI Code reads the `language` setting, `"auto"` by default, which follows `system_language()`; its **Edit >
  Preferences** offers **System language**, **English** and one entry per catalog it carries, by the catalog's `name`
  (`show_preferences` in [preferences.cpp](../examples/oui_code/preferences.cpp)). Its tests run in English, as the
  sources write it.

## Placeholders: format_text

A sentence with values in it is translated whole, with numbered placeholders, and filled after:

```cpp
oui::format_text(oui::translate("Editor", "Could not open {0}: {1}"), {path, reason});
```

`format_text(pattern, arguments)` replaces `{0}`, `{1}`... (one or two digits) with the arguments, so a translation
can put them in another order. A placeholder with no argument, or anything else in braces, stays as it is. The catalog
test checks that a translation has the same placeholders as its English.

## The other way round: untranslate

`untranslate(context, text)` gives the English a translated text came from, through the installed catalogs, or the
text unchanged. It is for code that meets a text the user typed or the interface shows and must compare it with what
the sources call it. OUI Code uses it to place a script's commands in a menu the script names in either language:

```cpp
// examples/oui_code/tools_commands.cpp
comparable(item.title()) == comparable(name) ||
    comparable(oui::untranslate("Menu", item.title())) == comparable(name)
```

## Texts in descriptions and scripts

A [description](oui_descriptions.md) (`.oui`) translates its texts under the context its root names:

```
Window "Preferences" context=Preferences
  Label "&Language:" buddy=language
  Label "The language changes the next time OUI Code starts."
```

A widget's own text, `placeholder=` and `tooltip=` are translated; a text that shows as written, as a name or an
arrow, says `untranslated`. A description with texts and no `context=` fails to build, unless it is built with
`UiTexts::as_written`, as scripts' descriptions are: a script's texts show as the script writes them. `ui_texts(root)`
lists a description's texts with their context and place, which is how the catalog test reads `.oui` files.

Scripts do not call `translate`. OUI Code's own scripts give a command's title per language, `{en: "...", es: "..."}`,
and `Scripting` picks the one for the interface language, English when there is none.

## Adding a language

For French, say:

1. **OUI's catalog.** Copy `oui/translations/es.json` to `oui/translations/fr.json`, set `"language": "fr"` and
   `"name": "Français"`, and translate every text. Keep the contexts and the English keys exactly; keep each `&`
   (moving it to a letter of the translation) and each `{0}`.
2. **Embed it.** Add the file to the `FILES` of `embedded_oui_catalogs` in [CMakeLists.txt](../CMakeLists.txt).
3. **Each program's catalog.** The same for `examples/oui_code/translations`, `examples/oui_run/translations` and
   `examples/oui_calc/translations`, and their `olagarro_embed_files` lines. The catalog test checks every `.json`
   in each folder, so a program cannot be half translated: either a language's file covers it all, or it is not
   there.
4. **Texts that are not in catalogs.** Some texts come whole per language: OUI Code's help
   (`examples/oui_code/help/help.md` and `help.es.md`, chosen in `help_markdown` in
   [help.cpp](../examples/oui_code/help.cpp)), the calculator's help (`oui/help/calculator.md` and `calculator.es.md`,
   chosen in `calculator_help` in [calculator_view.cpp](../oui/src/calculator_view.cpp)) pick their file by its name,
   so a new language needs only its file, `help.fr.md`, added to their `olagarro_embed_files` lines; without it, the
   English one shows. The commands
   OUI Code's scripts define give their titles by language code, `{en: ..., es: ...}`; a title without the new code
   shows in English.
5. **Update the test that counts OUI's catalogs.** `OuisBuiltInCatalogsAreTheOnesInTheRepository` in
   [translation_test.cpp](../tests/oui/translation_test.cpp) expects exactly one built-in catalog, Spanish.

OUI Code's preferences list the new language by itself, from its catalogs.

## Adding a text

Write the `translate` call with literals, then add the text to every catalog of that part of the repository, under
its context, translated. The test fails until it is there, and also fails for a catalog entry no source uses any more,
so a text removed from the sources is removed from the catalogs too.

## Tests that check catalogs

Catalogs are kept by hand; [translation_test.cpp](../tests/oui/translation_test.cpp) stands in for a tool that
would extract them:

| Test | Checks |
|------|--------|
| `OuisCatalogsCoverItsTexts` | `oui/translations` against `oui/include` and `oui/src` |
| `OuiCodesCatalogsCoverItsTexts` | `examples/oui_code/translations` against `examples/oui_code` |
| `OuiRunsCatalogsCoverItsTexts` | `examples/oui_run/translations` against `examples/oui_run` |
| `OuiCalcsCatalogsCoverItsTexts` | `examples/oui_calc/translations` against `examples/oui_calc` |
| `OuisBuiltInCatalogsAreTheOnesInTheRepository` | OUI has one built-in catalog, `es`, named `Español` |

The four coverage tests read every `.h` and `.cpp` under the folders, comments blanked out, and every `.oui`
description with a `context=`; `translation.cpp` and `ui.cpp` are skipped, as they define and apply translations
rather than call them with texts. For every `.json` catalog in the translations folder they report:

- a catalog that does not parse, or has no `name`;
- a `translate(` whose arguments are not string literals;
- a text the sources use that the catalog lacks or leaves empty, with the file and line of the call:
  `es.json: untranslated Context "Text" (file.cpp:12)`;
- a translation whose `{n}` placeholders differ from the English;
- a translation that has a `&` mnemonic where the English has none, or the other way round;
- an entry no source uses.

The other tests in the file check the reader (a catalog read, things that are not catalogs rejected), the lookup
order and the English fallback, `untranslate`, `install_language`, `format_text`, and `system_language` from the
environment.
