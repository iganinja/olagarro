#pragma once

#include <paint/pixel_format.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace olagarro::paint
{

// Pixels of one format in one block, row after row with nothing between them, channels in the order the format
// names them, 16-bit channels in the machine's order: what files are read into and written from.
struct Image
{
    PixelFormat format{PixelFormat::rgba8};
    int width{0};
    int height{0};
    std::vector<std::uint8_t> bytes;

    Image() = default;
    // All channels zero: see-through black.
    Image(PixelFormat format_given, int width_given, int height_given);

    [[nodiscard]] std::size_t row_bytes() const { return static_cast<std::size_t>(width) * bytes_per_pixel(format); }

    template <typename PixelType> [[nodiscard]] PixelType pixel(int x, int y) const
    {
        PixelType result;
        std::memcpy(result.value.data(), &bytes[static_cast<std::size_t>(y) * row_bytes() + x * sizeof(PixelType)],
                    sizeof(PixelType));
        return result;
    }
    template <typename PixelType> void set_pixel(int x, int y, const PixelType& pixel)
    {
        std::memcpy(&bytes[static_cast<std::size_t>(y) * row_bytes() + x * sizeof(PixelType)], pixel.value.data(),
                    sizeof(PixelType));
    }
    // Whether every pixel is solid.
    [[nodiscard]] bool is_opaque() const;
};

// `image` in another format. 8 bits become 16 by repeating the byte (0xab as 0xabab), 16 become 8 rounded to the
// nearest; colour becomes grey by its luma (Rec. 601), and grey becomes colour with the same value in all three.
[[nodiscard]] Image converted(const Image& image, PixelFormat format);

} // namespace olagarro::paint
