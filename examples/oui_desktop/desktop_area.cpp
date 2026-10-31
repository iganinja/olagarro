#include "desktop_area.h"

#include <oui/oui.h>

#include <algorithm>
#include <string>
#include <string_view>

using namespace olagarro;

namespace
{

using oui::Event;
using oui::KeyEvent;
using oui::MouseEvent;
using oui::Rect;
using oui::TextEvent;

[[nodiscard]] bool inside(const Rect& area, double x, double y)
{
    return x >= area.x && x < area.x + area.width && y >= area.y && y < area.y + area.height;
}

// A program's cells, as it drew them, in runs of the same look.
void paint_cells(oui::Painter& painter, const Rect& area, const oui::DesktopServer::Client& client)
{
    const auto rows{std::min(oui::whole(area.height), client.height)};
    const auto columns{std::min(oui::whole(area.width), client.width)};
    for(int row{}; row < rows; ++ row)
    {
        std::string run;
        oui::TextStyle style;
        int start{};
        const auto flush{[&]
                         {
                             if(!run.empty())
                             {
                                 painter.draw_text({area.x + start, area.y + row}, run, style);
                                 run.clear();
                             }
                         }};
        for(int column{}; column < columns; ++ column)
        {
            const auto& cell{client.cells[static_cast<std::size_t>(row * client.width + column)]};
            if(cell.continuation)
            {
                continue;
            }
            const oui::TextStyle look{cell.foreground, cell.background, cell.attributes};
            if(run.empty() || look.foreground != style.foreground || look.background != style.background ||
               look.attributes != style.attributes)
            {
                flush();
                style = look;
                start = column;
            }
            run += oui::encode_utf8(cell.glyph);
        }
        flush();
    }
}

} // namespace

DesktopArea::DesktopArea(oui::DesktopServer& server, bool cells) : m_server{server}, m_cells{cells}
{
    m_connections.emplace_back(m_server.on_connected([this](int id) { add(id); }));
    m_connections.emplace_back(m_server.on_disconnected([this](int id) { remove(id); }));
}

std::vector<int> DesktopArea::windows() const
{
    std::vector<int> ids;
    for(const auto& frame : m_frames)
    {
        ids.push_back(frame.id);
    }
    return ids;
}

int DesktopArea::focused() const
{
    return m_frames.empty() || m_frames.back().minimized ? 0 : m_frames.back().id;
}

void DesktopArea::raise(int id)
{
    telling_focus(
        [this, id]
        {
            if(const auto found{find(id)}; found != m_frames.end())
            {
                found->minimized = false;
                std::rotate(found, found + 1, m_frames.end());
            }
        });
}

void DesktopArea::minimize(int id)
{
    telling_focus(
        [this, id]
        {
            if(const auto found{find(id)}; found != m_frames.end())
            {
                found->minimized = true;
                std::rotate(m_frames.begin(), found, found + 1);
            }
        });
}

void DesktopArea::toggle_maximized(int id)
{
    raise(id);
    const auto found{find(id)};
    if(found == m_frames.end())
    {
        return;
    }
    if(found->maximized)
    {
        found->bounds = keep_inside(found->restored);
    }
    else
    {
        found->restored = found->bounds;
        found->bounds = bounds();
    }
    found->maximized = !found->maximized;
    configure(id);
}

void DesktopArea::close_focused()
{
    if(focused() != 0)
    {
        m_server.ask_to_close(focused());
    }
}

void DesktopArea::cycle()
{
    if(m_frames.size() > 1)
    {
        raise(m_frames.front().id);
    }
}

bool DesktopArea::waiting_for_desktop_key() const
{
    return m_desktop_key;
}

oui::Size DesktopArea::measure(const oui::Constraints& constraints) const
{
    return constraints.constrain({std::max(1.0, constraints.max_width), std::max(1.0, constraints.max_height)});
}

void DesktopArea::arrange(const Rect& bounds)
{
    Widget::arrange(bounds);
    for(auto& frame : m_frames)
    {
        const auto before{frame.bounds};
        frame.bounds = frame.maximized ? bounds : keep_inside(frame.bounds);
        if(frame.bounds.width != before.width || frame.bounds.height != before.height)
        {
            configure(frame.id);
        }
    }
}

void DesktopArea::paint(oui::Painter& painter, const oui::Theme& theme) const
{
    if(m_cells)
    {
        // Turbo Vision's desktop: a light shade, all over.
        std::string row;
        for(int column{}; column < oui::whole(bounds().width); ++ column)
        {
            row += "░";
        }
        for(double y{bounds().y}; y < bounds().y + bounds().height; ++ y)
        {
            painter.draw_text({bounds().x, y}, row, {theme.background, theme.text});
        }
    }
    else
    {
        painter.fill_rect(bounds(), background);
    }
    for(const auto& frame : m_frames)
    {
        if(frame.minimized)
        {
            continue;
        }
        const auto* const client{m_server.client(frame.id)};
        oui::WindowLook look;
        look.title = client != nullptr ? std::string_view{client->title} : std::string_view{};
        // The window with the keys has its title bar lit; in characters, its frame doubled.
        look.double_border = m_cells && frame.id == focused();
        look.placing = !m_cells && frame.id == focused();
        look.resizable = true;
        look.closable = true;
        look.minimizable = true;
        look.maximizable = true;
        look.maximized = frame.maximized;
        look.background = theme.dialog_background;
        painter.draw_window(frame.bounds, look, theme);
        const auto area{content(frame.bounds)};
        if(client != nullptr && !client->pixels.empty())
        {
            painter.draw_image(area, {client->pixels.data(), client->width, client->height, client->width}, theme);
        }
        else
        {
            painter.fill_rect(area, theme.dialog_background);
            if(client != nullptr && !client->cells.empty())
            {
                paint_cells(painter, area, *client);
            }
        }
    }
}

