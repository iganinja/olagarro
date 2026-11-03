#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace olagarro::paint
{

// How a canvas's pixels are kept. Grey ones have alpha too, so a grey canvas can be see-through over another; a file
// of grey pixels all solid is written without it.
enum class PixelFormat
{
    gray_alpha8,
    rgba8,
    gray_alpha16,
    rgba16,
};

[[nodiscard]] constexpr int channel_count(PixelFormat format)
{
    return format == PixelFormat::gray_alpha8 || format == PixelFormat::gray_alpha16 ? 2 : 4;
}

[[nodiscard]] constexpr int bits_per_channel(PixelFormat format)
{
    return format == PixelFormat::gray_alpha8 || format == PixelFormat::rgba8 ? 8 : 16;
}

[[nodiscard]] constexpr std::size_t bytes_per_pixel(PixelFormat format)
{
    return static_cast<std::size_t>(channel_count(format) * bits_per_channel(format) / 8);
}

[[nodiscard]] constexpr bool is_gray(PixelFormat format)
{
    return channel_count(format) == 2;
}

[[nodiscard]] constexpr PixelFormat pixel_format(bool gray, int bits)
{
    if(gray)
    {
        return bits == 16 ? PixelFormat::gray_alpha16 : PixelFormat::gray_alpha8;
    }
    return bits == 16 ? PixelFormat::rgba16 : PixelFormat::rgba8;
}

// "RGBA, 16 bits per channel", for the person.
[[nodiscard]] std::string_view describe(PixelFormat format);

// A pixel of a format as its channels: grey and alpha, or red, green, blue and alpha.
template <typename Channel, int Channels> struct Pixel
{
    using ChannelType = Channel;
    static constexpr int channels{Channels};
    static constexpr Channel full{static_cast<Channel>(sizeof(Channel) == 1 ? 0xffu : 0xffffu)};
    std::array<Channel, Channels> value{};

    [[nodiscard]] Channel alpha() const { return value[Channels - 1]; }
};

using GrayAlpha8 = Pixel<std::uint8_t, 2>;
using Rgba8 = Pixel<std::uint8_t, 4>;
using GrayAlpha16 = Pixel<std::uint16_t, 2>;
using Rgba16 = Pixel<std::uint16_t, 4>;

template <typename PixelType> struct FormatTag
{
    using Type = PixelType;
};

// Calls `action` with a FormatTag of the pixel type `format` keeps, so a piece written once for any pixel type runs on
// the one at hand: action(FormatTag<Rgba16>{}).
template <typename Action> decltype(auto) visit_format(PixelFormat format, Action&& action)
{
    switch(format)
    {
    case PixelFormat::gray_alpha8:
        return action(FormatTag<GrayAlpha8>{});
    case PixelFormat::rgba8:
        return action(FormatTag<Rgba8>{});
    case PixelFormat::gray_alpha16:
        return action(FormatTag<GrayAlpha16>{});
    case PixelFormat::rgba16:
        break;
    }
    return action(FormatTag<Rgba16>{});
}

} // namespace olagarro::paint
