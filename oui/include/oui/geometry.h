#pragma once

#include <algorithm>
#include <limits>

namespace olagarro::oui
{

struct Point
{
    int x{0};
    int y{0};
};

struct Size
{
    int width{0};
    int height{0};
};

struct Rect
{
    int x{0};
    int y{0};
    int width{0};
    int height{0};
};

struct Insets
{
    int left{0};
    int top{0};
    int right{0};
    int bottom{0};
};

struct Constraints
{
    int max_width{std::numeric_limits<int>::max()};
    int max_height{std::numeric_limits<int>::max()};

    [[nodiscard]] Size constrain(Size size) const
    {
        return {std::clamp(size.width, 0, max_width), std::clamp(size.height, 0, max_height)};
    }
};

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
