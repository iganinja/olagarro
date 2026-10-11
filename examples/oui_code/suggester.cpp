#include "suggester.h"

#include <oui/translation.h>
#include <oui/ui.h>
#include <oui/utf8.h>

#include <chrono>
#include <memory>
#include <utility>

using namespace olagarro;

namespace
{

// Typing these opens the list by itself: a letter of a word, or the dot before a member.
[[nodiscard]] bool opens_list(char32_t typed)
{
    return typed == U'.' || typed == U'_' || typed == U'$' || (typed >= U'a' && typed <= U'z') ||
           (typed >= U'A' && typed <= U'Z') || typed >= 0x80;
}

// What there is to suggest at the cursor, and where what it replaces starts.
struct Found
{
    std::size_t start{};
    std::vector<Suggestion> suggestions;
    // Whether what is written is enough to open the list as one writes: a word of two letters, or right after a
    // dot; in a user interface description, whatever fits, after a space, an = or a comma too.
    bool enough{};
    // Whether it is a description's, which a space, an = or a comma typed opens.
    bool description{};
};

// In a user interface description, what fits where the cursor is; elsewhere what completes the word written. Without
// `suggestions`, only where it starts and whether it is enough: the words of every document are not looked through.
[[nodiscard]] Found found_at(const Suggester::Context& context, const ScriptApi& api, bool suggestions)
{
    const auto text{context.editor->text()};
    const auto cursor{context.editor->cursor_offset()};
    if(context.language != nullptr && context.language->id == "oui")
    {
        auto fits{oui::ui_suggestions(text, cursor)};
        Found found{fits.start, {}, true, true};
        for(auto& item : fits.items)
        {
            found.suggestions.push_back({std::move(item.label), std::move(item.detail), std::move(item.text)});
        }
        return found;
    }
    const auto writing{writing_at(text, cursor)};
    const auto enough{writing.prefix.size() >= 2 || (writing.prefix.empty() && !writing.qualifier.empty())};
    return Found{writing.start,
                 suggestions
                     ? suggestions_for(writing, context.documents, context.language, context.script ? &api : nullptr)
                     : std::vector<Suggestion>{},
                 enough, false};
}

} // namespace

Suggester::Suggester(oui::Application& application, std::function<std::optional<Context>()> context,
                     const ScriptApi& api)
    : m_application{application}, m_context{std::move(context)}, m_api{api}
{
}

void Suggester::set_as_you_type(bool as_you_type)
{
    m_as_you_type = as_you_type;
}

bool Suggester::is_open() const
{
    return m_list != nullptr;
}

void Suggester::after_event(std::optional<char32_t> typed)
{
    if(std::exchange(m_just_asked, false))
    {
        return;
    }
    const auto context{m_context()};
    if(!context || context->editor == nullptr || context->editor->cursor_count() > 1)
    {
        close();
        close_signature();
        return;
    }
    if(m_saying_nothing && m_list != nullptr)
    {
        // What it said is for the moment it was asked.
        close();
    }
    else
    {
        follow_word(*context, typed);
    }
    // While the list is not open, how the call the cursor is in is called.
    if(m_list == nullptr)
    {
        follow_call(*context);
    }
}

void Suggester::follow_word(const Context& context, std::optional<char32_t> typed)
{
    const auto& text{context.editor->text()};
    const auto cursor{context.editor->cursor_offset()};
    const auto where{found_at(context, m_api, false)};
    if(m_list != nullptr)
    {
        // It follows the word while the cursor stays in it, and goes when it leaves.
        if(!m_editor.refers_to(context.editor) || where.start != m_start)
        {
            close();
            return;
        }
        // An arrow, a click on it: nothing written changed, and the choice made in it stays.
        if(text.substr(where.start, cursor - where.start) == m_written)
        {
            return;
        }
        auto found{found_at(context, m_api, true)};
        if(found.suggestions.empty())
        {
            close();
            return;
        }
        show(context, found.start, std::move(found.suggestions));
        return;
    }
    // A letter, or after a dot; in a description a space, an = or a comma too, after which what fits is known.
    if(!typed || !m_as_you_type ||
       !(opens_list(*typed) || (where.description && (*typed == U' ' || *typed == U'=' || *typed == U','))) ||
       !where.enough)
    {
        return;
    }
    // Written in the document: it is just before the cursor. A letter that went elsewhere, as one picking a menu's
    // entry, opens nothing in the document focused after.
    if(cursor == 0)
    {
        return;
    }
    auto before{cursor - 1};
    while(before > 0 && (static_cast<unsigned char>(text[before]) & 0xC0) == 0x80)
    {
        -- before;
    }
    if(oui::decode_utf8(text, before) != *typed)
    {
        return;
    }
    auto found{found_at(context, m_api, true)};
    if(!found.suggestions.empty())
    {
        show(context, found.start, std::move(found.suggestions));
    }
}

