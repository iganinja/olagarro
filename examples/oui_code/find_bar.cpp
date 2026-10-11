#include "find_bar.h"

#include "ui_descriptions.h"

#include <oui/oui.h>

#include <algorithm>
#include <array>
#include <iterator>
#include <string>
#include <string_view>

using namespace olagarro;

FindBar::FindBar(oui::Application& application, oui::TextEdit& document)
    : m_application{application}, m_document{&document}
{
    auto ui{build_oui_code_ui(OuiCodeUi::find_bar)};
    m_query = &ui.get<oui::LineEdit>("query");
    // Enter goes to the next match and into the document there, to work on it; with no match it stays here.
    m_query->on_submitted(
        [this](std::string_view)
        {
            // A match found means the document is there.
            if(find_match(true))
            {
                m_application.focus(*shown_document());
            }
        });
    // Matches show as soon as there is something to find, and the first one from where the search started
    // is selected and scrolled to, before Enter moves on.
    m_query->on_changed(
        [this](std::string_view)
        {
            refresh();
            find_as_typed();
        });
    m_count = &ui.get<oui::Label>("count");
    static_cast<void>(ui.get<oui::Button>("previous").on_clicked([this] { previous_match(); }));
    static_cast<void>(ui.get<oui::Button>("next").on_clicked([this] { next_match(); }));
    static_cast<void>(ui.get<oui::Button>("options").on_clicked([this] { toggle_options(); }));
    m_whole_word_button = &ui.get<oui::CheckButton>("whole_word");
    m_regex_button = &ui.get<oui::CheckButton>("regex");
    m_match_case_button = &ui.get<oui::CheckButton>("match_case");
    m_whole_word_button->on_toggled(
        [this](bool checked)
        {
            m_whole_word = checked;
            refresh();
        });
    m_regex_button->on_toggled(
        [this](bool checked)
        {
            m_regex = checked;
            refresh();
        });
    m_match_case_button->on_toggled(
        [this](bool checked)
        {
            m_match_case = checked;
            refresh();
        });
    m_whole_word = false;
    m_regex = false;
    m_match_case = false;
    m_options_panel = &ui.get<oui::Column>("options_panel");
    // The bar is the first child, as arrange takes it; the options open over it.
    add(ui.take("bar"));
    add(ui.take("options_panel"));
    set_visible(false);
    watch(document);
}

FindBar::~FindBar()
{
    for(const auto& connection : m_document_connections)
    {
        connection.disconnect();
    }
}

void FindBar::open(oui::TextEdit& document)
{
    set_document(document);
    set_visible(true);
    // Typing finds from here, the start of what was selected, as it goes.
    m_search_start = document.selection_range().first;
    if(const auto selected{document.selected_text()}; !selected.empty())
    {
        m_query->set_text(selected);
    }
    refresh();
    m_application.focus(*m_query);
    // What was looked for before shows selected, so typing replaces it.
    m_query->select_all();
}

void FindBar::set_document(oui::TextEdit& document)
{
    auto* const current{shown_document()};
    if(current != nullptr && current != &document)
    {
        current->clear_search_highlight();
    }
    if(current != &document)
    {
        m_search_start = document.selection_range().first;
    }
    watch(document);
    refresh();
}

void FindBar::close()
{
    set_visible(false);
    if(auto* const document{shown_document()})
    {
        document->clear_search_highlight();
        m_application.focus(*document);
    }
}

void FindBar::next_match()
{
    static_cast<void>(find_match(true));
}

void FindBar::find_selected(oui::TextEdit& document)
{
    set_document(document);
    auto text{document.selected_text()};
    if(text.empty())
    {
        const auto [start, end]{document.word_at_cursor()};
        text = std::string{document.text().substr(start, end - start)};
    }
    if(text.find('\n') != std::string::npos || text.find_first_not_of(" \t") == std::string::npos)
    {
        return;
    }
    m_regex = false;
    m_regex_button->set_checked(false);
    m_query->set_text(text);
    next_match();
}

void FindBar::previous_match()
{
    static_cast<void>(find_match(false));
}

oui::Size FindBar::measure(const oui::Constraints& constraints) const
{
    return constraints.constrain({1, 1});
}

void FindBar::arrange(const oui::Rect& bounds)
{
    oui::Widget::arrange(bounds);
    if(child_count() != 0u)
    {
        child_at(0).arrange(bounds);
    }
    if(m_options_panel != nullptr)
    {
        m_options_panel->set_visible(m_options_open);
        m_options_panel->arrange(options_bounds());
    }
}

oui::Rect FindBar::paint_bounds() const
{
    auto result{bounds()};
    if(m_options_open)
    {
        const auto popup{options_bounds()};
        const auto right{result.x + result.width};
        const auto bottom{result.y + result.height};
        result.x = std::min(result.x, popup.x);
        result.y = std::min(result.y, popup.y);
        result.width = std::max(right, popup.x + popup.width) - result.x;
        result.height = std::max(bottom, popup.y + popup.height) - result.y;
    }
    return result;
}

void FindBar::paint(oui::Painter& painter, const oui::Theme& theme) const
{
    child_at(0).paint(painter, theme);
    if(!m_options_open)
    {
        return;
    }
    painter.fill_rect(options_bounds(), theme.menu_background);
    for(std::size_t index{}; index < m_options_panel->child_count(); ++ index)
    {
        m_options_panel->child_at(index).paint(painter, theme);
    }
}

