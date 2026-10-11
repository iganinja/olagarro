#pragma once

#include <oui/event.h>
#include <oui/geometry.h>

#include <optional>

namespace olagarro::oui
{

// The thumb of a scrollbar `length` cells long, for a view that shows `shown` lines of `total`, from line `scroll`
// on, the furthest being `most`: where it starts along the track, from 0, and how many cells it takes. Every view
// with a scrollbar draws its thumb so, and ScrollbarDrag finds it there.
struct ScrollbarThumb
{
    int start{};
    int length{};
};
[[nodiscard]] ScrollbarThumb scrollbar_thumb(int length, int shown, int total, int scroll, int most);

// What a vertical scrollbar does with the mouse: a press above or below its thumb moves a page, and the thumb is
// dragged with the pointer, wherever the pointer goes, until it is let go. A view keeps one and gives it each mouse
// event before anything else; one it takes gives the line the view is to start at.
class ScrollbarDrag
{
  public:
    // `track` the scrollbar's cells; `shown`, `total`, `scroll` and `most` as scrollbar_thumb takes them; `page` the
    // lines a page moves.
    [[nodiscard]] std::optional<int> handle(const MouseEvent& mouse, const Rect& track, int shown, int total,
                                            int scroll, int most, int page);

  private:
    // Where along the thumb it was taken, while it is held.
    std::optional<int> m_grab;
};

} // namespace olagarro::oui
