#include <oui/script/rule_highlighter.h>
#include <oui/translation.h>

#include <stdexcept>

namespace olagarro::oui::script
{

namespace
{

// How deep states may nest, and how often a line may change state without moving on, before the rules are
// taken to go round in circles and the rest of the line is left as it is.
constexpr std::size_t deepest_nesting{64};
constexpr int most_changes_in_place{32};

} // namespace

RuleHighlighter::RuleHighlighter(std::vector<HighlightState> states)
{
    if(states.empty())
    {
        throw std::invalid_argument{translate("Highlighter", "a highlighter needs at least one state")};
    }
    std::map<std::string, std::size_t> indices;
    for(std::size_t index{}; index < states.size(); ++ index)
    {
        indices[states[index].name] = index;
    }
    for(const auto& state : states)
    {
        CompiledState compiled{state.kind, {}};
        for(const auto& rule : state.rules)
        {
            std::optional<std::size_t> push;
            if(!rule.push.empty())
            {
                const auto found{indices.find(rule.push)};
                if(found == indices.end())
                {
                    throw std::invalid_argument{
                        format_text(translate("Highlighter", "state '{0}' enters '{1}', which does not exist"),
                                    {state.name, rule.push})};
                }
                push = found->second;
            }
            compiled.rules.push_back({Regex{rule.pattern, rule.flags}, rule.kind, push, rule.pop});
        }
        m_states.push_back(std::move(compiled));
    }
}

SyntaxHighlighter::State RuleHighlighter::id_of(const std::vector<std::size_t>& stack) const
{
    const auto found{m_ids.find(stack)};
    if(found != m_ids.end())
    {
        return found->second;
    }
    const auto id{m_stacks.size()};
    m_stacks.push_back(stack);
    m_ids.emplace(stack, id);
    return id;
}

SyntaxHighlighter::State RuleHighlighter::highlight_line(std::string_view line, State state,
                                                         std::vector<SyntaxSpan>& spans) const
{
    auto stack{m_stacks.at(state)};
    const Utf16Text text{line};
    const auto length{text.units.size()};
    // Spans of the same kind side by side become one; plain text needs none.
    const auto emit{[&spans, &text](std::size_t start, std::size_t end, SyntaxKind kind)
                    {
                        if(start >= end || kind == SyntaxKind::plain)
                        {
                            return;
                        }
                        const auto byte_start{text.byte_offsets[start]};
                        const auto byte_end{text.byte_offsets[end]};
                        if(!spans.empty() && spans.back().kind == kind && spans.back().end == byte_start)
                        {
                            spans.back().end = byte_end;
                            return;
                        }
                        spans.push_back({byte_start, byte_end, kind});
                    }};
    // Each rule's next match from where the line has got to, found again only once the line passes it.
    std::vector<std::optional<std::pair<std::size_t, std::size_t>>> next;
    std::vector<bool> searched;
    std::size_t position{};
    int changes_in_place{};
    // Reaching the end of the line goes round once more: a rule for $ can still change state there.
    while(position <= length)
    {
        const auto& current{m_states[stack.back()]};
        if(searched.empty())
        {
            next.assign(current.rules.size(), std::nullopt);
            searched.assign(current.rules.size(), false);
        }
        std::optional<std::size_t> best;
        for(std::size_t index{}; index < current.rules.size(); ++ index)
        {
            auto& found{next[index]};
            const auto& rule{current.rules[index]};
            // A match of nothing that changes nothing would hold the line in place: look past it.
            while(!searched[index] || (found && found->first < position) ||
                  (found && found->first == found->second && !rule.push && !rule.pop))
            {
                const auto from{!searched[index] || !found || found->first < position ? position : found->first + 1};
                searched[index] = true;
                found = rule.regex.search(text.units, from);
                if(!found)
                {
                    break;
                }
            }
            if(found && (!best || found->first < next[*best]->first))
            {
                best = index;
            }
        }
        if(!best)
        {
            emit(position, length, current.kind);
            break;
        }
        const auto [start, end]{*next[*best]};
        const auto& rule{current.rules[*best]};
        emit(position, start, current.kind);
        const auto entered{rule.push ? m_states[*rule.push].kind : current.kind};
        emit(start, end, rule.kind.value_or(entered));
        position = end;
        if(rule.pop && stack.size() > 1)
        {
            stack.pop_back();
        }
        if(rule.push && stack.size() < deepest_nesting)
        {
            stack.push_back(*rule.push);
        }
        if(rule.push || rule.pop)
        {
            // Another state, other rules.
            searched.clear();
        }
        if(start == end)
        {
            if(++ changes_in_place > most_changes_in_place)
            {
                emit(position, length, m_states[stack.back()].kind);
                break;
            }
        }
        else
        {
            changes_in_place = 0;
        }
    }
    return id_of(stack);
}

} // namespace olagarro::oui::script
