#include "navigation.h"

#include <utility>

using namespace olagarro;

namespace
{

constexpr std::size_t visited_limit{50};
constexpr std::size_t jump_lines{10};

} // namespace

NavigationHistory::NavigationHistory(oui::Application& application, Editors& editors,
                                     std::function<void(const std::filesystem::path&)> open)
    : m_application{application}, m_editors{editors}, m_open{std::move(open)}
{
}

void NavigationHistory::remember()
{
    const auto now{here()};
    if(!now)
    {
        return;
    }
    const auto jumped{m_last &&
                      (now->editor.get() != m_last->editor.get() ||
                       (now->line > m_last->line ? now->line - m_last->line : m_last->line - now->line) >= jump_lines)};
    if(jumped)
    {
        m_back.push_back(*m_last);
        if(m_back.size() > visited_limit)
        {
            m_back.erase(m_back.begin());
        }
        m_forward.clear();
    }
    m_last = now;
}

void NavigationHistory::back()
{
    travel(m_back, m_forward);
}

void NavigationHistory::forward()
{
    travel(m_forward, m_back);
}

std::optional<NavigationHistory::Visited> NavigationHistory::here() const
{
    auto& group{m_editors.active()};
    const auto index{group.tabs.selected_index()};
    const auto* const editor{selected_document_editor(group.tabs)};
    if(editor == nullptr || index >= group.documents.size() || group.documents[index].preview_of)
    {
        return std::nullopt;
    }
    return Visited{editor, group.documents[index].path, editor->cursor_offset(), editor->line_column().first};
}

bool NavigationHistory::visit(const Visited& place)
{
    const auto arrive{[&](oui::TextEdit& editor)
                      {
                          editor.select_range(place.offset, place.offset);
                          m_application.focus(editor);
                          m_last = here();
                      }};
    for(auto& group : m_editors.groups())
    {
        for(std::size_t index{}; index < group.documents.size(); ++ index)
        {
            auto* const editor{document_editor(group.tabs, index)};
            if(place.editor.refers_to(editor) && group.documents[index].path == place.path)
            {
                group.tabs.select(index);
                arrive(*editor);
                return true;
            }
        }
    }
    if(!place.path)
    {
        return false;
    }
    m_open(*place.path);
    auto& group{m_editors.active()};
    const auto index{group.tabs.selected_index()};
    auto* const editor{selected_document_editor(group.tabs)};
    if(editor == nullptr || index >= group.documents.size() || group.documents[index].path != place.path)
    {
        return false;
    }
    arrive(*editor);
    return true;
}

void NavigationHistory::travel(std::vector<Visited>& from, std::vector<Visited>& to)
{
    const auto leaving{here()};
    while(!from.empty())
    {
        const auto place{from.back()};
        from.pop_back();
        if(visit(place))
        {
            if(leaving)
            {
                to.push_back(*leaving);
            }
            return;
        }
    }
}
