#include <paint/tile.h>

#include <algorithm>

namespace olagarro::paint
{

Tile::Tile(PixelFormat format)
    : m_format{format}, m_bytes(static_cast<std::size_t>(tile_size) * tile_size * bytes_per_pixel(format))
{
}

bool Tile::empty() const
{
    return std::all_of(m_bytes.begin(), m_bytes.end(), [](std::uint8_t byte) { return byte == 0; });
}

} // namespace olagarro::paint
