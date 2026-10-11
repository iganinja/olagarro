#include <oui/syntax_highlighter.h>

#include <oui/theme.h>

#include <array>
#include <utility>

namespace olagarro::oui
{

std::optional<SyntaxKind> syntax_kind_named(std::string_view name)
{
    static constexpr std::array<std::pair<std::string_view, SyntaxKind>, 16> names{{
        {"plain", SyntaxKind::plain},
        {"keyword", SyntaxKind::keyword},
        {"type", SyntaxKind::type},
        {"function", SyntaxKind::function},
        {"string", SyntaxKind::string},
        {"number", SyntaxKind::number},
        {"constant", SyntaxKind::constant},
        {"comment", SyntaxKind::comment},
        {"preprocessor", SyntaxKind::preprocessor},
        {"operator", SyntaxKind::operator_},
        {"tag", SyntaxKind::tag},
        {"attribute", SyntaxKind::attribute},
        {"heading", SyntaxKind::heading},
        {"emphasis", SyntaxKind::emphasis},
        {"link", SyntaxKind::link},
        {"invalid", SyntaxKind::invalid},
    }};
    for(const auto& [text, kind] : names)
    {
        if(text == name)
        {
            return kind;
        }
    }
    return std::nullopt;
}

Color syntax_color(const Theme& theme, SyntaxKind kind)
{
    switch(kind)
    {
    case SyntaxKind::plain:
        return theme.editor_foreground;
    case SyntaxKind::keyword:
        return theme.syntax_keyword;
    case SyntaxKind::type:
        return theme.syntax_type;
    case SyntaxKind::function:
        return theme.syntax_function;
    case SyntaxKind::string:
        return theme.syntax_string;
    case SyntaxKind::number:
    case SyntaxKind::constant:
        return theme.syntax_number;
    case SyntaxKind::comment:
        return theme.syntax_comment;
    case SyntaxKind::preprocessor:
        return theme.syntax_preprocessor;
    case SyntaxKind::operator_:
        return theme.syntax_operator;
    case SyntaxKind::tag:
        return theme.syntax_type;
    case SyntaxKind::attribute:
        return theme.syntax_preprocessor;
    case SyntaxKind::heading:
        return theme.heading_foreground;
    case SyntaxKind::emphasis:
        return theme.syntax_keyword;
    case SyntaxKind::link:
        return theme.link_foreground;
    case SyntaxKind::invalid:
        return theme.error_foreground;
    }
    return theme.editor_foreground;
}

} // namespace olagarro::oui
