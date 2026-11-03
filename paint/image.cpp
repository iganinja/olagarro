#include <paint/image.h>

#include <algorithm>
#include <cmath>

namespace olagarro::paint
{

namespace
{

// A channel as a number from 0 to 1.
template <typename Channel> [[nodiscard]] double unit(Channel value)
{
    return static_cast<double>(value) / (sizeof(Channel) == 1 ? 255.0 : 65535.0);
}

template <typename Channel> [[nodiscard]] Channel channel(double value)
{
    const auto full{sizeof(Channel) == 1 ? 255.0 : 65535.0};
    return static_cast<Channel>(std::lround(std::clamp(value, 0.0, 1.0) * full));
}

template <typename To, typename From> [[nodiscard]] To channel_as(From value)
{
    if constexpr(sizeof(To) == sizeof(From))
    {
        return static_cast<To>(value);
    }
    else if constexpr(sizeof(To) == 2)
    {
        return static_cast<To>(value * 257u);
    }
    else
    {
        return static_cast<To>((static_cast<unsigned>(value) * 255u + 32767u) / 65535u);
    }
}

template <typename To, typename From> [[nodiscard]] To pixel_as(const From& from)
{
    using ToChannel = typename To::ChannelType;
    To to;
    if constexpr(To::channels == From::channels)
    {
        for(int index{}; index < To::channels; ++ index)
        {
            to.value[index] = channel_as<ToChannel>(from.value[index]);
        }
    }
    else if constexpr(To::channels == 4)
    {
        const auto gray{channel_as<ToChannel>(from.value[0])};
        to.value = {gray, gray, gray, channel_as<ToChannel>(from.value[1])};
    }
    else
    {
        const auto luma{0.299 * unit(from.value[0]) + 0.587 * unit(from.value[1]) + 0.114 * unit(from.value[2])};
        to.value = {channel<ToChannel>(luma), channel_as<ToChannel>(from.value[3])};
    }
    return to;
}

} // namespace

Image::Image(PixelFormat format_given, int width_given, int height_given)
    : format{format_given}, width{width_given}, height{height_given},
      bytes(static_cast<std::size_t>(width_given) * static_cast<std::size_t>(height_given) *
            bytes_per_pixel(format_given))
{
}

bool Image::is_opaque() const
{
    return visit_format(format,
                        [this](auto tag)
                        {
                            using PixelType = typename decltype(tag)::Type;
                            for(int y{}; y < height; ++ y)
                            {
                                for(int x{}; x < width; ++ x)
                                {
                                    if(pixel<PixelType>(x, y).alpha() != PixelType::full)
                                    {
                                        return false;
                                    }
                                }
                            }
                            return true;
                        });
}

Image converted(const Image& image, PixelFormat format)
{
    if(image.format == format)
    {
        return image;
    }
    Image result{format, image.width, image.height};
    visit_format(image.format,
                 [&](auto from_tag)
                 {
                     using From = typename decltype(from_tag)::Type;
                     visit_format(format,
                                  [&](auto to_tag)
                                  {
                                      using To = typename decltype(to_tag)::Type;
                                      for(int y{}; y < image.height; ++ y)
                                      {
                                          for(int x{}; x < image.width; ++ x)
                                          {
                                              result.set_pixel(x, y, pixel_as<To>(image.pixel<From>(x, y)));
                                          }
                                      }
                                  });
                 });
    return result;
}

} // namespace olagarro::paint
