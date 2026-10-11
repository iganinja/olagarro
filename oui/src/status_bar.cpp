#include <oui/status_bar.h>
#include <oui/text_layout.h>
#include <oui/utf8.h>

#include <algorithm>

namespace olagarro::oui
{

void StatusBar::set_left(std::string text)
{
    m_text.left = std::move(text);
}

void StatusBar::set_right(std::string text)
{
    m_text.right = std::move(text);
}

void StatusBar::set_provider(std::function<StatusText()> provider)
{
    m_provider = std::move(provider);
}

Size StatusBar::measure(const Constraints& constraints) const
{
    return constraints.constrain({constraints.max_width, 1});
}

void StatusBar::paint(Painter& painter, const Theme& theme) const
{
    const auto text{m_provider ? m_provider() : m_text};
    const TextStyle style{theme.status_foreground, theme.status_background};
    painter.draw_bar(bounds(), BarKind::status, theme);
    auto x{bounds().x};
    if(!text.keys.empty())
    {
        x += 1;
        for(const auto& [key, label] : text.keys)
        {
            painter.draw_label({x, bounds().y}, key, {theme.hotkey_foreground, theme.status_background});
            x += shown_width(key);
            const auto told{" " + label + "  "};
            painter.draw_label({x, bounds().y}, told, style);
            x += shown_width(told);
        }
        if(!text.left.empty())
        {
            painter.draw_label({x, bounds().y}, "│ ", style);
            x += 2;
        }
    }
    painter.draw_label({x, bounds().y}, text.left, style);
    const auto right_width{shown_width(text.right)};
    painter.draw_label({bounds().x + bounds().width - right_width, bounds().y}, text.right, style, Align::end);
}

int status_keys_width(const std::vector<StatusKey>& keys)
{
    if(keys.empty())
    {
        return 0;
    }
    auto width{1 + 2};
    for(const auto& [key, label] : keys)
    {
        width += shown_width(key) + shown_width(" " + label + "  ");
    }
    return width;
}

std::unique_ptr<StatusBar> status_bar()
{
    return std::make_unique<StatusBar>();
}

StatusBar& status_bar(Container& parent)
{
    return parent.add(status_bar());
}

} // namespace olagarro::oui
