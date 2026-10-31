#include <oui/progress_bar.h>

#include <oui/painter.h>
#include <oui/text_metrics.h>

#include <algorithm>
#include <cmath>

namespace olagarro::oui
{

void ProgressBar::set_value(double value)
{
    m_value = std::isnan(value) ? 0.0 : std::clamp(value, 0.0, 1.0);
}

double ProgressBar::value() const
{
    return m_value;
}

void ProgressBar::set_text(std::string_view text)
{
    m_text = text;
}

std::string_view ProgressBar::text() const
{
    return m_text;
}

Size ProgressBar::measure(const Constraints& constraints) const
{
    // Ten cells of track at least, and its text after them, in whole cells: in a proportional font text takes fewer
    // cells than it has characters.
    const auto& metrics{text_metrics()};
    const auto cell{std::max(1, metrics.cell_width())};
    const auto words{m_text.empty() ? 0 : (metrics.width(m_text) + cell - 1) / cell + 1};
    return constraints.constrain({static_cast<double>(10 + words), 1.0});
}

void ProgressBar::paint(Painter& painter, const Theme& theme) const
{
    painter.draw_progress(bounds(), {m_value, m_text}, theme);
}

std::unique_ptr<ProgressBar> progress_bar()
{
    return std::make_unique<ProgressBar>();
}

ProgressBar& progress_bar(Container& parent)
{
    return parent.add(progress_bar());
}

} // namespace olagarro::oui
