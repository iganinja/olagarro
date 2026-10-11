#pragma once

#include "languages.h"

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// What OUI Code suggests while one writes: words of the open documents, what the document's language offers, and,
// in a script, what OUI's scripts can use, as oui/oui.d.ts declares it.

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

// What OUI's scripts can use, from the TypeScript declarations of oui/oui.d.ts: what is under each name, "oui" for
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

  private:
    std::map<std::string, std::vector<Suggestion>, std::less<>> m_members;
    // Each class's own instance members, and the class it derives from.
    std::map<std::string, std::vector<Suggestion>, std::less<>> m_instance_members;
    std::map<std::string, std::string, std::less<>> m_bases;
    // What can be called, by its whole name: "oui.readFile", "oui.Label" for its constructor, "oui.TextEdit#append"
    // for what an instance has.
    std::map<std::string, Signature, std::less<>> m_signatures;
};

// The API's class a document makes `name` as, as "oui.Label" for `label = new oui.Label(...)`; empty for none.
[[nodiscard]] std::string class_made_as(std::string_view name, const std::vector<std::string_view>& documents);

// The suggestions for `writing`, best first: under a name the API declares, what is under it; under a name a
// document makes as one of its classes, `label = new oui.Label(...)`, what such an instance has; otherwise what
// the language and the API offer, then the words of `documents`, those of three letters or more. Those that start
// as the word does, whatever the case; those that start exactly so first. Nothing for nothing written but after a
// dot.
[[nodiscard]] std::vector<Suggestion> suggestions_for(const Writing& writing,
                                                      const std::vector<std::string_view>& documents,
                                                      const Language* language, const ScriptApi* api);
