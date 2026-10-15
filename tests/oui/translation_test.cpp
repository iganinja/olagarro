#include <oui/declarative.h>
#include <oui/ui.h>
#include <oui/command.h>
#include <oui/translation.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{

using namespace olagarro;
using namespace olagarro::oui;

TranslationCatalog parse(const std::string& json)
{
    std::string error;
    auto catalog{parse_translation_catalog(json, error)};
    EXPECT_TRUE(catalog.has_value()) << error;
    return catalog ? std::move(*catalog) : TranslationCatalog{};
}

std::string parse_error(const std::string& json)
{
    std::string error;
    EXPECT_FALSE(parse_translation_catalog(json, error).has_value());
    return error;
}

// Leaves no catalog installed behind, whatever a test installs.
class OuiTranslationTests : public testing::Test
{
  protected:
    void TearDown() override { remove_translations(); }
};

TEST_F(OuiTranslationTests, ReadsACatalog)
{
    const auto catalog{parse(R"({
        "language": "es",
        "name": "Espa\u00f1ol",
        "contexts": {
            "FileDialog": { "Open": "Abrir", "Save": "" },
            "Editor": { "Line\nbreak \"quoted\"": "L\u00ednea \ud83d\ude00" }
        }
    })")};
    EXPECT_EQ(catalog.language, "es");
    EXPECT_EQ(catalog.name, "Español");
    EXPECT_EQ(catalog.contexts.at("FileDialog").at("Open"), "Abrir");
    EXPECT_EQ(catalog.contexts.at("Editor").at("Line\nbreak \"quoted\""), "Línea \U0001F600");
}

TEST_F(OuiTranslationTests, RejectsWhatIsNotACatalog)
{
    EXPECT_EQ(parse_error(R"({"name": "x"})"), "line 1, column 14: the catalog does not say its \"language\"");
    EXPECT_NE(parse_error(R"({"language": "es", "contexts": {"A": {"b": 1}}})").find("expected a string"),
              std::string::npos);
    EXPECT_NE(parse_error(R"({"language": "es", "version": "1"})").find("unknown key \"version\""), std::string::npos);
    EXPECT_NE(parse_error("{\"language\": \"es\"} x").find("text after the catalog"), std::string::npos);
    EXPECT_NE(parse_error("{\"language\": \"es\",\n  \"name\": \"\\q\"}").find("line 2, column 12: an unknown escape"),
              std::string::npos);
    EXPECT_NE(parse_error(R"({"language": "\ud83d"})").find("surrogate"), std::string::npos);
    EXPECT_NE(parse_error(R"({"language": "es")").find("expected ',' or '}'"), std::string::npos);
}

TEST_F(OuiTranslationTests, TranslatesWithTheNewestCatalogFirstAndFallsBackToEnglish)
{
    auto library{parse(R"({"language": "es", "contexts": {"Dialogs": {"OK": "Aceptar", "Cancel": "Cancelar"}}})")};
    auto application{parse(R"({"language": "es", "contexts": {"Dialogs": {"OK": "Vale", "Cancel": ""}}})")};
    EXPECT_EQ(translate("Dialogs", "OK"), "OK");
    install_translation(Translation{std::move(library)});
    install_translation(Translation{std::move(application)});
    EXPECT_EQ(translate("Dialogs", "OK"), "Vale");
    // An empty translation is an untranslated text: the older catalog still gets its turn.
    EXPECT_EQ(translate("Dialogs", "Cancel"), "Cancelar");
    EXPECT_EQ(translate("Dialogs", "Help"), "Help");
    EXPECT_EQ(translate("Elsewhere", "OK"), "OK");
    remove_translations();
    EXPECT_EQ(translate("Dialogs", "OK"), "OK");
}