bool DesktopArea::is_focusable() const
{
    return true;
}

bool DesktopArea::claims_key(const Event&) const
{
    return true;
}

bool DesktopArea::handle_event(const Event& event)
{
    if(const auto* const mouse{std::get_if<MouseEvent>(&event)})
    {
        return handle_mouse(*mouse);
    }
    if(own_key(event))
    {
        return true;
    }
    if(std::holds_alternative<KeyEvent>(event) || std::holds_alternative<TextEvent>(event) ||
       std::holds_alternative<oui::ModifiersEvent>(event))
    {
        if(focused() != 0)
        {
            m_server.send(focused(), event);
        }
        return true;
    }
    return false;
}

bool DesktopArea::own_key(const Event& event)
{
    const auto* const pressed{std::get_if<KeyEvent>(&event)};
    const auto* const typed{std::get_if<TextEvent>(&event)};
    if(m_desktop_key && (pressed != nullptr || typed != nullptr))
    {
        m_desktop_key = false;
        if(pressed != nullptr && pressed->key == oui::Key::f12)
        {
            // The window gets it.
            return false;
        }
        if(pressed != nullptr && pressed->key == oui::Key::tab)
        {
            cycle();
        }
        const auto letter{typed != nullptr ? typed->codepoint : U'\0'};
        if(letter == U't' && open_terminal)
        {
            open_terminal();
        }
        else if(letter == U'x')
        {
            close_focused();
        }
        else if(letter == U'm')
        {
            toggle_maximized(focused());
        }
        else if(letter == U'n')
        {
            minimize(focused());
        }
        else if(letter == U'q' && quit)
        {
            quit();
        }
        return true;
    }
    if(pressed != nullptr && pressed->key == oui::Key::f12 && !pressed->control && !pressed->alt && !pressed->shift)
    {
        m_desktop_key = true;
        return true;
    }
    if(const auto* const key{std::get_if<KeyEvent>(&event)})
    {
        if(key->key == oui::Key::tab && key->alt && !key->control)
        {
            cycle();
            return true;
        }
        if(key->key == oui::Key::f4 && key->alt && !key->control)
        {
            close_focused();
            return true;
        }
        if(key->key == oui::Key::f9 && key->alt && !key->control)
        {
            minimize(focused());
            return true;
        }
        if(key->key == oui::Key::f10 && key->alt && !key->control)
        {
            toggle_maximized(focused());
            return true;
        }
        if(key->key == oui::Key::backspace && key->alt && key->control && quit)
        {
            quit();
            return true;
        }
    }
    if(const auto* const text{std::get_if<TextEvent>(&event)}; text != nullptr && text->control && text->alt &&
                                                               (text->codepoint == U't' || text->codepoint == U'T') &&
                                                               open_terminal)
    {
        open_terminal();
        return true;
    }
    return false;
}

Rect DesktopArea::content(const Rect& frame) const
{
    if(m_cells)
    {
        return {frame.x + 1, frame.y + 1, std::max(0.0, frame.width - 2), std::max(0.0, frame.height - 2)};
    }
    return {frame.x, frame.y + 1, frame.width, std::max(0.0, frame.height - 1)};
}

DesktopArea::TitleButton DesktopArea::title_button_at(const Rect& frame, double x) const
{
    const auto right{frame.x + frame.width - 1};
    const auto width{m_cells ? 3.0 : 2.0};
    const auto end{m_cells ? right - 2 : right};
    for(const auto button : {TitleButton::close, TitleButton::maximize, TitleButton::minimize})
    {
        const auto slot{button == TitleButton::close ? 0 : button == TitleButton::maximize ? 1 : 2};
        const auto last{end - slot * width};
        if(x <= last && x > last - width)
        {
            return button;
        }
    }
    return TitleButton::none;
}

std::vector<DesktopArea::Frame>::iterator DesktopArea::find(int id)
{
    return std::find_if(m_frames.begin(), m_frames.end(), [id](const Frame& frame) { return frame.id == id; });
}

template <typename Change> void DesktopArea::telling_focus(Change change)
{
    const auto before{focused()};
    change();
    if(focused() != before)
    {
        configure(before);
        configure(focused());
    }
}

const DesktopArea::Frame* DesktopArea::frame_at(double x, double y) const
{
    for(auto frame{m_frames.rbegin()}; frame != m_frames.rend(); ++ frame)
    {
        const auto& area{frame->bounds};
        if(frame->minimized)
        {
            continue;
        }
        if(x >= area.x && x < area.x + area.width && y >= area.y && y < area.y + area.height)
        {
            return &*frame;
        }
    }
    return nullptr;
}

