#include <paint/pixel_format.h>

namespace olagarro::paint
{

std::string_view describe(PixelFormat format)
{
    switch(format)
    {
    case PixelFormat::gray_alpha8:
        return "Grayscale, 8 bits per channel";
    case PixelFormat::rgba8:
        return "RGBA, 8 bits per channel";
    case PixelFormat::gray_alpha16:
        return "Grayscale, 16 bits per channel";
    case PixelFormat::rgba16:
        break;
    }
    return "RGBA, 16 bits per channel";
}

} // namespace olagarro::paint
