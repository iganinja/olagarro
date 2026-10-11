#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace olagarro::oui
{

// A user interface described as text, a widget a line, those inside one indented under it:
//
//     Window "Find in Files" context=FindInFiles width=60
//       Label "&Find:" buddy=query
//       LineEdit id=query placeholder="Text to find"
//       Row centered
//         Button "OK" id=ok default
//
// A line is the widget's type, its text in double quotes when it has one, right after the type, and its properties:
// name=value, the value in double quotes when it has spaces, or a name alone for one that is on. \" and \\ write a
// quote and a backslash in a quoted text, and \n a line break. A line that starts with # is a comment; indentation is
// spaces only.

struct DeclarativeNode
{
    std::string type;
    std::optional<std::string> text;
    std::vector<std::pair<std::string, std::string>> properties;
    // Where each property's name is written, from 1, in the order of `properties`; empty for a node not read from
    // text.
    std::vector<std::size_t> property_columns;
    std::vector<DeclarativeNode> children;
    // Where it is written, both from 1.
    std::size_t line{};
    std::size_t column{};

    // A property's value; "" for one given alone; none when it is not given.
    [[nodiscard]] std::optional<std::string_view> property(std::string_view name) const;
    [[nodiscard]] bool has(std::string_view name) const;
};

struct ParseDiagnostic
{
    std::size_t line;
    std::size_t column;
    std::string message;
};

struct ParseResult
{
    std::optional<DeclarativeNode> root;
    std::vector<ParseDiagnostic> diagnostics;

    [[nodiscard]] bool succeeded() const;
};

[[nodiscard]] ParseResult parse_oui(std::string_view source);

} // namespace olagarro::oui
