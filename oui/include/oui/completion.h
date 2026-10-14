#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace olagarro::oui
{

// What an editor suggests while one writes: words of the open documents, what the document's language offers, and,
// in a script, what an API declares, as OUI Code's oui.d.ts declares what its scripts can use.

// Something to suggest: a word, as a keyword, or a piece of text with a name, as a loop.
struct Suggestion
{
    std::string label;
    // What kind it is, shown beside it.
    std::string detail;
    // What goes in; the label when empty. "$0" in it, if anywhere, is where the cursor goes after.
    std::string text;
};

// What the cursor is writing: the word before it, from `start`, and the names and dots before that word, as "oui" in
// "oui.rea", when there are.
struct Writing
{
    std::size_t start{};
    std::string prefix;
    std::string qualifier;
};

[[nodiscard]] Writing writing_at(std::string_view text, std::size_t cursor);

// A call the cursor is in the brackets of, as in "oui.readFile(pa": where its "(" is, the name called and the names
// and dots before it, whether it is `new`, and which argument the cursor is in, from 0.
struct Call
{
    std::size_t open{};
    std::string name;
    std::string qualifier;
    bool made{};
    std::size_t argument{};
};

// The innermost call the cursor is in, past strings and comments; none outside every call.
[[nodiscard]] std::optional<Call> call_at(std::string_view text, std::size_t cursor);

// How something of the API is called, as "readFile(path: string): string | null", where each parameter is in it,
// and what it does.
struct Signature
{
    std::string text;
    std::vector<std::pair<std::size_t, std::size_t>> parameters;
    std::vector<std::string> doc;

    // The parameter the argument `argument` goes to: the last when it takes any number of them, as console.log.
    [[nodiscard]] std::optional<std::size_t> parameter_of(std::size_t argument) const;
};

// What scripts can use, from TypeScript declarations, as OUI Code's oui.d.ts: what is under each name, "oui" for
// oui's own, "" for what is there without a name before it, as console and setTimeout.
class ScriptApi
{
  public:
    explicit ScriptApi(std::string_view declarations);

    // What is under `qualifier`; none when it names nothing declared.
    [[nodiscard]] const std::vector<Suggestion>* members(std::string_view qualifier) const;
    // What an instance of the class `name`, as "oui.Label", has, with what it has of the classes it derives from;
    // none for no such class.
    [[nodiscard]] std::vector<Suggestion> instance_members(std::string_view name) const;
    // How `call` is called, when it calls something the API declares: a function under a name, a class made with
    // `new`, or what an instance of a class a document makes has, as suggestions_for finds it.
    [[nodiscard]] std::optional<Signature> signature(const Call& call,
                                                     const std::vector<std::string_view>& documents) const;

    // How what the API declares is called and what it does, by its whole name: "oui.readFile" for a function,
    // "oui.Label" for a class's constructor, "oui.TextEdit#append" for what an instance has, a field too, whose text
    // is its name and type, as "width: number". None for what it does not declare.
    [[nodiscard]] const Signature* declared(std::string_view whole) const;

    // The API's class `name` is in the documents, as far as they tell: made with `new`, as `label = new oui.Label(`;
    // given what a function or method of the API returns, as `soft = image.gaussianBlur(4)` when gaussianBlur is
    // declared to return a class; or a parameter of a function written with the name of a method a class declares,
    // as `image` in `render(image, values) {` when a class has `render(image: Image, ...)`. Empty when they do not.
    [[nodiscard]] std::string type_of(std::string_view name, const std::vector<std::string_view>& documents) const;

  private:
    [[nodiscard]] std::string type_of(std::string_view name, const std::vector<std::string_view>& documents,
                                      int depth) const;
    // The class `type` names as written where `scope` is, looked for from there outwards; empty for none.
    [[nodiscard]] std::string resolved(std::string_view type, std::string_view scope) const;
    // The class what `key`, as m_signatures names it, returns; empty when it returns no class.
    [[nodiscard]] std::string returned(const std::string& key) const;

    std::map<std::string, std::vector<Suggestion>, std::less<>> m_members;
    // Each class's own instance members, and the class it derives from.
    std::map<std::string, std::vector<Suggestion>, std::less<>> m_instance_members;
    std::map<std::string, std::string, std::less<>> m_bases;
    // What can be called, by its whole name: "oui.readFile", "oui.Label" for its constructor, "oui.TextEdit#append"
    // for what an instance has.
    std::map<std::string, Signature, std::less<>> m_signatures;
    // The fields of classes, as declared tells them.
    std::map<std::string, Signature, std::less<>> m_fields;
    // Where each of them is declared, the namespace its types are written from.
    std::map<std::string, std::string, std::less<>> m_scopes;
    // Every class, by its whole name.
    std::set<std::string, std::less<>> m_classes;
};

// The API's class a document makes `name` as, as "oui.Label" for `label = new oui.Label(...)`; empty for none.
[[nodiscard]] std::string class_made_as(std::string_view name, const std::vector<std::string_view>& documents);

// The suggestions for `writing`, best first: under a name the API declares, what is under it; under a name a
// document makes as one of its classes, `label = new oui.Label(...)`, what such an instance has; otherwise what
// what `language` and the API offer, then the words of `documents`, those of three letters or more. Those that start
// as the word does, whatever the case; those that start exactly so first. Nothing for nothing written but after a
// dot.
[[nodiscard]] std::vector<Suggestion> suggestions_for(const Writing& writing,
                                                      const std::vector<std::string_view>& documents,
                                                      const std::vector<Suggestion>* language, const ScriptApi* api);

} // namespace olagarro::oui
