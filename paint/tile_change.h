#pragma once

#include <paint/canvas.h>

#include <optional>
#include <utility>
#include <vector>

namespace olagarro::paint
{

// What one step did to a canvas: the tiles it changed, as they were and as they became, none where a tile was or
// became see-through. Applied either way, it undoes the step or does it again; a program's history keeps one a step.
struct TileChange
{
    std::vector<std::pair<TileIndex, std::optional<Tile>>> before;
    std::vector<std::pair<TileIndex, std::optional<Tile>>> after;

    // Whether the step changed nothing.
    [[nodiscard]] bool empty() const { return before.empty(); }
    // The tiles changed, to be drawn again.
    [[nodiscard]] std::vector<TileIndex> tiles() const;
    // Puts the tiles back as they were before the step, or as it left them.
    void undo(Canvas& canvas) const;
    void redo(Canvas& canvas) const;
};

// What going from `before` to `after`, two states of one canvas, changed: the tiles they do not share and whose pixels
// differ. Cheap when `before` is a copy kept as the step began, as only the tiles written since are compared.
[[nodiscard]] TileChange tile_change(const Canvas& before, const Canvas& after);

} // namespace olagarro::paint
