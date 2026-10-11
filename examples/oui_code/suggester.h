#pragma once

#include "completion.h"
#include "signature_tip.h"

#include <oui/application.h>
#include <oui/popup_list.h>
#include <oui/text_edit.h>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The list of suggestions at the cursor: it opens as one writes, when the word has two letters or after a dot,
// or when asked; it follows what is written, and goes when the cursor leaves the word. Choosing puts the suggestion
// in place of the word. Asked with nothing to suggest, it says so for a moment, so it does not look broken. In a
// script, while the list is not open, the cursor in the brackets of a call the API declares shows how it is called.
class Suggester
{
  public:
    // What it suggests in: the document on show, its language, whether it is a script of OUI's, and the text of
    // every open document. None when what is on show is no document.
    struct Context
    {
        olagarro::oui::TextEdit* editor{};
        const Language* language{};
        bool script{};
        std::vector<std::string_view> documents;
    };

    Suggester(olagarro::oui::Application& application, std::function<std::optional<Context>()> context,
              const ScriptApi& api);
    Suggester(const Suggester&) = delete;
    Suggester& operator=(const Suggester&) = delete;

    // Whether it opens by itself as one writes; asking always opens it.
    void set_as_you_type(bool as_you_type);
    // After each event, once handled: `typed` is what was typed, when something was, to open the list as one writes;
    // an open list follows the word or goes.
    void after_event(std::optional<char32_t> typed);
    // Asked for, as with Ctrl+Space.
    void suggest();
    [[nodiscard]] bool is_open() const;

  private:
    void show(const Context& context, std::size_t start, std::vector<Suggestion> suggestions);
    void say_nothing(const Context& context, std::size_t start);
    void choose(std::size_t index);
    void close();
    // The list, as one writes a word: it follows it, or opens as it is written.
    void follow_word(const Context& context, std::optional<char32_t> typed);
    // The signature of the call the cursor is in, shown or followed; closed outside every call.
    void follow_call(const Context& context);
    void close_signature();

    olagarro::oui::Application& m_application;
    std::function<std::optional<Context>()> m_context;
    const ScriptApi& m_api;
    bool m_as_you_type{true};
    // The document the list is for, which may close while it is open.
    olagarro::oui::WidgetRef<olagarro::oui::TextEdit> m_editor;
    // Where the word being suggested for starts.
    std::size_t m_start{};
    // What was written of it when the list was last made: the same, and the list stays as it is.
    std::string m_written;
    olagarro::oui::PopupList* m_list{};
    std::vector<Suggestion> m_suggestions;
    // The list says there is nothing until the next event, or for a moment.
    bool m_saying_nothing{false};
    // Just asked for: the look after the event that asked leaves it as it is.
    bool m_just_asked{false};
    olagarro::oui::Timer m_quiet;
    SignatureTip* m_tip{};
    // The call it shows, and the one it was closed in with Escape: the document and where its "(" is.
    struct CallPlace
    {
        olagarro::oui::WidgetRef<const olagarro::oui::TextEdit> editor;
        std::size_t open{};

        [[nodiscard]] bool is(const olagarro::oui::TextEdit* other, std::size_t at) const
        {
            return editor.refers_to(other) && open == at;
        }
    };
    CallPlace m_call;
    std::optional<CallPlace> m_dismissed;
};