void Suggester::suggest()
{
    m_just_asked = true;
    const auto context{m_context()};
    if(!context || context->editor == nullptr || context->editor->cursor_count() > 1)
    {
        return;
    }
    auto found{found_at(*context, m_api, true)};
    if(found.suggestions.empty())
    {
        say_nothing(*context, found.start);
        return;
    }
    show(*context, found.start, std::move(found.suggestions));
}

void Suggester::show(const Context& context, std::size_t start, std::vector<Suggestion> suggestions)
{
    const auto cell{context.editor->cell_of(start)};
    if(!cell)
    {
        close();
        return;
    }
    std::vector<oui::PopupItem> items;
    items.reserve(suggestions.size());
    for(const auto& suggestion : suggestions)
    {
        items.push_back({suggestion.label, suggestion.detail});
    }
    m_suggestions = std::move(suggestions);
    m_saying_nothing = false;
    m_written = std::string{context.editor->text().substr(start, context.editor->cursor_offset() - start)};
    if(m_list != nullptr && m_editor.refers_to(context.editor) && m_start == start)
    {
        m_list->set_items(std::move(items));
        m_list->set_anchor(*cell);
        return;
    }
    close();
    m_editor = context.editor;
    m_start = start;
    auto list{std::make_unique<oui::PopupList>(*cell)};
    list->set_items(std::move(items));
    list->on_chosen([this](std::size_t index) { choose(index); });
    list->on_cancelled([this] { close(); });
    m_list = list.get();
    m_application.show_popup(std::move(list), [this] { m_list = nullptr; });
}

void Suggester::say_nothing(const Context& context, std::size_t start)
{
    const auto cell{context.editor->cell_of(start)};
    if(!cell)
    {
        return;
    }
    close();
    auto list{std::make_unique<oui::PopupList>(*cell)};
    list->set_message(oui::translate("Suggestions", "Nothing to suggest"));
    list->on_cancelled([this] { close(); });
    m_list = list.get();
    m_editor = context.editor;
    m_start = start;
    m_saying_nothing = true;
    m_application.show_popup(std::move(list), [this] { m_list = nullptr; });
    m_quiet = m_application.start_timer(std::chrono::milliseconds{1500},
                                        [this]
                                        {
                                            m_quiet.stop();
                                            if(m_saying_nothing)
                                            {
                                                close();
                                            }
                                        });
}

void Suggester::choose(std::size_t index)
{
    // The document it was for may have closed since: then nothing is written anywhere.
    auto* const editor{m_editor.get()};
    if(index >= m_suggestions.size() || editor == nullptr)
    {
        close();
        return;
    }
    auto text{m_suggestions[index].text.empty() ? m_suggestions[index].label : m_suggestions[index].text};
    const auto start{m_start};
    // A piece of many lines takes the indentation of the line it goes in.
    const auto& whole{editor->text()};
    const auto line_start{whole.rfind('\n', start == 0 ? 0 : start - 1)};
    const auto from{line_start == std::string_view::npos || start == 0 ? 0 : line_start + 1};
    const auto indent{std::string{whole.substr(from, whole.find_first_not_of(" \t", from) - from)}};
    for(std::size_t at{text.find('\n')}; at != std::string::npos; at = text.find('\n', at + 1 + indent.size()))
    {
        text.insert(at + 1, indent);
    }
    const auto mark{text.find("$0")};
    if(mark != std::string::npos)
    {
        text.erase(mark, 2);
    }
    close();
    editor->select_range(start, editor->cursor_offset());
    editor->paste(text);
    if(mark != std::string::npos)
    {
        editor->select_range(start + mark, start + mark);
    }
}

void Suggester::close()
{
    m_saying_nothing = false;
    m_quiet.stop();
    if(m_list != nullptr)
    {
        m_list = nullptr;
        m_application.close_popup();
    }
}

void Suggester::follow_call(const Context& context)
{
    auto* const editor{context.editor};
    const auto call{context.script ? call_at(editor->text(), editor->cursor_offset()) : std::nullopt};
    const auto signature{call ? m_api.signature(*call, context.documents) : std::nullopt};
    if(!signature)
    {
        m_dismissed.reset();
        close_signature();
        return;
    }
    // Closed with Escape, it stays closed until the cursor is in another call.
    if(m_dismissed && m_dismissed->is(editor, call->open))
    {
        return;
    }
    m_dismissed.reset();
    const auto cell{editor->cell_of(call->open - call->name.size())};
    if(!cell)
    {
        close_signature();
        return;
    }
    if(m_tip == nullptr)
    {
        auto tip{std::make_unique<SignatureTip>(*cell)};
        tip->on_cancelled(
            [this]
            {
                m_dismissed = m_call;
                close_signature();
            });
        m_tip = tip.get();
        m_application.show_popup(std::move(tip), [this] { m_tip = nullptr; });
    }
    m_tip->set_anchor(*cell);
    m_tip->show(*signature, call->argument);
    m_call = {editor, call->open};
}

void Suggester::close_signature()
{
    if(m_tip != nullptr)
    {
        m_tip = nullptr;
        m_application.close_popup();
    }
}
