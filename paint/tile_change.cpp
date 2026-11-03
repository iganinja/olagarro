#include <paint/tile_change.h>

#include <set>

namespace olagarro::paint
{

namespace
{

using Tiles = std::vector<std::pair<TileIndex, std::optional<Tile>>>;

void put(Canvas& canvas, const Tiles& tiles)
{
    for(const auto& [index, tile] : tiles)
    {
        canvas.set_tile(index, tile ? std::make_shared<const Tile>(*tile) : nullptr);
    }
}

[[nodiscard]] std::optional<Tile> kept(const std::shared_ptr<const Tile>& tile)
{
    return tile != nullptr ? std::optional<Tile>{*tile} : std::nullopt;
}

} // namespace

std::vector<TileIndex> TileChange::tiles() const
{
    std::vector<TileIndex> indices;
    indices.reserve(before.size());
    for(const auto& each : before)
    {
        indices.push_back(each.first);
    }
    return indices;
}

void TileChange::undo(Canvas& canvas) const
{
    put(canvas, before);
}

void TileChange::redo(Canvas& canvas) const
{
    put(canvas, after);
}

TileChange tile_change(const Canvas& before, const Canvas& after)
{
    std::set<TileIndex> indices;
    for(const auto* const canvas : {&before, &after})
    {
        for(const auto& each : canvas->tiles())
        {
            indices.insert(each.first);
        }
    }
    TileChange change;
    for(const auto& index : indices)
    {
        const auto old_tile{before.tile(index)};
        const auto new_tile{after.tile(index)};
        if(old_tile == new_tile || (old_tile != nullptr && new_tile != nullptr && *old_tile == *new_tile))
        {
            continue;
        }
        change.before.emplace_back(index, kept(old_tile));
        change.after.emplace_back(index, kept(new_tile));
    }
    return change;
}

} // namespace olagarro::paint
