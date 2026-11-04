#pragma once

#include <paint/color.h>
#include <paint/fill.h>
#include <paint/gradient.h>
#include <paint/shapes.h>
#include <paint/stroke.h>

#include <optional>
#include <string_view>
#include <vector>

namespace olagarro::paint
{

// The tools a ToolSession drives, as OUI Paint's Tools panel has them.
enum class Tool
{
    brush,
    pencil,
    eraser,
    fill,
    gradient,
    // Line / Curve: a line, bent by its two handles once drawn, with arrowheads as LineSettings asks.
    line,
    // A rectangle, a rounded one or an ellipse, as ShapeSettings asks.
    shapes,
    arc,
    rectangle_select,
    ellipse_select,
    lasso_select,
    magic_wand,
    color_picker,
    // Move Selected Pixels: what is selected lifted, moved, turned and scaled, put down when done with.
    move_pixels,
    // Move Selection: the selection's outline only.
    move_selection,
};

// Every tool, in the order of OUI Paint's Tools panel.
[[nodiscard]] const std::vector<Tool>& all_tools();
// The tool's name in a program's settings and commands, as OUI Paint's: "brush", "rectangle_select".
[[nodiscard]] std::string_view tool_id(Tool tool);
[[nodiscard]] std::optional<Tool> tool_from_id(std::string_view id);

// What the tools paint with and how: the brush's width, hardness and antialiasing, a fill's tolerance, the shape and
// how it is filled, the gradient, and the two colours, the primary with the left button and the secondary with the
// right one.
struct ToolOptions
{
    BrushSettings brush;
    FillSettings fill;
    ShapeSettings shape;
    LineSettings line;
    GradientKind gradient{GradientKind::linear};
    // The gradient makes the canvas's own pixels see-through instead of painting colours.
    bool gradient_transparency{false};
    Color primary{0.0, 0.0, 0.0, 1.0};
    Color secondary{1.0, 1.0, 1.0, 1.0};
};

} // namespace olagarro::paint
