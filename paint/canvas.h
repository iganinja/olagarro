#pragma once

#include <paint/image.h>
#include <paint/tile.h>

#include <cstdint>
#include <map>
#include <memory>

namespace olagarro::paint
{

// One picture to paint on, of a size and a pixel format, kept in tiles. A tile it does not have is see-through black,
// and none is kept that holds only that: an empty canvas costs nothing, and one with a little drawn on it the tiles
// drawn on. Tiles are shared until written to: a copy of a canvas, as undo keeps, costs the pointers to its tiles, and
// writing to a tile someone else holds too copies it first.
class Canvas
{
  public:
    using Tiles = std::map<TileIndex, std::shared_ptr<Tile>>;

    Canvas(PixelFormat format, int width, int height);

    [[nodiscard]] PixelFormat format() const { return m_format; }
    [[nodiscard]] int width() const { return m_width; }
    [[nodiscard]] int height() const { return m_height; }

    // The tile at `index`, none where it is see-through.
    [[nodiscard]] std::shared_ptr<const Tile> tile(TileIndex index) const;
    // The tile at `index` to write to: made see-through where there was none, copied first when shared. One left
    // see-through is kept until drop_empty_tiles.
    [[nodiscard]] Tile& writable_tile(TileIndex index);
    // Puts a tile back as it was, or none, as undo does.
    void set_tile(TileIndex index, std::shared_ptr<const Tile> tile);
    [[nodiscard]] const Tiles& tiles() const { return m_tiles; }
    // Forgets the tiles whose every byte is zero, as a tool does when it is done writing to them one by one.
    void drop_empty_tiles();

    // Copies `image`, of this canvas's format, in with its top left corner at x, y. Nothing stops at the canvas's
    // edges, as a piece being moved may pass them; a tile it leaves see-through is forgotten.
    void write(const Image& image, int x, int y);
    // The pixels of a rectangle, see-through where there are none.
    [[nodiscard]] Image read(int x, int y, int width, int height) const;

  private:
    PixelFormat m_format;
    int m_width;
    int m_height;
    Tiles m_tiles;
};

// The tile a pixel coordinate is in, also for negative ones.
[[nodiscard]] int tile_of(int coordinate);

// The canvas's pixels, its width by its height, in 8-bit RGBA: what a PNG is written from.
[[nodiscard]] Image to_rgba8(const Canvas& canvas);
// A canvas of `format` from `width` by `height` pixels of 8-bit RGBA, row after row, as a PNG is read into; the tiles
// all see-through are not kept.
[[nodiscard]] Canvas from_rgba8(int width, int height, const std::uint8_t* pixels,
                                PixelFormat format = PixelFormat::rgba8);

} // namespace olagarro::paint
