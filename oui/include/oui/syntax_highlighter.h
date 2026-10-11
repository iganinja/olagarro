#pragma once

#include <oui/color.h>

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace olagarro::oui
{

struct Theme;

// What a stretch of source text is, which decides its colour.
enum class SyntaxKind
{
    plain,
    keyword,
    type,
    function,
    string,
    number,
    constant,
    comment,
    preprocessor,
    operator_,
    tag,
    attribute,
    heading,
    emphasis,
    link,
    invalid
};

// The kind a name stands for, as definitions written as text name them: "keyword", "string", "operator"...
[[nodiscard]] std::optional<SyntaxKind> syntax_kind_named(std::string_view name);
// The theme's colour for text of `kind`.
[[nodiscard]] Color syntax_color(const Theme& theme, SyntaxKind kind);

// A stretch of a line of a given kind, as byte offsets within the line; what no span covers is plain.
struct SyntaxSpan
{
    std::size_t start{};
    std::size_t end{};
    SyntaxKind kind{SyntaxKind::plain};
};

// Colours text a line at a time, as a TextEdit shows it. How a line is coloured can depend on the lines
// before it, such as inside a comment that spans lines, through the state each line hands to the next.
class SyntaxHighlighter
{
  public:
    // What a line starts in; the text's first line starts in 0. A highlighter numbers the rest its own way.
    using State = std::size_t;

    virtual ~SyntaxHighlighter() = default;
    // Fills `spans`, in order and not overlapping, with the kinds of `line`, which has no line break, and
    // returns the state the next line starts in.
    virtual State highlight_line(std::string_view line, State state, std::vector<SyntaxSpan>& spans) const = 0;
};

} // namespace olagarro::oui
