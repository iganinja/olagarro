#pragma once

namespace olagarro::paint
{

// A point in a canvas's pixels, which may fall between them: (0.5, 0.5) is the middle of the top left one.
struct Position
{
    double x{0.0};
    double y{0.0};
};

// A rectangle of pixels.
struct PixelArea
{
    int x{0};
    int y{0};
    int width{0};
    int height{0};

    [[nodiscard]] bool empty() const { return width <= 0 || height <= 0; }
};

} // namespace olagarro::paint