TEST_F(OuiTranslationTests, FindsTheEnglishATranslationCameFrom)
{
    auto library{parse(R"({"language": "es", "contexts": {"Menu": {"&Tools": "&Herramientas", "&View": ""}}})")};
    auto application{parse(R"({"language": "es", "contexts": {"Menu": {"&Tools": "Ú&tiles"}}})")};
    EXPECT_EQ(untranslate("Menu", "&Herramientas"), "&Herramientas");
    install_translation(Translation{std::move(library)});
    install_translation(Translation{std::move(application)});
    EXPECT_EQ(untranslate("Menu", "Ú&tiles"), "&Tools");
    EXPECT_EQ(untranslate("Menu", "&Herramientas"), "&Tools");
    // Not a translation, as an untranslated text is not: the text itself.
    EXPECT_EQ(untranslate("Menu", "&View"), "&View");
    EXPECT_EQ(untranslate("Menu", ""), "");
    EXPECT_EQ(untranslate("Elsewhere", "Ú&tiles"), "Ú&tiles");
}

// Help texts and the like are a file for each language, named as English's with the language before its extension.
TEST_F(OuiTranslationTests, PicksTheTextOfALanguageByTheNameOfItsFile)
{
    const std::vector<std::string_view> texts{"English", "Español", "Français"};
    const std::vector<std::string_view> names{"help.md", "help.es.md", "help.fr.md"};
    EXPECT_EQ(text_for_language(texts, names, "fr"), "Français");
    EXPECT_EQ(text_for_language(texts, names, "es"), "Español");
    EXPECT_EQ(text_for_language(texts, names, "en"), "English");
    EXPECT_EQ(text_for_language(texts, names, "de"), "English") << "English when there is none in the language";
}

TEST_F(OuiTranslationTests, InstallsALanguageFromOuiAndTheApplication)
{
    std::vector<Translation> application;
    application.emplace_back(parse(R"({"language": "es", "contexts": {"Editor": {"Save": "Guardar"}}})"));
    application.emplace_back(parse(R"({"language": "fr", "contexts": {"Editor": {"Save": "Enregistrer"}}})"));
    install_language("es", application);
    EXPECT_EQ(translate("Editor", "Save"), "Guardar");
    EXPECT_EQ(translate("Dialogs", "Cancel"), "Cancelar");
    EXPECT_EQ(format_shortcut({U's', true, false, true}), "Ctrl+Mayús+S");
    install_language("en", application);
    EXPECT_EQ(translate("Editor", "Save"), "Save");
    EXPECT_EQ(translate("Dialogs", "Cancel"), "Cancel");
}

TEST_F(OuiTranslationTests, SaysWhatIsWrongWithADescriptionInTheInterfaceLanguage)
{
    install_language("es", {});
    EXPECT_EQ(parse_oui("label").diagnostics.front().message, "Se esperaba un tipo de widget, como Label o Button.");
    try
    {
        static_cast<void>(build_ui("Window\n  Slider\n", UiTexts::as_written));
        ADD_FAILURE() << "a Slider is no widget";
    }
    catch(const UiError& error)
    {
        EXPECT_NE(std::string{error.what()}.find("No hay ningún widget llamado Slider."), std::string::npos)
            << error.what();
    }
}

TEST_F(OuiTranslationTests, FormatsPlaceholdersInAnyOrder)
{
    EXPECT_EQ(format_text("Could not open {0}: {1}", {"a.txt", "no access"}), "Could not open a.txt: no access");
    EXPECT_EQ(format_text("{1} ({0})", {"a", "b"}), "b (a)");
    EXPECT_EQ(format_text("{0}{0}", {"x"}), "xx");
    EXPECT_EQ(format_text("{2} {name} {", {"a"}), "{2} {name} {");
    EXPECT_EQ(format_text("", {"a"}), "");
}

#ifndef _WIN32
TEST_F(OuiTranslationTests, ReadsTheLanguageFromTheEnvironment)
{
    const std::vector<const char*> variables{"LANGUAGE", "LC_ALL", "LC_MESSAGES", "LANG"};
    std::vector<std::pair<const char*, std::optional<std::string>>> saved;
    for(const auto* const variable : variables)
    {
        const auto* const value{std::getenv(variable)};
        saved.emplace_back(variable, value ? std::optional<std::string>{value} : std::nullopt);
        unsetenv(variable);
    }
    EXPECT_EQ(system_language(), "en");
    setenv("LANG", "es_ES.UTF-8", 1);
    EXPECT_EQ(system_language(), "es");
    setenv("LC_MESSAGES", "C", 1);
    EXPECT_EQ(system_language(), "en");
    setenv("LANGUAGE", "fr:en", 1);
    EXPECT_EQ(system_language(), "fr");
    setenv("LANGUAGE", "pt_BR@latin", 1);
    EXPECT_EQ(system_language(), "pt");
    for(const auto& [variable, value] : saved)
    {
        if(value)
        {
            setenv(variable, value->c_str(), 1);
        }
        else
        {
            unsetenv(variable);
        }
    }
}
#endif

