#pragma once

#include <oui/regex.h>
#include <oui/syntax_highlighter.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace olagarro::oui::script
{

// A rule of a highlighting state: text the pattern matches is of `kind`, and can enter another state or
// leave this one. Without a kind it takes the kind of the state it enters, or of the state it is in.
struct HighlightRule
{
    std::string pattern;
    std::string flags;
    std::optional<SyntaxKind> kind;
    // The state to enter after the match, by name.
    std::string push;
    // Leaves this state for the one it was entered from.
    bool pop{false};
};

// Where the text is, such as inside a string or a comment: the rules that apply there, and the kind of the
// text no rule matches.
struct HighlightState
{
    std::string name;
    SyntaxKind kind{SyntaxKind::plain};
    std::vector<HighlightRule> rules;
};

// Colours text by rules, as definitions of languages describe them: at each place the rule whose pattern
// matches first wins, the earliest in the list on a tie. States nest, so a string can open inside a
// template inside a string; the stack of states is what a line hands to the next.
class RuleHighlighter final : public SyntaxHighlighter
{
  public:
    // The text starts in `states.front()`. Throws std::invalid_argument for a pattern that does not compile
    // or a push to a state that does not exist.
    explicit RuleHighlighter(std::vector<HighlightState> states);

    State highlight_line(std::string_view line, State state, std::vector<SyntaxSpan>& spans) const override;

  private:
    struct CompiledRule
    {
        Regex regex;
        std::optional<SyntaxKind> kind;
        std::optional<std::size_t> push;
        bool pop;
    };
    struct CompiledState
    {
        SyntaxKind kind;
        std::vector<CompiledRule> rules;
    };

    [[nodiscard]] State id_of(const std::vector<std::size_t>& stack) const;

    std::vector<CompiledState> m_states;
    // Every stack of states a line has handed on, numbered as the State the next line starts in.
    mutable std::vector<std::vector<std::size_t>> m_stacks{{0}};
    mutable std::map<std::vector<std::size_t>, State> m_ids{{{0}, 0}};
};

} // namespace olagarro::oui::script
