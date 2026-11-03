#include <paint/canvas.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace olagarro::paint
{

int tile_of(int coordinate)
{
    return coordinate >= 0 ? coordinate / tile_size : -((-coordinate + tile_size - 1) / tile_size);
}

Canvas::Canvas(PixelFormat format, int width, int height) : m_format{format}, m_width{width}, m_height{height}
{
}

std::shared_ptr<const Tile> Canvas::tile(TileIndex index) const
{
    const auto found{m_tiles.find(index)};
    return found != m_tiles.end() ? found->second : nullptr;
}

Tile& Canvas::writable_tile(TileIndex index)
{
    auto& slot{m_tiles[index]};
    if(slot == nullptr)
    {
        slot = std::make_shared<Tile>(m_format);
    }
    else if(slot.use_count() > 1)
    {
        slot = std::make_shared<Tile>(*slot);
    }
    return *slot;
}

void Canvas::set_tile(TileIndex index, std::shared_ptr<const Tile> tile)
{
    if(tile == nullptr)
    {
        m_tiles.erase(index);
        return;
    }
    if(tile->format() != m_format)
    {
        throw std::invalid_argument{"A tile of another format cannot go into this canvas."};
    }
    // Never written to while anyone else holds it: writable_tile copies it first.
    m_tiles[index] = std::const_pointer_cast<Tile>(std::move(tile));
}

void Canvas::drop_empty_tiles()
{
    for(auto each{m_tiles.begin()}; each != m_tiles.end();)
    {
        each = each->second->empty() ? m_tiles.erase(each) : std::next(each);
    }
}

void Canvas::write(const Image& image, int x, int y)
{
    if(image.format != m_format)
    {
        throw std::invalid_argument{"An image of another format cannot be written into this canvas."};
    }
    const auto pixel_bytes{bytes_per_pixel(m_format)};
    for(auto tile_y{tile_of(y)}; tile_y <= tile_of(y + image.height - 1); ++ tile_y)
    {
        for(auto tile_x{tile_of(x)}; tile_x <= tile_of(x + image.width - 1); ++ tile_x)
        {
            const auto left{std::max(x, tile_x * tile_size)};
            const auto right{std::min(x + image.width, (tile_x + 1) * tile_size)};
            const auto top{std::max(y, tile_y * tile_size)};
            const auto bottom{std::min(y + image.height, (tile_y + 1) * tile_size)};
            if(left >= right || top >= bottom)
            {
                continue;
            }
            const auto source_row{[&](int row)
                                  {
                                      return &image.bytes[static_cast<std::size_t>(row - y) * image.row_bytes() +
                                                          static_cast<std::size_t>(left - x) * pixel_bytes];
                                  }};
            const auto span{static_cast<std::size_t>(right - left) * pixel_bytes};
            const TileIndex index{tile_x, tile_y};
            if(tile(index) == nullptr)
            {
                // Nothing over nothing makes no tile.
                auto blank{true};
                for(auto row{top}; blank && row < bottom; ++ row)
                {
                    const auto* const from{source_row(row)};
                    blank = std::all_of(from, from + span, [](std::uint8_t byte) { return byte == 0; });
                }
                if(blank)
                {
                    continue;
                }
            }
            auto& target{writable_tile(index)};
            for(auto row{top}; row < bottom; ++ row)
            {
                std::memcpy(target.row(row - tile_y * tile_size) + (left - tile_x * tile_size) * pixel_bytes,
                            source_row(row), span);
            }
            if(target.empty())
            {
                m_tiles.erase(index);
            }
        }
    }
}

Image Canvas::read(int x, int y, int width, int height) const
{
    Image image{m_format, width, height};
    const auto pixel_bytes{bytes_per_pixel(m_format)};
    for(auto tile_y{tile_of(y)}; tile_y <= tile_of(y + height - 1); ++ tile_y)
    {
        for(auto tile_x{tile_of(x)}; tile_x <= tile_of(x + width - 1); ++ tile_x)
        {
            const auto source{tile({tile_x, tile_y})};
            if(source == nullptr)
            {
                continue;
            }
            const auto left{std::max(x, tile_x * tile_size)};
            const auto right{std::min(x + width, (tile_x + 1) * tile_size)};
            const auto top{std::max(y, tile_y * tile_size)};
            const auto bottom{std::min(y + height, (tile_y + 1) * tile_size)};
            for(auto row{top}; row < bottom; ++ row)
            {
                std::memcpy(&image.bytes[static_cast<std::size_t>(row - y) * image.row_bytes() +
                                         static_cast<std::size_t>(left - x) * pixel_bytes],
                            source->row(row - tile_y * tile_size) + (left - tile_x * tile_size) * pixel_bytes,
                            static_cast<std::size_t>(right - left) * pixel_bytes);
            }
        }
    }
    return image;
}

Image to_rgba8(const Canvas& canvas)
{
    return converted(canvas.read(0, 0, canvas.width(), canvas.height()), PixelFormat::rgba8);
}

Canvas from_rgba8(int width, int height, const std::uint8_t* pixels, PixelFormat format)
{
    Image image{PixelFormat::rgba8, width, height};
    if(!image.bytes.empty())
    {
        std::memcpy(image.bytes.data(), pixels, image.bytes.size());
    }
    Canvas canvas{format, width, height};
    canvas.write(converted(image, format), 0, 0);
    return canvas;
}

} // namespace olagarro::paint
