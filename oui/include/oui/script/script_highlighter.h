#pragma once

#include <oui/script/script_engine.h>
#include <oui/syntax_highlighter.h>

#include <memory>
#include <string>

namespace olagarro::oui::script
{

// A highlighter from the states a script colours a language with, OUI Code's languages and oui_run's text edits
// alike: {start: [{match: /if/, kind: "keyword"}, {match: /"/, push: "string"}], string: {kind: "string", rules:
// [...]}}. The text starts in "start", or in the first state when there is none so named. Throws ScriptError,
// beginning with `where`, for states written otherwise.
[[nodiscard]] std::shared_ptr<const SyntaxHighlighter> highlighter_of(const Value& states, const std::string& where);

} // namespace olagarro::oui::script