// Catalogs are kept by hand, so these checks stand in for a tool: every text the sources pass to
// translate() must be in every catalog of that part of the repository, translated, and nothing else.

struct SourceText
{
    std::string context;
    std::string text;
    std::string where;
};

std::string read_file(const std::filesystem::path& path)
{
    std::ifstream file{path, std::ios::binary};
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

// The source with comments blanked out, string literals kept.
std::string without_comments(const std::string& source)
{
    std::string result{source};
    for(std::size_t i{}; i < result.size(); ++ i)
    {
        if(result[i] == '"' || result[i] == '\'')
        {
            const auto quote{result[i]};
            for(++ i; i < result.size() && result[i] != quote; ++ i)
            {
                if(result[i] == '\\')
                {
                    ++ i;
                }
            }
        }
        else if(result.compare(i, 2, "//") == 0)
        {
            for(; i < result.size() && result[i] != '\n'; ++ i)
            {
                result[i] = ' ';
            }
        }
        else if(result.compare(i, 2, "/*") == 0)
        {
            const auto end{result.find("*/", i + 2)};
            const auto stop{end == std::string::npos ? result.size() : end + 2};
            for(; i < stop; ++ i)
            {
                result[i] = result[i] == '\n' ? '\n' : ' ';
            }
            -- i;
        }
    }
    return result;
}

std::string unescape_literals(const std::string& literals)
{
    std::string result;
    bool inside{false};
    for(std::size_t i{}; i < literals.size(); ++ i)
    {
        const auto c{literals[i]};
        if(c == '"')
        {
            inside = !inside;
        }
        else if(inside && c == '\\' && i + 1 < literals.size())
        {
            const auto escaped{literals[++ i]};
            result += escaped == 'n' ? '\n' : escaped == 't' ? '\t' : escaped;
        }
        else if(inside)
        {
            result += c;
        }
    }
    return result;
}

// The texts passed to translate() under `folders`. A call whose arguments are not string literals is
// reported in `problems`, since no catalog check could see its text.
std::vector<SourceText> source_texts(const std::vector<std::filesystem::path>& folders,
                                     std::vector<std::string>& problems)
{
    static const std::regex call{R"re(\btranslate\s*\()re"};
    static const std::regex literal_call{
        R"re(^translate\s*\(\s*"((?:[^"\\]|\\.)*)"\s*,\s*((?:"(?:[^"\\]|\\.)*"\s*)+)\))re"};
    std::vector<SourceText> texts;
    for(const auto& folder : folders)
    {
        for(const auto& entry : std::filesystem::recursive_directory_iterator{folder})
        {
            const auto extension{entry.path().extension()};
            // A user interface's description: its texts, under the context its root names. One that names none is a
            // script's, whose texts show as they are written.
            if(extension == ".oui")
            {
                const auto parsed{oui::parse_oui(read_file(entry.path()))};
                if(!parsed.succeeded())
                {
                    problems.push_back(entry.path().filename().string() + ": " + parsed.diagnostics.front().message);
                    continue;
                }
                if(!parsed.root->has("context"))
                {
                    continue;
                }
                for(const auto& text : oui::ui_texts(*parsed.root))
                {
                    texts.push_back(
                        {text.context, text.text, entry.path().filename().string() + ":" + std::to_string(text.line)});
                }
                continue;
            }
            // translation.cpp defines translate().
            if(entry.path().stem() == "translation" || (extension != ".h" && extension != ".cpp"))
            {
                continue;
            }
            const auto source{without_comments(read_file(entry.path()))};
            for(auto found{std::sregex_iterator{source.begin(), source.end(), call}}; found != std::sregex_iterator{};
                ++ found)
            {
                const auto offset{static_cast<std::size_t>(found->position())};
                const auto line{1 +
                                std::count(source.begin(), source.begin() + static_cast<std::ptrdiff_t>(offset), '\n')};
                const auto where{entry.path().filename().string() + ":" + std::to_string(line)};
                std::smatch match;
                const std::string rest{source.substr(offset, 2000)};
                if(!std::regex_search(rest, match, literal_call))
                {
                    // ui.cpp translates the texts of descriptions, which are read from the .oui files themselves,
                    // above.
                    if(entry.path().filename() != "ui.cpp")
                    {
                        problems.push_back(where + ": translate() without string literals");
                    }
                    continue;
                }
                texts.push_back(
                    {unescape_literals("\"" + match[1].str() + "\""), unescape_literals(match[2].str()), where});
            }
        }
    }
    return texts;
}

std::set<std::string> placeholders(const std::string& text)
{
    static const std::regex placeholder{R"(\{[0-9]+\})"};
    std::set<std::string> result;
    for(auto found{std::sregex_iterator{text.begin(), text.end(), placeholder}}; found != std::sregex_iterator{};
        ++ found)
    {
        result.insert(found->str());
    }
    return result;
}

void expect_catalogs_cover_sources(const std::filesystem::path& catalogs,
                                   const std::vector<std::filesystem::path>& sources)
{
    std::vector<std::string> problems;
    const auto texts{source_texts(sources, problems)};
    EXPECT_FALSE(texts.empty());
    std::size_t catalog_count{};
    for(const auto& entry : std::filesystem::directory_iterator{catalogs})
    {
        if(entry.path().extension() != ".json")
        {
            continue;
        }
        ++ catalog_count;
        const auto file{entry.path().filename().string()};
        std::string error;
        const auto catalog{parse_translation_catalog(read_file(entry.path()), error)};
        if(!catalog)
        {
            problems.push_back(file + ": " + error);
            continue;
        }
        if(catalog->name.empty())
        {
            problems.push_back(file + ": no \"name\" for the language chooser");
        }
        std::set<std::pair<std::string, std::string>> used;
        for(const auto& text : texts)
        {
            used.insert({text.context, text.text});
            const auto context{catalog->contexts.find(text.context)};
            const auto entry_found{context == catalog->contexts.end() ? nullptr : [&]() -> const std::string*
                                   {
                                       const auto found{context->second.find(text.text)};
                                       return found == context->second.end() ? nullptr : &found->second;
                                   }()};
            const auto quoted{text.context + " \"" + text.text + "\" (" + text.where + ")"};
            if(entry_found == nullptr || entry_found->empty())
            {
                problems.push_back(file + ": untranslated " + quoted);
                continue;
            }
            if(placeholders(*entry_found) != placeholders(text.text))
            {
                problems.push_back(file + ": other placeholders in " + quoted);
            }
            if((mnemonic(*entry_found) == 0) != (mnemonic(text.text) == 0))
            {
                problems.push_back(file + ": the '&' mnemonic comes and goes in " + quoted);
            }
        }
        for(const auto& [context, entries] : catalog->contexts)
        {
            for(const auto& [text, translation] : entries)
            {
                if(used.count({context, text}) == 0)
                {
                    problems.push_back(file + ": no source uses " + context + " \"" + text + "\"");
                }
            }
        }
    }
    EXPECT_GT(catalog_count, 0u);
    std::string report;
    for(const auto& problem : problems)
    {
        report += problem + "\n";
    }
    EXPECT_TRUE(problems.empty()) << report;
}

const std::filesystem::path source_dir{OLAGARRO_SOURCE_DIR};

TEST_F(OuiTranslationTests, OuisCatalogsCoverItsTexts)
{
    expect_catalogs_cover_sources(source_dir / "oui/translations",
                                  {source_dir / "oui/include", source_dir / "oui/src"});
}

TEST_F(OuiTranslationTests, OuisBuiltInCatalogsAreTheOnesInTheRepository)
{
    ASSERT_EQ(oui_translations().size(), 1u);
    EXPECT_EQ(oui_translations().front().language(), "es");
    EXPECT_EQ(oui_translations().front().name(), "Español");
}

} // namespace
