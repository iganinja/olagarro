#include <paint/tool_session.h>

#include <algorithm>
#include <cmath>

namespace olagarro::paint
{

namespace
{

constexpr auto pi{3.14159265358979323846};
// Shift keeps angles to steps of 15 degrees.
constexpr auto angle_step{pi / 12.0};

// How a selection tool combines with what is selected, by the button and the keys held, as in Paint.NET.
[[nodiscard]] SelectionMode selection_mode(const ToolInput& input)
{
    if(input.secondary && input.alt)
    {
        return SelectionMode::intersect;
    }
    if(input.secondary && input.control)
    {
        return SelectionMode::invert;
    }
    if(input.control)
    {
        return SelectionMode::add;
    }
    if(input.alt)
    {
        return SelectionMode::subtract;
    }
    return SelectionMode::replace;
}

// The pixel a point is in.
[[nodiscard]] std::pair<int, int> pixel_at(Position at)
{
    return {static_cast<int>(std::floor(at.x)), static_cast<int>(std::floor(at.y))};
}

[[nodiscard]] bool within(Position a, Position b, double reach)
{
    return std::abs(a.x - b.x) <= reach && std::abs(a.y - b.y) <= reach;
}

} // namespace

std::vector<std::pair<Position, Position>> selection_outline(const Selection& selection)
{
    std::vector<std::pair<Position, Position>> lines;
    if(!selection.active())
    {
        return lines;
    }
    const auto area{selection.bounds()};
    const auto in{[&](int x, int y) {
        return x >= 0 && y >= 0 && x < selection.width() && y < selection.height() && selection.at(x, y) >= 128;
    }};
    // Along each line between two rows, the runs where one side is selected and the other not; then the same between
    // columns.
    for(auto y{area.y}; y <= area.y + area.height; ++ y)
    {
        std::optional<int> run;
        for(auto x{area.x}; x <= area.x + area.width; ++ x)
        {
            const auto edge{x < area.x + area.width && in(x, y - 1) != in(x, y)};
            if(edge && !run)
            {
                run = x;
            }
            else if(!edge && run)
            {
                lines.push_back({{static_cast<double>(*run), static_cast<double>(y)},
                                 {static_cast<double>(x), static_cast<double>(y)}});
                run.reset();
            }
        }
    }
    for(auto x{area.x}; x <= area.x + area.width; ++ x)
    {
        std::optional<int> run;
        for(auto y{area.y}; y <= area.y + area.height; ++ y)
        {
            const auto edge{y < area.y + area.height && in(x - 1, y) != in(x, y)};
            if(edge && !run)
            {
                run = y;
            }
            else if(!edge && run)
            {
                lines.push_back({{static_cast<double>(x), static_cast<double>(*run)},
                                 {static_cast<double>(x), static_cast<double>(y)}});
                run.reset();
            }
        }
    }
    return lines;
}

ToolSession::ToolSession(Canvas& canvas, Selection& selection)
    : m_canvas{canvas}, m_selection{selection}, m_shown{canvas}
{
}

void ToolSession::set_tool(Tool tool)
{
    if(tool == m_tool)
    {
        return;
    }
    finish();
    m_tool = tool;
}

bool ToolSession::busy() const
{
    return m_held || m_line_edit || m_arc || m_floating;
}

void ToolSession::begin_step()
{
    m_step_canvas = m_canvas;
    m_step_selection = m_selection.mask();
}

void ToolSession::end_step(bool selection_changed)
{
    if(!m_step_canvas)
    {
        return;
    }
    m_canvas.drop_empty_tiles();
    ToolStep step{m_tool, tile_change(*m_step_canvas, m_canvas), std::nullopt};
    if(selection_changed)
    {
        step.selection = SelectionChange{std::move(m_step_selection), m_selection.mask()};
    }
    m_step_canvas.reset();
    m_step_selection.reset();
    if(!step.change.empty() || step.selection)
    {
        m_steps.push_back(std::move(step));
    }
}

std::vector<ToolStep> ToolSession::take_steps()
{
    return std::exchange(m_steps, {});
}

std::vector<TileIndex> ToolSession::take_changed()
{
    auto tiles{tile_change(m_shown, m_canvas).tiles()};
    m_shown = m_canvas;
    return tiles;
}

void ToolSession::press(const ToolInput& input)
{
    // Another tool used puts down what floats; set_tool has left a line bent as it is.
    if(m_tool != Tool::move_pixels)
    {
        settle_floating();
    }
    if(m_tool != Tool::arc)
    {
        m_held = true;
    }
    switch(m_tool)
    {
    case Tool::brush:
    case Tool::pencil:
    case Tool::eraser:
        paint_press(input);
        return;
    case Tool::fill:
    {
        const auto [x, y]{pixel_at(input.at)};
        if(x < 0 || y < 0 || x >= m_canvas.width() || y >= m_canvas.height())
        {
            return;
        }
        begin_step();
        static_cast<void>(bucket_fill(m_canvas, m_selection, x, y,
                                      input.secondary ? m_options.secondary : m_options.primary, m_options.fill));
        end_step(false);
        return;
    }
    case Tool::gradient:
    case Tool::line:
    case Tool::shapes:
        if(m_tool == Tool::line && m_line_edit)
        {
            // A press on one of the line's points takes it, to bend the line; anywhere else leaves the line as it is
            // and starts another.
            for(std::size_t index{}; index < m_line_edit->points.size(); ++ index)
            {
                if(within(m_line_edit->points[index], input.at, m_handle_reach))
                {
                    m_line_edit->held = index;
                    return;
                }
            }
            finish_line();
        }
        begin_step();
        m_dragging = Dragging{input.at, m_canvas, input.secondary, std::nullopt};
        drag_draw(input, false);
        return;
    case Tool::arc:
        use_arc(input, true);
        return;
    case Tool::rectangle_select:
    case Tool::ellipse_select:
    case Tool::lasso_select:
        begin_step();
        m_selecting = Selecting{input.at, selection_mode(input), m_selection.mask(), {input.at}};
        return;
    case Tool::magic_wand:
    {
        const auto [x, y]{pixel_at(input.at)};
        if(x < 0 || y < 0 || x >= m_canvas.width() || y >= m_canvas.height())
        {
            return;
        }
        begin_step();
        m_selection.add_coverage(
            fill_region(m_canvas.read(0, 0, m_canvas.width(), m_canvas.height()), x, y, m_options.fill),
            selection_mode(input));
        end_step(true);
        return;
    }
    case Tool::color_picker:
        pick(input);
        return;
    case Tool::move_pixels:
    case Tool::move_selection:
        move_selected(input, false);
        return;
    }
}

void ToolSession::drag(const ToolInput& input)
{
    if(!m_held)
    {
        if(m_tool == Tool::arc)
        {
            use_arc(input, false);
        }
        return;
    }
    switch(m_tool)
    {
    case Tool::brush:
    case Tool::pencil:
    case Tool::eraser:
        if(m_stroke != nullptr)
        {
            m_stroke->line_to(input.at, input.pressure);
        }
        return;
    case Tool::gradient:
    case Tool::line:
    case Tool::shapes:
        drag_draw(input, false);
        return;
    case Tool::arc:
        use_arc(input, false);
        return;
    case Tool::rectangle_select:
    case Tool::ellipse_select:
    case Tool::lasso_select:
        select_shape(input, false);
        return;
    case Tool::color_picker:
        pick(input);
        return;
    case Tool::move_pixels:
    case Tool::move_selection:
        move_selected(input, false);
        return;
    case Tool::fill:
    case Tool::magic_wand:
        return;
    }
}

void ToolSession::release(const ToolInput& input)
{
    if(!m_held)
    {
        return;
    }
    m_held = false;
    switch(m_tool)
    {
    case Tool::brush:
    case Tool::pencil:
    case Tool::eraser:
        if(m_stroke != nullptr)
        {
            m_stroke->line_to(input.at, input.pressure);
            m_stroke.reset();
            end_step(false);
        }
        return;
    case Tool::gradient:
    case Tool::line:
    case Tool::shapes:
        drag_draw(input, true);
        return;
    case Tool::rectangle_select:
    case Tool::ellipse_select:
    case Tool::lasso_select:
        select_shape(input, true);
        return;
    case Tool::move_pixels:
    case Tool::move_selection:
        move_selected(input, true);
        return;
    case Tool::arc:
    case Tool::fill:
    case Tool::magic_wand:
    case Tool::color_picker:
        return;
    }
}

void ToolSession::hover(const ToolInput& input)
{
    if(m_tool == Tool::arc)
    {
        use_arc(input, false);
    }
}

void ToolSession::finish()
{
    // A drag under way ends where the pointer last was.
    if(m_stroke != nullptr)
    {
        m_stroke.reset();
        end_step(false);
    }
    if(m_dragging)
    {
        if(m_dragging->preview)
        {
            m_canvas = *m_dragging->preview;
        }
        m_dragging.reset();
        end_step(false);
    }
    if(m_selecting)
    {
        m_selecting.reset();
        end_step(true);
    }
    if(m_moving)
    {
        m_moving.reset();
        end_step(true);
    }
    m_held = false;
    finish_line();
    if(m_arc)
    {
        cancel();
    }
    settle_floating();
}

void ToolSession::cancel()
{
    const auto put_back{[this](const Canvas& base)
                        {
                            m_canvas = base;
                            m_step_canvas.reset();
                            m_step_selection.reset();
                        }};
    if(m_arc)
    {
        put_back(m_arc->base);
        m_arc.reset();
    }
    if(m_line_edit)
    {
        put_back(m_line_edit->base);
        m_line_edit.reset();
    }
    if(m_dragging)
    {
        put_back(m_dragging->base);
        m_dragging.reset();
        m_held = false;
    }
    finish();
}

void ToolSession::erase_selection()
{
    finish();
    if(!m_selection.active())
    {
        return;
    }
    begin_step();
    static_cast<void>(erase_selected(m_canvas, m_selection));
    end_step(false);
}

void ToolSession::deselect()
{
    finish();
    if(!m_selection.active())
    {
        return;
    }
    begin_step();
    m_selection.clear();
    end_step(true);
}

void ToolSession::flip_selection(bool horizontal)
{
    transform_selection(
        [horizontal](PieceTransform& transform)
        {
            // Mirrored in the image's frame: the turn goes the other way.
            transform.radians = -transform.radians;
            (horizontal ? transform.scale_x : transform.scale_y) *= -1.0;
        });
}

void ToolSession::turn_selection(int quarter_turns)
{
    transform_selection([quarter_turns](PieceTransform& transform) { transform.radians += quarter_turns * pi / 2.0; });
}

void ToolSession::transform_selection(const std::function<void(PieceTransform&)>& change)
{
    if(!m_floating && !m_selection.active())
    {
        return;
    }
    if(m_held || m_line_edit || m_arc)
    {
        // What a drag or another tool was doing is put down first, and with it what floated.
        finish();
        if(!m_selection.active())
        {
            return;
        }
    }
    begin_step();
    lift_floating();
    change(m_floating->transform);
    place_piece(m_canvas, m_floating->base, m_selection, m_floating->piece, m_floating->transform,
                m_options.brush.exact);
    end_step(true);
}

void ToolSession::paint_press(const ToolInput& input)
{
    auto settings{m_options.brush};
    if(m_tool == Tool::pencil)
    {
        settings = {1.0, 1.0, false, settings.exact};
    }
    begin_step();
    m_stroke =
        std::make_unique<Stroke>(m_canvas, m_selection, input.secondary ? m_options.secondary : m_options.primary,
                                 settings, m_tool == Tool::eraser ? StrokeMode::erase : StrokeMode::paint);
    m_stroke->line_to(input.at, input.pressure);
}

Position ToolSession::constrained(const ToolInput& input, Position from) const
{
    auto to{input.at};
    if(input.shift && m_tool == Tool::line)
    {
        const auto angle{std::round(std::atan2(to.y - from.y, to.x - from.x) / angle_step) * angle_step};
        const auto length{std::hypot(to.x - from.x, to.y - from.y)};
        to = {from.x + std::cos(angle) * length, from.y + std::sin(angle) * length};
    }
    else if(input.shift && m_tool == Tool::shapes)
    {
        // A square or a circle.
        const auto side{std::max(std::abs(to.x - from.x), std::abs(to.y - from.y))};
        to = {from.x + (to.x < from.x ? -side : side), from.y + (to.y < from.y ? -side : side)};
    }
    return to;
}

Canvas ToolSession::drawn_over(const Canvas& base, Position from, Position to, bool secondary) const
{
    // The right button swaps the colours, as in Paint.NET.
    const auto& first{secondary ? m_options.secondary : m_options.primary};
    const auto& second{secondary ? m_options.primary : m_options.secondary};
    const auto& brush{m_options.brush};
    auto drawn{base};
    if(m_tool == Tool::gradient)
    {
        static_cast<void>(draw_gradient(drawn, base, m_selection, m_options.gradient, from, to, first, second,
                                        m_options.gradient_transparency));
    }
    else if(m_tool == Tool::line)
    {
        static_cast<void>(draw_line(drawn, m_selection, from, to, first, brush.width, brush.antialias, m_options.line));
    }
    else
    {
        static_cast<void>(draw_shape(drawn, m_selection, m_options.shape.kind, m_options.shape.style, from, to, first,
                                     second, brush.width, brush.antialias));
    }
    return drawn;
}

void ToolSession::drag_draw(const ToolInput& input, bool released)
{
    if(m_tool == Tool::line && m_line_edit && m_line_edit->held)
    {
        m_line_edit->points[*m_line_edit->held] = input.at;
        redraw_line();
        if(released)
        {
            m_line_edit->held.reset();
        }
        return;
    }
    if(!m_dragging)
    {
        return;
    }
    const auto from{m_dragging->from};
    const auto to{constrained(input, from)};
    // Shown over the canvas until let go, then drawn into it.
    m_dragging->preview = drawn_over(m_dragging->base, from, to, m_dragging->secondary);
    if(!released)
    {
        return;
    }
    m_canvas = *m_dragging->preview;
    if(m_tool == Tool::line)
    {
        // Drawn, it waits to be bent: its handles a third and two thirds along it, one step until done with.
        const auto along{[&](double part)
                         { return Position{from.x + (to.x - from.x) * part, from.y + (to.y - from.y) * part}; }};
        const auto& brush{m_options.brush};
        m_line_edit = LineEdit{m_dragging->base,
                               {from, along(1.0 / 3.0), along(2.0 / 3.0), to},
                               m_dragging->secondary ? m_options.secondary : m_options.primary,
                               brush.width,
                               brush.antialias,
                               m_options.line,
                               std::nullopt};
        m_dragging.reset();
        return;
    }
    m_dragging.reset();
    end_step(false);
}

void ToolSession::redraw_line()
{
    auto& edit{*m_line_edit};
    m_canvas = edit.base;
    static_cast<void>(
        draw_curve(m_canvas, m_selection, edit.points, edit.color, edit.width, edit.antialias, edit.settings));
}

void ToolSession::finish_line()
{
    if(!m_line_edit)
    {
        return;
    }
    m_line_edit.reset();
    end_step(false);
}

Canvas ToolSession::arc_over(const Canvas& base) const
{
    const auto& arc{m_arc->gesture.arc()};
    const auto& brush{m_options.brush};
    auto drawn{base};
    static_cast<void>(draw_arc(drawn, m_selection, arc.center, arc.radius, arc.start, arc.sweep,
                               m_arc->secondary ? m_options.secondary : m_options.primary, brush.width,
                               brush.antialias));
    return drawn;
}

void ToolSession::use_arc(const ToolInput& input, bool click)
{
    if(!m_arc)
    {
        if(!click)
        {
            return;
        }
        begin_step();
        m_arc = ArcDrawing{m_canvas, {}, input.secondary, std::nullopt};
    }
    auto done{false};
    if(click)
    {
        done = m_arc->gesture.click(input.at, input.shift);
    }
    else
    {
        m_arc->gesture.move(input.at, input.shift);
    }
    if(m_arc->gesture.step() == ArcGesture::Step::sweep || done)
    {
        m_arc->preview = arc_over(m_arc->base);
    }
    if(done)
    {
        m_canvas = *m_arc->preview;
        m_arc.reset();
        end_step(false);
    }
}

void ToolSession::select_shape(const ToolInput& input, bool released)
{
    if(!m_selecting)
    {
        return;
    }
    auto& selecting{*m_selecting};
    m_selection.set_mask(selecting.before);
    if(m_tool == Tool::lasso_select)
    {
        selecting.lasso.push_back(input.at);
        m_selection.add_polygon(selecting.lasso, selecting.mode);
    }
    else
    {
        // From the pixel the press was on to the one the pointer is on, both whole; with Shift, a square.
        auto left{std::floor(std::min(selecting.from.x, input.at.x))};
        auto top{std::floor(std::min(selecting.from.y, input.at.y))};
        auto right{std::ceil(std::max(selecting.from.x, input.at.x))};
        auto bottom{std::ceil(std::max(selecting.from.y, input.at.y))};
        if(input.shift)
        {
            const auto side{std::max(right - left, bottom - top)};
            if(input.at.x < selecting.from.x)
            {
                left = right - side;
            }
            else
            {
                right = left + side;
            }
            if(input.at.y < selecting.from.y)
            {
                top = bottom - side;
            }
            else
            {
                bottom = top + side;
            }
        }
        const auto clicked{right - left <= 1.0 && bottom - top <= 1.0};
        if(!clicked && m_tool == Tool::ellipse_select)
        {
            m_selection.add_ellipse(left, top, right, bottom, selecting.mode);
        }
        else if(!clicked)
        {
            m_selection.add_rectangle({static_cast<int>(left), static_cast<int>(top), static_cast<int>(right - left),
                                       static_cast<int>(bottom - top)},
                                      selecting.mode);
        }
        // A click without dragging selects nothing, as in Paint.NET.
        if(released && clicked && selecting.mode == SelectionMode::replace)
        {
            m_selection.clear();
        }
    }
    if(released)
    {
        m_selecting.reset();
        end_step(true);
    }
}

void ToolSession::pick(const ToolInput& input)
{
    const auto [x, y]{pixel_at(input.at)};
    if(x < 0 || y < 0 || x >= m_canvas.width() || y >= m_canvas.height())
    {
        return;
    }
    const auto pixel{converted(m_canvas.read(x, y, 1, 1), PixelFormat::rgba16).pixel<Rgba16>(0, 0)};
    const Color color{pixel.value[0] / 65535.0, pixel.value[1] / 65535.0, pixel.value[2] / 65535.0,
                      pixel.value[3] / 65535.0};
    (input.secondary ? m_options.secondary : m_options.primary) = color;
}

void ToolSession::settle_floating()
{
    // Its pixels are down already, where it was last moved: only the hole under it is forgotten.
    m_floating.reset();
}

void ToolSession::lift_floating()
{
    if(m_floating)
    {
        return;
    }
    // Lifts what is selected, leaving a hole: the whole canvas when nothing is selected.
    Floating floating{m_canvas, lift_piece(m_canvas, m_selection), {}};
    static_cast<void>(erase_selected(floating.base, m_selection));
    m_floating = std::move(floating);
}

std::array<Position, 8> ToolSession::floating_handles() const
{
    std::array<Position, 4> corners;
    if(m_floating)
    {
        corners = piece_corners(m_floating->piece, m_floating->transform);
    }
    else
    {
        // Nothing lifted yet: where the selection is, which is what a press lifts.
        const auto area{m_selection.bounds()};
        const auto left{static_cast<double>(area.x)};
        const auto top{static_cast<double>(area.y)};
        const auto right{static_cast<double>(area.x + area.width)};
        const auto bottom{static_cast<double>(area.y + area.height)};
        corners = {Position{left, top}, Position{right, top}, Position{right, bottom}, Position{left, bottom}};
    }
    std::array<Position, 8> handles;
    for(std::size_t index{}; index < 4; ++ index)
    {
        const auto& a{corners[index]};
        const auto& b{corners[(index + 1) % 4]};
        handles[index] = a;
        handles[index + 4] = {(a.x + b.x) / 2.0, (a.y + b.y) / 2.0};
    }
    return handles;
}

void ToolSession::move_selected(const ToolInput& input, bool released)
{
    if(!m_moving)
    {
        if(released)
        {
            return;
        }
        begin_step();
        if(m_tool == Tool::move_selection)
        {
            m_moving = Moving{Moving::Kind::outline, input.at, {}, 0, m_selection.mask(), input.at};
            return;
        }
        // A handle pressed scales; the right button turns round the middle as it shows; anything else moves.
        auto kind{input.secondary ? Moving::Kind::rotate : Moving::Kind::move};
        auto handle{0};
        if(!input.secondary && (m_floating || m_selection.active()))
        {
            const auto handles{floating_handles()};
            for(std::size_t index{}; index < handles.size(); ++ index)
            {
                if(within(handles[index], input.at, m_handle_reach))
                {
                    kind = Moving::Kind::scale;
                    handle = static_cast<int>(index);
                    break;
                }
            }
        }
        lift_floating();
        const auto& piece{m_floating->piece};
        const Position middle{piece.x + piece.pixels.width / 2.0 + m_floating->transform.dx,
                              piece.y + piece.pixels.height / 2.0 + m_floating->transform.dy};
        m_moving = Moving{kind, input.at, m_floating->transform, handle, std::nullopt, middle};
        return;
    }
    if(m_moving->kind == Moving::Kind::outline)
    {
        m_selection.set_mask(m_moving->selection);
        m_selection.offset(static_cast<int>(std::lround(input.at.x - m_moving->from.x)),
                           static_cast<int>(std::lround(input.at.y - m_moving->from.y)));
    }
    else
    {
        const auto& start{m_moving->start};
        auto& transform{m_floating->transform};
        const auto& piece{m_floating->piece};
        switch(m_moving->kind)
        {
        case Moving::Kind::move:
            // By whole pixels, so a piece moved and nothing else keeps its pixels exactly.
            transform.dx = start.dx + std::round(input.at.x - m_moving->from.x);
            transform.dy = start.dy + std::round(input.at.y - m_moving->from.y);
            break;
        case Moving::Kind::rotate:
        {
            const auto& round{m_moving->pivot};
            auto turned{std::atan2(input.at.y - round.y, input.at.x - round.x) -
                        std::atan2(m_moving->from.y - round.y, m_moving->from.x - round.x)};
            if(input.shift)
            {
                turned = std::round((start.radians + turned) / angle_step) * angle_step - start.radians;
            }
            transform = turned_round(piece, start, round, turned);
            break;
        }
        case Moving::Kind::scale:
        {
            // In the piece's own frame, unturned: the handle follows the pointer and the one across stays.
            const auto center_x{piece.x + piece.pixels.width / 2.0 + start.dx};
            const auto center_y{piece.y + piece.pixels.height / 2.0 + start.dy};
            const auto cosine{std::cos(start.radians)};
            const auto sine{std::sin(start.radians)};
            const auto px{input.at.x - center_x};
            const auto py{input.at.y - center_y};
            const auto local_x{px * cosine + py * sine};
            const auto local_y{-px * sine + py * cosine};
            const auto half_width{piece.pixels.width / 2.0};
            const auto half_height{piece.pixels.height / 2.0};
            // Which way the handle is from the middle, -1, 0 or 1 on each axis.
            constexpr int across[8]{-1, 1, 1, -1, 0, 1, 0, -1};
            constexpr int down[8]{-1, -1, 1, 1, -1, 0, 1, 0};
            const auto hx{across[m_moving->handle]};
            const auto hy{down[m_moving->handle]};
            auto scale_x{start.scale_x};
            auto scale_y{start.scale_y};
            auto middle_x{0.0};
            auto middle_y{0.0};
            if(hx != 0)
            {
                const auto fixed{-hx * half_width * start.scale_x};
                scale_x = (local_x - fixed) / (2.0 * hx * half_width);
                middle_x = (local_x + fixed) / 2.0;
            }
            if(hy != 0)
            {
                const auto fixed{-hy * half_height * start.scale_y};
                scale_y = (local_y - fixed) / (2.0 * hy * half_height);
                middle_y = (local_y + fixed) / 2.0;
            }
            if(input.shift && hx != 0 && hy != 0 && start.scale_x != 0.0)
            {
                // Its proportions kept, by the larger change.
                const auto ratio{start.scale_y / start.scale_x};
                if(std::abs(scale_x / start.scale_x) > std::abs(scale_y / start.scale_y))
                {
                    scale_y = scale_x * ratio;
                }
                else
                {
                    scale_x = scale_y / ratio;
                }
                middle_x = (hx * half_width * scale_x - hx * half_width * start.scale_x) / 2.0;
                middle_y = (hy * half_height * scale_y - hy * half_height * start.scale_y) / 2.0;
            }
            transform.scale_x = std::abs(scale_x) < 1e-3 ? 1e-3 : scale_x;
            transform.scale_y = std::abs(scale_y) < 1e-3 ? 1e-3 : scale_y;
            // The middle moves with the handle, turned back into the image's frame.
            transform.dx = start.dx + middle_x * cosine - middle_y * sine;
            transform.dy = start.dy + middle_x * sine + middle_y * cosine;
            break;
        }
        case Moving::Kind::outline:
            break;
        }
        place_piece(m_canvas, m_floating->base, m_selection, piece, transform, m_options.brush.exact);
    }
    if(released)
    {
        m_moving.reset();
        end_step(true);
    }
}

ToolOverlay ToolSession::overlay() const
{
    ToolOverlay overlay;
    if(m_dragging)
    {
        overlay.preview = m_dragging->preview;
    }
    else if(m_arc)
    {
        overlay.preview = m_arc->preview;
    }
    overlay.lines = selection_outline(m_selection);
    if(m_selecting && m_tool == Tool::lasso_select)
    {
        const auto& lasso{m_selecting->lasso};
        for(std::size_t index{1}; index < lasso.size(); ++ index)
        {
            overlay.lines.emplace_back(lasso[index - 1], lasso[index]);
        }
    }
    if(m_arc && m_arc->gesture.step() == ArcGesture::Step::radius)
    {
        overlay.lines.emplace_back(m_arc->gesture.center(), m_arc->gesture.pointer());
    }
    if(m_line_edit)
    {
        overlay.handles.assign(m_line_edit->points.begin(), m_line_edit->points.end());
    }
    if(m_tool == Tool::move_pixels && (m_floating || m_selection.active()))
    {
        const auto handles{floating_handles()};
        overlay.handles.assign(handles.begin(), handles.end());
    }
    return overlay;
}

} // namespace olagarro::paint
