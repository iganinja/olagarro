#pragma once

#include <paint/arc.h>
#include <paint/floating.h>
#include <paint/tile_change.h>
#include <paint/tool.h>

#include <array>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace olagarro::paint
{

// What the pointer did, in the canvas's own pixels: (0.5, 0.5) is the middle of the top left one. Whatever shows the
// canvas turns its pointer into these.
struct ToolInput
{
    Position at;
    // The right button: the secondary colour, or another selection mode, or turning what floats.
    bool secondary{false};
    bool shift{false};
    bool control{false};
    bool alt{false};
    // From 0 to 1, as a pen gives it; a mouse is 1.
    double pressure{1.0};
};

// How a selection changed: its mask before and after, none where nothing was selected.
struct SelectionChange
{
    std::optional<Canvas> before;
    std::optional<Canvas> after;
};

// What one use of a tool did, to be one step of a program's history: the canvas's tiles, and the selection when the
// tool changed it.
struct ToolStep
{
    Tool tool{Tool::brush};
    TileChange change;
    std::optional<SelectionChange> selection;
};

// What to draw over the canvas while a tool works, in the canvas's pixels.
struct ToolOverlay
{
    // The canvas as it would be, shown in its place: a shape, a gradient or an arc before it is let go.
    std::optional<Canvas> preview;
    // Lines from one point to another: the selection's outline, a lasso being drawn, an arc's radius.
    std::vector<std::pair<Position, Position>> lines;
    // Points a press takes hold of: a line's ends and handles, what floats' corners and edges.
    std::vector<Position> handles;
};

// The edge of what is selected half or more, as lines between pixels; none when nothing is selected.
[[nodiscard]] std::vector<std::pair<Position, Position>> selection_outline(const Selection& selection);

// A tool used on one canvas and its selection: presses, drags and releases in, painting, selecting and moving out, one
// step at a time. The canvas and the selection must outlive it.
class ToolSession
{
  public:
    ToolSession(Canvas& canvas, Selection& selection);
    ToolSession(const ToolSession&) = delete;
    ToolSession& operator=(const ToolSession&) = delete;

    [[nodiscard]] Tool tool() const { return m_tool; }
    // Puts down what the tool in use was doing first, as finish() does.
    void set_tool(Tool tool);
    [[nodiscard]] ToolOptions& options() { return m_options; }
    [[nodiscard]] const ToolOptions& options() const { return m_options; }
    // How near a handle, in the canvas's pixels, a press takes it: a program sets it from its zoom.
    void set_handle_reach(double pixels) { m_handle_reach = pixels; }

    void press(const ToolInput& input);
    // The pointer moved with the button held: every move, for the tools that trace it, or the last of a frame.
    void drag(const ToolInput& input);
    void release(const ToolInput& input);
    // The pointer moved with no button held, which only the Arc tool, between its clicks, hears of.
    void hover(const ToolInput& input);

    // Whether something is under way: a button held, a line still to bend, an arc between its clicks, pixels floating.
    [[nodiscard]] bool busy() const;
    // Leaves what is under way as it is: a line bent is drawn, what floats is put down, an arc between its clicks is
    // left out.
    void finish();
    // Leaves out what is under way and not yet a step: an arc between its clicks, a line still to bend.
    void cancel();

    // The steps finished since last asked, oldest first.
    [[nodiscard]] std::vector<ToolStep> take_steps();
    // The canvas's tiles changed since last asked, to be drawn again.
    [[nodiscard]] std::vector<TileIndex> take_changed();
    [[nodiscard]] ToolOverlay overlay() const;

  private:
    // A gradient, line or shape being dragged: where it began, the canvas before it, which button, and the canvas with
    // it drawn as the pointer last was, shown until it is let go.
    struct Dragging
    {
        Position from;
        Canvas base;
        bool secondary{false};
        std::optional<Canvas> preview;
    };
    // A line just drawn, still to bend: its four points, its ends and two handles between them, which drags move, and
    // what it was drawn with.
    struct LineEdit
    {
        Canvas base;
        std::array<Position, 4> points;
        Color color;
        double width{1.0};
        bool antialias{true};
        LineSettings settings;
        std::optional<std::size_t> held;
    };
    // An arc between its clicks: the canvas before it, the clicks so far, the button it began with, and the arc as it
    // would be drawn.
    struct ArcDrawing
    {
        Canvas base;
        ArcGesture gesture;
        bool secondary{false};
        std::optional<Canvas> preview;
    };
    // Pixels lifted by Move Selected Pixels: the canvas with the hole they left, what they are, and where they went.
    struct Floating
    {
        Canvas base;
        FloatingPiece piece;
        PieceTransform transform;
    };
    // A move being dragged: how, where it was pressed, the transform it began from, the handle pulled, the selection
    // as it was for Move Selection, and the point it turns round.
    struct Moving
    {
        enum class Kind
        {
            move,
            rotate,
            // Pulling one of the eight handles: 0 to 3 the corners from the top left round, 4 to 7 the edges from the
            // top round.
            scale,
            outline,
        };
        Kind kind{Kind::move};
        Position from;
        PieceTransform start;
        int handle{0};
        std::optional<Canvas> selection;
        Position pivot;
    };
    // A rectangle, an ellipse or a lasso being drawn to select: where it began, how it combines, the selection before
    // it, and a lasso's points.
    struct Selecting
    {
        Position from;
        SelectionMode mode{SelectionMode::replace};
        std::optional<Canvas> before;
        std::vector<Position> lasso;
    };

    // A step begins: the canvas and the selection as they are, to know what it changed.
    void begin_step();
    void end_step(bool selection_changed);
    void paint_press(const ToolInput& input);
    void drag_draw(const ToolInput& input, bool released);
    // The canvas as `base` with the line, shape or gradient being dragged drawn over it.
    [[nodiscard]] Canvas drawn_over(const Canvas& base, Position from, Position to, bool secondary) const;
    void use_arc(const ToolInput& input, bool click);
    [[nodiscard]] Canvas arc_over(const Canvas& base) const;
    void select_shape(const ToolInput& input, bool released);
    void move_selected(const ToolInput& input, bool released);
    void pick(const ToolInput& input);
    void redraw_line();
    void finish_line();
    void settle_floating();
    [[nodiscard]] std::array<Position, 8> floating_handles() const;
    [[nodiscard]] Position constrained(const ToolInput& input, Position from) const;

    Canvas& m_canvas;
    Selection& m_selection;
    Tool m_tool{Tool::brush};
    ToolOptions m_options;
    double m_handle_reach{6.0};
    // The canvas and the selection as the step under way began.
    std::optional<Canvas> m_step_canvas;
    std::optional<Canvas> m_step_selection;
    // The canvas as take_changed last saw it.
    Canvas m_shown;
    std::vector<ToolStep> m_steps;
    std::unique_ptr<Stroke> m_stroke;
    std::optional<Dragging> m_dragging;
    std::optional<LineEdit> m_line_edit;
    std::optional<ArcDrawing> m_arc;
    std::optional<Floating> m_floating;
    std::optional<Moving> m_moving;
    std::optional<Selecting> m_selecting;
    bool m_held{false};
};

} // namespace olagarro::paint
