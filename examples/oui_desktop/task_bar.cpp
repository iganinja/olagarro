#include "task_bar.h"

#include <oui/oui.h>

#include <algorithm>
#include <string_view>
#include <utility>

using namespace olagarro;

TaskBar::TaskBar(DesktopArea& desktop, oui::DesktopServer& server, std::vector<Launcher> launchers)
    : m_desktop{desktop}, m_server{server}, m_launchers{std::move(launchers)}
{
}

oui::Size TaskBar::measure(const oui::Constraints& constraints) const
{
    return constraints.constrain({std::max(1, constraints.max_width), 1});
}

void TaskBar::paint(oui::Painter& painter, const oui::Theme& theme) const
{
    painter.draw_bar(bounds(), oui::BarKind::status, theme);
    for(const auto& entry : entries())
    {
        if(entry.launcher >= 0)
        {
            oui::ButtonLook look;
            look.text = m_launchers[static_cast<std::size_t>(entry.launcher)].title;
            look.shadow = false;
            look.around = theme.status_background;
            painter.draw_button(entry.bounds, look, theme);
        }
        else
        {
            oui::TabLook look;
            look.title = entry.title;
            look.selected = entry.window == m_desktop.focused();
            painter.draw_tab(entry.bounds, look, theme);
        }
    }
    if(m_desktop.waiting_for_desktop_key())
    {
        // What F12 can be followed by, at the right.
        const std::string_view keys{"F12: Tab next  t terminal  x close  m maximize  n minimize  q quit "};
        const auto width{oui::text_width(keys)};
        painter.draw_label({bounds().x + std::max(0, bounds().width - width), bounds().y}, keys,
                           {theme.status_foreground, theme.status_background});
    }
}

bool TaskBar::handle_event(const oui::Event& event)
{
    const auto* const mouse{std::get_if<oui::MouseEvent>(&event)};
    if(mouse == nullptr || !mouse->pressed)
    {
        return mouse != nullptr;
    }
    for(const auto& entry : entries())
    {
        if(mouse->x >= entry.bounds.x && mouse->x < entry.bounds.x + entry.bounds.width)
        {
            if(entry.launcher >= 0)
            {
                launch(m_launchers[static_cast<std::size_t>(entry.launcher)].command, m_server.socket());
            }
            else if(entry.window == m_desktop.focused())
            {
                m_desktop.minimize(entry.window);
            }
            else
            {
                m_desktop.raise(entry.window);
            }
        }
    }
    return true;
}

std::vector<TaskBar::Entry> TaskBar::entries() const
{
    std::vector<Entry> list;
    auto x{bounds().x + 1};
    for(std::size_t index{}; index < m_launchers.size(); ++ index)
    {
        const auto width{oui::text_width(m_launchers[index].title) + 4};
        list.push_back({{x, bounds().y, width, 1}, static_cast<int>(index), 0, {}});
        x += width + 1;
    }
    x += 2;
    auto ids{m_desktop.windows()};
    std::sort(ids.begin(), ids.end());
    for(const auto id : ids)
    {
        const auto* const client{m_server.client(id)};
        auto title{client != nullptr && !client->title.empty() ? client->title : std::string{"?"}};
        title = oui::fit_text(title, 24);
        const auto width{oui::text_width(title) + 4};
        list.push_back({{x, bounds().y, width, 1}, -1, id, std::move(title)});
        x += width;
    }
    return list;
}
