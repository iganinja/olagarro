#pragma once

#include <oui/completion.h>
#include <oui/script/script_engine.h>
#include <oui/syntax_highlighter.h>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// What a language suggests as one writes: a word, as a keyword, or a piece of text with a name, as a loop.
using Suggestion = olagarro::oui::Suggestion;

// A language OUI Code knows: what files are in it, what starts a comment, which brackets pair, how its text is
// coloured and what it suggests. Scripts define them with oui.languages.register.
struct Language
{
    std::string id;
    std::string name;
    // Lowercase, with the dot: ".cpp".
    std::vector<std::string> extensions;
    // Whole names, such as CMakeLists.txt, matched before extensions.
    std::vector<std::string> file_names;
    std::string comment;
    std::vector<std::pair<char, char>> brackets;
    std::shared_ptr<const olagarro::oui::SyntaxHighlighter> highlighter;
    std::vector<Suggestion> suggestions;
};

// The languages scripts defined, and which one a file is in.
class Languages
{
  public:
    // Adds the language a script's definition describes, replacing one with the same id. Throws
    // olagarro::oui::script::ScriptError saying what is wrong with a definition that does not hold.
    void add(const olagarro::oui::script::Value& definition);
    // The language of `file`, by its name and then its extension; none for a file of no known language.
    [[nodiscard]] std::shared_ptr<const Language> for_path(const std::filesystem::path& file) const;
    [[nodiscard]] std::shared_ptr<const Language> with_id(std::string_view id) const;
    [[nodiscard]] const std::vector<std::shared_ptr<const Language>>& all() const;

  private:
    std::vector<std::shared_ptr<const Language>> m_languages;
};
