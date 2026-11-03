#pragma once

#include <paint/pixel_format.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace olagarro::paint
{

// A canvas is kept in squares of this many pixels a side: what undo keeps, what work is shared out by, and what is
// drawn again when something changes.
inline constexpr int tile_size{256};

// Which tile, counted in tiles from the canvas's top left corner.
struct TileIndex
{
    int x{0};
    int y{0};

    [[nodiscard]] bool operator==(const TileIndex& other) const { return x == other.x && y == other.y; }
    [[nodiscard]] bool operator!=(const TileIndex& other) const { return !(*this == other); }
    [[nodiscard]] bool operator<(const TileIndex& other) const { return y != other.y ? y < other.y : x < other.x; }
};

// The pixels of one tile, see-through black when made.
class Tile
{
  public:
    explicit Tile(PixelFormat format);

    [[nodiscard]] PixelFormat format() const { return m_format; }
    [[nodiscard]] std::uint8_t* row(int y) { return &m_bytes[static_cast<std::size_t>(y) * row_bytes()]; }
    [[nodiscard]] const std::uint8_t* row(int y) const { return &m_bytes[static_cast<std::size_t>(y) * row_bytes()]; }
    [[nodiscard]] std::size_t row_bytes() const { return tile_size * bytes_per_pixel(m_format); }

    template <typename PixelType> [[nodiscard]] PixelType pixel(int x, int y) const
    {
        PixelType result;
        std::memcpy(result.value.data(), row(y) + x * sizeof(PixelType), sizeof(PixelType));
        return result;
    }
    template <typename PixelType> void set_pixel(int x, int y, const PixelType& pixel)
    {
        std::memcpy(row(y) + x * sizeof(PixelType), pixel.value.data(), sizeof(PixelType));
    }

    // Whether every byte is zero, as a tile just made: what a canvas need not keep.
    [[nodiscard]] bool empty() const;

    [[nodiscard]] bool operator==(const Tile& other) const
    {
        return m_format == other.m_format && m_bytes == other.m_bytes;
    }
    [[nodiscard]] bool operator!=(const Tile& other) const { return !(*this == other); }

  private:
    PixelFormat m_format;
    std::vector<std::uint8_t> m_bytes;
};

} // namespace olagarro::paint
