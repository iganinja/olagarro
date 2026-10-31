#include <oui/scrollbar.h>

#include <algorithm>

namespace olagarro::oui
{

ScrollbarThumb scrollbar_thumb(int length, int shown, int total, int scroll, int most)
{
    if(length <= 0 || total <= 0)
    {
        return {0, std::max(0, length)};
    }
    const auto thumb{std::clamp(length * shown / total, 1, length)};
    const auto travel{length - thumb};
    return {most <= 0 ? 0 : std::clamp(scroll, 0, most) * travel / most, thumb};
}

std::optional<int> ScrollbarDrag::handle(const MouseEvent& mouse, const Rect& track, int shown, int total, int scroll,
                                         int most, int page)
{
    // The thumb is in whole cells.
    const auto height{whole(track.height)};
    const auto thumb{scrollbar_thumb(height, shown, total, scroll, most)};
    if(m_grab)
    {
        if(mouse.released)
        {
            m_grab.reset();
            return scroll;
        }
        if(!mouse.dragging)
        {
            m_grab.reset();
            return std::nullopt;
        }
        const auto travel{height - thumb.length};
        const auto start{std::clamp(whole(mouse.y - track.y) - *m_grab, 0, std::max(0, travel))};
        return travel == 0 ? 0 : (start * most + travel / 2) / travel;
    }
    if(!mouse.pressed || mouse.right || mouse.x < track.x || mouse.x >= track.x + track.width || mouse.y < track.y ||
       mouse.y >= track.y + track.height || most <= 0)
    {
        return std::nullopt;
    }
    const auto at{whole(mouse.y - track.y)};
    if(at < thumb.start)
    {
        return std::max(0, scroll - page);
    }
    if(at >= thumb.start + thumb.length)
    {
        return std::min(most, scroll + page);
    }
    m_grab = at - thumb.start;
    return scroll;
}

} // namespace olagarro::oui