oui::Pointer DesktopArea::pointer_at(oui::Point point) const
{
    const Frame* frame{nullptr};
    if(m_drag == Drag::content)
    {
        for(const auto& each : m_frames)
        {
            frame = each.id == m_drag_id ? &each : frame;
        }
    }
    else if(const auto* const under{frame_at(point.x, point.y)};
            under != nullptr && inside(content(under->bounds), point.x, point.y))
    {
        frame = under;
    }
    const auto* const client{frame == nullptr ? nullptr : m_server.client(frame->id)};
    return client == nullptr ? oui::Pointer::arrow : client->pointer;
}

Rect DesktopArea::keep_inside(Rect frame) const
{
    const auto& area{bounds()};
    frame.width = std::clamp(frame.width, std::min(minimum_width, area.width), std::max(1.0, area.width));
    frame.height = std::clamp(frame.height, std::min(minimum_height, area.height), std::max(1.0, area.height));
    frame.x = std::clamp(frame.x, area.x, area.x + std::max(0.0, area.width - frame.width));
    frame.y = std::clamp(frame.y, area.y, area.y + std::max(0.0, area.height - frame.height));
    return frame;
}

void DesktopArea::configure(int id)
{
    if(const auto found{find(id)}; found != m_frames.end())
    {
        const auto area{content(found->bounds)};
        m_server.configure(id, oui::whole(area.width), oui::whole(area.height), id == focused());
    }
}

void DesktopArea::add(int id)
{
    const auto& area{bounds()};
    const auto step{static_cast<double>(m_frames.size() % 8)};
    telling_focus(
        [&]
        {
            m_frames.push_back({id, keep_inside({area.x + 2 + 4 * step, area.y + 1 + 2 * step,
                                                 std::min(100.0, area.width - 8), std::min(32.0, area.height - 4)})});
        });
    configure(id);
}

void DesktopArea::remove(int id)
{
    const auto found{find(id)};
    if(found == m_frames.end())
    {
        return;
    }
    telling_focus([this, found] { m_frames.erase(found); });
    if(m_drag_id == id)
    {
        m_drag = Drag::none;
    }
}

void DesktopArea::forward(int id, MouseEvent mouse)
{
    if(const auto found{find(id)}; found != m_frames.end())
    {
        const auto area{content(found->bounds)};
        mouse.x -= area.x;
        mouse.y -= area.y;
        m_server.send(id, mouse);
    }
}

bool DesktopArea::handle_mouse(const MouseEvent& mouse)
{
    if(m_drag != Drag::none && (mouse.dragging || mouse.released))
    {
        const auto found{find(m_drag_id)};
        if(found != m_frames.end())
        {
            const auto dx{mouse.x - m_grab_x};
            const auto dy{mouse.y - m_grab_y};
            if(m_drag == Drag::move)
            {
                found->bounds = keep_inside({m_start.x + dx, m_start.y + dy, m_start.width, m_start.height});
            }
            else if(m_drag == Drag::resize)
            {
                found->bounds = keep_inside({m_start.x, m_start.y, std::max(minimum_width, m_start.width + dx),
                                             std::max(minimum_height, m_start.height + dy)});
                configure(m_drag_id);
            }
            else
            {
                forward(m_drag_id, mouse);
            }
        }
        if(mouse.released)
        {
            m_drag = Drag::none;
        }
        return true;
    }
    const auto* const frame{frame_at(mouse.x, mouse.y)};
    if(frame == nullptr)
    {
        return mouse.pressed;
    }
    // Raising the window moves the frames: what is needed of this one is kept first.
    const auto id{frame->id};
    const auto area{frame->bounds};
    const auto maximized{frame->maximized};
    if(!mouse.pressed)
    {
        // The pointer passing over, or the wheel: the window under it hears, keys or not.
        if(inside(content(area), mouse.x, mouse.y))
        {
            forward(id, mouse);
        }
        return true;
    }
    raise(id);
    m_drag_id = id;
    m_grab_x = mouse.x;
    m_grab_y = mouse.y;
    m_start = area;
    if(mouse.y == area.y && !mouse.right)
    {
        const auto button{title_button_at(area, mouse.x)};
        if(button == TitleButton::close)
        {
            m_server.ask_to_close(id);
        }
        else if(button == TitleButton::maximize)
        {
            toggle_maximized(id);
        }
        else if(button == TitleButton::minimize)
        {
            minimize(id);
        }
        else if(mouse.clicks == 2)
        {
            toggle_maximized(id);
        }
        else if(!maximized)
        {
            m_drag = Drag::move;
        }
        return true;
    }
    if(mouse.y == area.y + area.height - 1 && mouse.x == area.x + area.width - 1 && !mouse.right && !maximized)
    {
        m_drag = Drag::resize;
    }
    else if(inside(content(area), mouse.x, mouse.y))
    {
        m_drag = Drag::content;
        forward(id, mouse);
    }
    return true;
}
