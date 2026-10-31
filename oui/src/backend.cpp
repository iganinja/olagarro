#include <oui/backend.h>

namespace olagarro::oui
{

void Backend::render(const std::function<void(Painter&)>& paint)
{
    const auto cells{size()};
    CellBuffer buffer{whole(cells.width), whole(cells.height)};
    CellBufferPainter painter{buffer};
    paint(painter);
    present(buffer);
}

} // namespace olagarro::oui
