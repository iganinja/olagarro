#include <oui/status_bar.h>
#include <oui/text_layout.h>
#include <oui/text_metrics.h>
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
        const auto& metrics{text_metrics()};
        for(const auto& [key, label] : text.keys)
        {
            const auto key_width{metrics.label_width(key)};
            painter.draw_label({x, bounds().y, key_width, 1}, key, {theme.hotkey_foreground, theme.status_background});
            x += key_width;
            const auto told{" " + label + "  "};
            const auto told_width{metrics.label_width(told)};
            painter.draw_label({x, bounds().y, told_width, 1}, told, style);
            x += told_width;
        }
        if(!text.left.empty())
        {
            painter.draw_label(Point{x, bounds().y}, "│ ", style);
            x += 2;
        }
    }
    const auto right_width{text_metrics().label_width(text.right)};
    const auto right{bounds().x + bounds().width - right_width};
    painter.draw_label({x, bounds().y, std::max(0.0, right - x), 1}, text.left, style);
    painter.draw_label({right, bounds().y, right_width, 1}, text.right, style, Align::end);
}

double status_keys_width(const std::vector<StatusKey>& keys)
{
    if(keys.empty())
    {
        return 0;
    }
    auto width{1.0 + 2};
    for(const auto& [key, label] : keys)
    {
        width += text_metrics().label_width(key) + text_metrics().label_width(" " + label + "  ");
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
