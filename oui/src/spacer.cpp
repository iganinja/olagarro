#include <oui/spacer.h>

namespace olagarro::oui
{

VSpacer::VSpacer(int height) : m_height{height}
{
}

Size VSpacer::measure(const Constraints& constraints) const
{
    return constraints.constrain({0, m_height});
}

void VSpacer::paint(Painter&, const Theme&) const
{
}

std::unique_ptr<VSpacer> vspacer(int height)
{
    return std::make_unique<VSpacer>(height);
}

VSpacer& vspacer(int height, Container& parent)
{
    return parent.add(vspacer(height));
}

HSpacer::HSpacer(int width) : m_width{width}
{
}

Size HSpacer::measure(const Constraints& constraints) const
{
    return constraints.constrain({m_width, 0});
}

void HSpacer::paint(Painter&, const Theme&) const
{
}

std::unique_ptr<HSpacer> hspacer(int width)
{
    return std::make_unique<HSpacer>(width);
}

HSpacer& hspacer(int width, Container& parent)
{
    return parent.add(hspacer(width));
}

} // namespace olagarro::oui