oui::Widget* FindBar::hit_test(int x, int y)
{
    if(m_options_open && options_bounds().x <= x && x < options_bounds().x + options_bounds().width &&
       options_bounds().y <= y && y < options_bounds().y + options_bounds().height)
    {
        return this;
    }
    return oui::Container::hit_test(x, y);
}

bool FindBar::handle_event(const oui::Event& event)
{
    if(const auto* mouse{std::get_if<oui::MouseEvent>(&event)})
    {
        if(m_options_open && mouse->released)
        {
            const auto popup{options_bounds()};
            if(mouse->x >= popup.x && mouse->x < popup.x + popup.width && mouse->y >= popup.y &&
               mouse->y < popup.y + popup.height)
            {
                const auto row{mouse->y - popup.y};
                if(row < 3 && m_options_panel != nullptr)
                {
                    auto* const checkbox{
                        dynamic_cast<oui::CheckButton*>(&m_options_panel->child_at(static_cast<std::size_t>(row)))};
                    if(checkbox != nullptr)
                    {
                        checkbox->set_checked(!checkbox->is_checked());
                        m_whole_word = m_whole_word_button->is_checked();
                        m_regex = m_regex_button->is_checked();
                        m_match_case = m_match_case_button->is_checked();
                        refresh();
                    }
                }
                return true;
            }
        }
    }
    if(const auto* key{std::get_if<oui::KeyEvent>(&event)})
    {
        if(m_options_open && !key->control && !key->alt && (key->key == oui::Key::up || key->key == oui::Key::down))
        {
            if(focus_option(key->key == oui::Key::down ? 1 : -1))
            {
                return true;
            }
        }
        if(key->key == oui::Key::escape)
        {
            // From the options, back to what is looked for; from there, out of the bar.
            if(m_options_open)
            {
                toggle_options();
                return true;
            }
            close();
            return true;
        }
    }
    return false;
}

bool FindBar::focus_option(int offset)
{
    std::array<oui::CheckButton*, 3> options{m_whole_word_button, m_regex_button, m_match_case_button};
    const auto current{
        std::find(options.begin(), options.end(), dynamic_cast<oui::CheckButton*>(m_application.focused()))};
    if(current == options.end())
    {
        return false;
    }
    const auto index{static_cast<int>(std::distance(options.begin(), current))};
    const auto next{(index + offset + static_cast<int>(options.size())) % static_cast<int>(options.size())};
    m_application.focus(*options.at(static_cast<std::size_t>(next)));
    return true;
}

oui::Rect FindBar::options_bounds() const
{
    return {bounds().x + std::max(0, bounds().width - 28), bounds().y - 3, 28, 3};
}

oui::SearchOptions FindBar::search_options() const
{
    oui::SearchOptions options;
    options.match_case = m_match_case;
    options.whole_word = m_whole_word;
    options.regex = m_regex;
    return options;
}

void FindBar::find_as_typed()
{
    auto* const document{shown_document()};
    if(document == nullptr || !is_visible())
    {
        return;
    }
    const auto [anchor, cursor]{document->selection_range()};
    document->select_range(m_search_start, m_search_start);
    if(!find_match(true))
    {
        document->select_range(anchor, cursor);
    }
}

bool FindBar::find_match(bool forward)
{
    auto* const document{shown_document()};
    if(document == nullptr || m_query->text().empty())
    {
        return false;
    }
    return forward ? document->find_next(m_query->text(), search_options())
                   : document->find_previous(m_query->text(), search_options());
}

void FindBar::watch(oui::TextEdit& document)
{
    if(shown_document() == &document && !m_document_connections.empty())
    {
        return;
    }
    for(const auto& connection : m_document_connections)
    {
        connection.disconnect();
    }
    m_document = &document;
    m_document_connections = {document.on_changed([this] { update_count(); }),
                              document.on_cursor_moved([this] { update_count(); })};
}

void FindBar::refresh()
{
    auto* const document{shown_document()};
    if(document == nullptr || !is_visible())
    {
        return;
    }
    if(m_query->text().empty())
    {
        document->clear_search_highlight();
    }
    else
    {
        document->set_search_highlight(m_query->text(), search_options());
    }
    update_count();
}

void FindBar::update_count()
{
    auto* const document{shown_document()};
    if(document == nullptr || !is_visible() || m_query->text().empty())
    {
        m_count->set_text("");
        return;
    }
    const auto total{document->search_match_count()};
    if(total == 0)
    {
        m_count->set_text(oui::translate("FindBar", "No results"));
        return;
    }
    const auto selected{document->selected_search_match()};
    m_count->set_text(oui::format_text(oui::translate("FindBar", "{0} of {1}"),
                                       {selected ? std::to_string(*selected) : "?", std::to_string(total)}));
}

bool FindBar::is_overlay() const
{
    return m_options_open && is_visible();
}

void FindBar::dismiss_overlays_at(int x, int y)
{
    const auto area{paint_bounds()};
    const auto inside{x >= area.x && x < area.x + area.width && y >= area.y && y < area.y + area.height};
    if(m_options_open && !inside)
    {
        m_options_open = false;
    }
    oui::Container::dismiss_overlays_at(x, y);
}

oui::TextEdit* FindBar::shown_document() const
{
    return m_document.get();
}

void FindBar::toggle_options()
{
    m_options_open = !m_options_open;
    m_options_panel->set_visible(m_options_open);
    if(m_options_open)
    {
        m_application.focus(*m_whole_word_button);
    }
    else
    {
        m_application.focus(*m_query);
    }
}
