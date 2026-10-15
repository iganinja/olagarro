#include <oui/script/rule_highlighter.h>
#include <oui/script/script_highlighter.h>
#include <oui/translation.h>

#include <algorithm>
#include <stdexcept>

namespace olagarro::oui::script
{

namespace
{

[[nodiscard]] SyntaxKind kind_of(const Value& value, const std::string& where)
{
    const auto name{value.to_string()};
    const auto kind{syntax_kind_named(name)};
    if(!kind)
    {
        throw ScriptError{format_text(translate("Highlighter", "{0}: unknown kind \"{1}\""), {where, name})};
    }
    return *kind;
}

// A rule as scripts write it: {match: /regex/ or "regex", kind: "keyword", push: "state", pop: true}.
[[nodiscard]] HighlightRule rule_of(const Value& value, const std::string& where)
{
    if(!value.is_object())
    {
        throw ScriptError{format_text(
            translate("Highlighter", "{0} must be an object such as {match: /if/, kind: \"keyword\"}"), {where})};
    }
    HighlightRule rule;
    const auto match{value.get("match")};
    if(match.is_regexp())
    {
        rule.pattern = match.get("source").to_string();
        // Each rule is searched from a place of the highlighter's choosing; g means nothing here.
        for(const auto flag : match.get("flags").to_string())
        {
            if(flag != 'g')
            {
                rule.flags += flag;
            }
        }
    }
    else if(match.is_string())
    {
        rule.pattern = match.to_string();
    }
    else
    {
        throw ScriptError{format_text(translate("Highlighter", "{0} needs match, a regular expression"), {where})};
    }
    if(const auto kind{value.get("kind")}; !kind.is_undefined())
    {
        rule.kind = kind_of(kind, where);
    }
    if(const auto push{value.get("push")}; !push.is_undefined())
    {
        rule.push = push.to_string();
    }
    rule.pop = value.get("pop").to_bool();
    return rule;
}

} // namespace

std::shared_ptr<const SyntaxHighlighter> highlighter_of(const Value& states, const std::string& where)
{
    // States, the first one or "start" being where the text starts: each a list of rules, or
    // {kind: "comment", rules: [...]} for one whose unmatched text is of a kind.
    if(!states.is_object() || states.is_array())
    {
        throw ScriptError{format_text(
            translate("Highlighter", "{0}: states must be an object, one list of rules per state"), {where})};
    }
    std::vector<HighlightState> compiled;
    auto names{states.keys()};
    const auto start{std::find(names.begin(), names.end(), "start")};
    if(start != names.end())
    {
        std::rotate(names.begin(), start, start + 1);
    }
    for(const auto& name : names)
    {
        const auto state{states.get(name)};
        const auto state_where{format_text(translate("Highlighter", "{0}: state {1}"), {where, name})};
        HighlightState highlight_state{name, SyntaxKind::plain, {}};
        auto rules{state};
        if(!state.is_array())
        {
            if(const auto kind{state.get("kind")}; !kind.is_undefined())
            {
                highlight_state.kind = kind_of(kind, state_where);
            }
            rules = state.get("rules");
        }
        if(!rules.is_array())
        {
            throw ScriptError{format_text(translate("Highlighter", "{0} needs a list of rules"), {state_where})};
        }
        for(std::size_t index{}; index < rules.length(); ++ index)
        {
            highlight_state.rules.push_back(
                rule_of(rules.at(index), format_text(translate("Highlighter", "{0}, rule {1}"),
                                                     {state_where, std::to_string(index + 1)})));
        }
        compiled.push_back(std::move(highlight_state));
    }
    try
    {
        return std::make_shared<RuleHighlighter>(std::move(compiled));
    }
    catch(const std::invalid_argument& failure)
    {
        throw ScriptError{where + ": " + failure.what()};
    }
}

} // namespace olagarro::oui::script
