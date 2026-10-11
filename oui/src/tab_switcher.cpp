#include <oui/tab_switcher.h>

#include <oui/list.h>
#include <oui/window.h>
#include <oui/layout.h>
#include <oui/label.h>

#include <algorithm>
#include <functional>

namespace olagarro::oui
{

void TabSwitcher::set_title(std::string title)
{
    m_title = std::move(title);
}

namespace
{

class WrappingList final : public List
{
  public:
    WrappingList(Window& dialog, std::function<void(int)> cycle) : m_dialog{&dialog}, m_cycle{std::move(cycle)} {}

    bool handle_event(const Event& event) override
    {
        const auto* const key{std::get_if<KeyEvent>(&event)};
        // Ctrl with Tab, Shift+Tab, Page Down or Page Up goes on through the list, as the keys that opened it.
        if(key != nullptr && key->control && !children().empty())
        {
            if(key->key == Key::tab || key->key == Key::page_down)
            {
                m_cycle(1);
                return true;
            }
            if(key->key == Key::shift_tab || key->key == Key::page_up)
            {
                m_cycle(-1);
                return true;
            }
        }
        if(key != nullptr && !children().empty())
        {
            const auto count{static_cast<int>(row_count())};
            const auto current{static_cast<int>(selected_row().value_or(0))};
            if(key->key == Key::up)
            {
                select_row(static_cast<std::size_t>((current - 1 + count) % count));
                return true;
            }
            if(key->key == Key::down)
            {
                select_row(static_cast<std::size_t>((current + 1) % count));
                return true;
            }
        }
        if(List::handle_event(event))
        {
            m_dialog->close(DialogResult::accepted);
            return true;
        }
        return false;
    }

  private:
    Window* m_dialog{};
    std::function<void(int)> m_cycle;
};

[[nodiscard]] std::unique_ptr<WrappingList> wrapping_list(Window& dialog, std::function<void(int)> cycle)
{
    return std::make_unique<WrappingList>(dialog, std::move(cycle));
}

} // namespace

void TabSwitcher::open(Application& application, const std::vector<std::string>& titles, std::size_t selected)
{
    if(m_open || titles.empty())
    {
        return;
    }
    m_open = true;
    auto dialog{window(m_title)};
    auto& dialog_ref{*dialog};
    dialog->set_close_on_escape(true);
    dialog->set_close_on_enter(true);
    dialog->set_resizable(true);
    auto content{column()};
    auto* self{this};
    auto list{wrapping_list(*dialog, [self](int step) { self->move(step); })};
    auto& list_ref{*list};
    for(const auto& title : titles)
    {
        list->add(label(title));
    }
    if(selected < titles.size())
    {
        list_ref.select_row(selected);
    }
    content->add(std::move(list));
    dialog->add(std::move(content));
    m_application = &application;
    m_dialog = &dialog_ref;
    m_list = &list_ref;
    dialog_ref.on_closed(
        [&application, self, &list_ref](DialogResult result)
        {
            self->m_open = false;
            self->m_cycling = false;
            self->m_dialog = nullptr;
            self->m_list = nullptr;
            self->m_pick_timer.stop();
            const auto selected_index{list_ref.selected_row()};
            const auto has_value{selected_index.has_value()};
            const auto index{has_value ? *selected_index : std::size_t{0}};
            application.post(
                [self, result, index, has_value]
                {
                    if(result == DialogResult::accepted && has_value)
                    {
                        self->m_confirmed.fire(index);
                    }
                    else
                    {
                        self->m_cancelled.fire();
                    }
                });
        });
    application.push_overlay(std::move(dialog));
}

void TabSwitcher::cycle(Application& application, const std::vector<std::string>& titles, int step)
{
    if(m_open && m_cycling)
    {
        move(step);
        return;
    }
    if(m_open || titles.empty())
    {
        return;
    }
    const auto count{titles.size()};
    open(application, titles, step > 0 ? std::min<std::size_t>(1, count - 1) : count - 1);
    m_cycling = true;
    if(application.reports_modifiers())
    {
        if(!m_listens_to_modifiers)
        {
            m_listens_to_modifiers = true;
            auto* self{this};
            application.on_modifiers_changed(
                [self](const ModifiersEvent& modifiers)
                {
                    if(self->m_cycling && !modifiers.control)
                    {
                        self->pick();
                    }
                });
        }
        return;
    }
    wait_for_pick();
}

void TabSwitcher::move(int step)
{
    if(m_list == nullptr || m_list->row_count() == 0)
    {
        return;
    }
    const auto count{static_cast<int>(m_list->row_count())};
    const auto current{static_cast<int>(m_list->selected_row().value_or(0))};
    m_list->select_row(static_cast<std::size_t>(((current + step) % count + count) % count));
    if(m_cycling && m_application != nullptr && !m_application->reports_modifiers())
    {
        wait_for_pick();
    }
}

void TabSwitcher::pick()
{
    if(m_dialog != nullptr)
    {
        m_dialog->close(DialogResult::accepted);
    }
}

void TabSwitcher::wait_for_pick()
{
    auto* self{this};
    auto* application{m_application};
    m_pick_timer = m_application->start_timer(std::chrono::milliseconds{700},
                                              [self, application]
                                              {
                                                  // Out of the timer's own call, which stopping it would cut short.
                                                  application->post(
                                                      [self]
                                                      {
                                                          self->m_pick_timer.stop();
                                                          self->pick();
                                                      });
                                              });
}

bool TabSwitcher::is_open() const
{
    return m_open;
}

std::unique_ptr<TabSwitcher> tab_switcher()
{
    return std::make_unique<TabSwitcher>();
}

} // namespace olagarro::oui
