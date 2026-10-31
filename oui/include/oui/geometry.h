#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

namespace olagarro::oui
{

// Positions and sizes in cells: a 1 is a column across or a row down. They may fall between cells where a backend
// places things finer than a cell, as a window does by the pixel; a terminal keeps them whole.
struct Point
{
    double x{0.0};
    double y{0.0};
};

struct Size
{
    double width{0.0};
    double height{0.0};
};

struct Rect
{
    double x{0.0};
    double y{0.0};
    double width{0.0};
    double height{0.0};
};

struct Insets
{
    double left{0.0};
    double top{0.0};
    double right{0.0};
    double bottom{0.0};
};

struct Constraints
{
    // As large as an int, so code that takes them as one stays defined.
    double max_width{static_cast<double>(std::numeric_limits<int>::max())};
    double max_height{static_cast<double>(std::numeric_limits<int>::max())};

    [[nodiscard]] Size constrain(Size size) const
    {
        return {std::clamp(size.width, 0.0, max_width), std::clamp(size.height, 0.0, max_height)};
    }
};

// The whole cell a position falls in, as the row, the column, the character or the cell of a buffer it is: the
// tolerance takes a 2.9999999 that means 3 as 3.
[[nodiscard]] inline int whole(double value)
{
    return static_cast<int>(std::floor(value + 1e-6));
}

enum class Spacing
{
    none,
    tight,
    normal,
    loose
};

class Length
{
  public:
    enum class Kind
    {
        content,
        fill,
        flex,
        percent,
        cells
    };

    [[nodiscard]] static constexpr Length content() { return {Kind::content, 0.0f}; }
    [[nodiscard]] static constexpr Length fill() { return {Kind::fill, 1.0f}; }
    [[nodiscard]] static constexpr Length flex(float factor = 1.0f) { return {Kind::flex, factor}; }
    [[nodiscard]] static constexpr Length percent(float value) { return {Kind::percent, value}; }
    [[nodiscard]] static constexpr Length cells(int value) { return {Kind::cells, static_cast<float>(value)}; }

    [[nodiscard]] constexpr Kind kind() const { return m_kind; }
    [[nodiscard]] constexpr float value() const { return m_value; }

  private:
    constexpr Length(Kind kind, float value) : m_kind{kind}, m_value{value} {}

    Kind m_kind;
    float m_value;
};

[[nodiscard]] constexpr int spacing_cells(Spacing spacing)
{
    switch(spacing)
    {
    case Spacing::none:
        return 0;
    case Spacing::tight:
        return 1;
    case Spacing::normal:
        return 2;
    case Spacing::loose:
        return 3;
    }

    return 0;
}

} // namespace olagarro::oui
