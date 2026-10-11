#pragma once

#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace olagarro::oui
{

// Translations grouped by context: context, then English text, then its translation. The context lets the
// same English word read differently in two places. An empty translation means nobody has translated
// that text yet.
using TranslationTexts = std::map<std::string, std::map<std::string, std::string, std::less<>>, std::less<>>;

// The translations of one language as a JSON file keeps them:
//
//     {
//       "language": "es",
//       "name": "Español",
//       "contexts": {
//         "FileDialog": { "Cancel": "Cancelar", "Open": "Abrir" }
//       }
//     }
//
// Every text the sources pass to translate() goes under its context, with its translation; a text the
// catalog leaves out or leaves empty shows in English.
struct TranslationCatalog
{
    // A language code such as "es".
    std::string language;
    // The language's name in that language, such as "Español", for a language chooser.
    std::string name;
    TranslationTexts contexts;
};

// Nothing when the text is not such a catalog; `error` then says why and where.
[[nodiscard]] std::optional<TranslationCatalog> parse_translation_catalog(std::string_view json, std::string& error);

// A catalog ready for looking texts up.
class Translation
{
  public:
    explicit Translation(TranslationCatalog catalog);

    [[nodiscard]] const std::string& language() const;
    [[nodiscard]] const std::string& name() const;
    // Nothing when the catalog has no translation for the text.
    [[nodiscard]] const std::string* find(std::string_view context, std::string_view text) const;
    // The English text whose translation `translated` is, the other way round; nothing when there is none.
    [[nodiscard]] const std::string* source_of(std::string_view context, std::string_view translated) const;

  private:
    TranslationCatalog m_catalog;
};

// Catalogs are looked up newest first, so an application's catalog can override OUI's own. They are meant
// to be installed once, before the interface is built: widgets take their texts when they are created,
// and the installed list is not guarded for use from several threads.
void install_translation(Translation translation);
void remove_translations();
// The language of the catalog installed last, which the texts come in: "en" when none is, as the sources write them.
[[nodiscard]] std::string installed_language();
// The translation of a text in the installed catalogs, or the text itself. Pass string literals, so a
// search of the sources finds every text a catalog has to cover.
[[nodiscard]] std::string translate(std::string_view context, std::string_view text);
// The English text a translation in the installed catalogs came from, or the text itself: what the sources call
// something shown in another language, as a menu a user names in either.
[[nodiscard]] std::string untranslate(std::string_view context, std::string_view text);

// The text among `texts` in `language`: the one whose file, at the same place in `names`, is named as the first's
// with the language before its extension, "help.es.md" beside "help.md"; the first, English, when there is none. For
// texts kept a file per language, as help, built in with olagarro_embed_files and its _names().
[[nodiscard]] std::string_view text_for_language(const std::vector<std::string_view>& texts,
                                                 const std::vector<std::string_view>& names, std::string_view language);

// Replaces {0}, {1}... with the arguments, so a translation can put them in another order:
// format_text(translate("Editor", "Could not open {0}: {1}"), {path, reason}). A placeholder without an
// argument stays as it is.
[[nodiscard]] std::string format_text(std::string_view pattern, std::initializer_list<std::string_view> arguments);

// The catalogs olagarro_embed_files built into a program. A catalog that does not parse is a broken
// build, so this throws std::runtime_error saying which one and why.
[[nodiscard]] std::vector<Translation> embedded_translations(const std::vector<std::string_view>& catalogs);
// OUI's own catalogs, built into the library.
[[nodiscard]] const std::vector<Translation>& oui_translations();
// Installs OUI's catalog and the application's for `language`, replacing whatever was installed. English,
// or a language without catalogs, leaves the texts as the sources write them.
void install_language(std::string_view language, const std::vector<Translation>& application_translations);
// The user's interface language as a code such as "es": from the environment on POSIX and the user's
// locale on Windows; "en" when they say nothing.
[[nodiscard]] std::string system_language();

} // namespace olagarro::oui
