#include <oui/signature_tip.h>

#include <oui/painter.h>
#include <oui/theme.h>
#include <oui/utf8.h>

#include <algorithm>

namespace olagarro::oui
{

namespace
{

using oui::Point;
using oui::Rect;
using oui::Size;

// As wide as it grows, in cells, before it wraps.
constexpr int widest{80};

[[nodiscard]] int cells_of(std::string_view text)
{
    auto width{0};
    for(std::size_t index{}; index < text.size();)
    {
        width += oui::terminal_cell_width(oui::decode_utf8(text, index));
    }
    return width;
}

// The stretches of `text` that fit in `width` cells, broken after a space where there is one.
[[nodiscard]] std::vector<std::pair<std::size_t, std::size_t>> wrapped(std::string_view text, int width)
{
    std::vector<std::pair<std::size_t, std::size_t>> stretches;
    std::size_t start{};
    while(start < text.size())
    {
        auto cells{0};
        auto index{start};
        std::size_t after_space{};
        while(index < text.size())
        {
            auto next{index};
            const auto codepoint{oui::decode_utf8(text, next)};
            const auto cell_width{oui::terminal_cell_width(codepoint)};
            if(cells + cell_width > width && index > start)
            {
                break;
            }
            cells += cell_width;
            index = next;
            if(codepoint == U' ')
            {
                after_space = index;
            }
        }
        const auto end{index < text.size() && after_space > start ? after_space : index};
        stretches.emplace_back(start, end);
        start = end;
    }
    if(stretches.empty())
    {
        stretches.emplace_back(0, 0);
    }
    return stretches;
}

} // namespace

SignatureTip::SignatureTip(Point anchor) : m_anchor{anchor}
{
}

void SignatureTip::show(Signature signature, std::size_t argument)
{
    m_signature = std::move(signature);
    m_argument = argument;
}

void SignatureTip::set_anchor(Point anchor)
{
    m_anchor = anchor;
}

const Signature& SignatureTip::signature() const
{
    return m_signature;
}

std::optional<std::size_t> SignatureTip::parameter() const
{
    return m_signature.parameter_of(m_argument);
}

std::vector<SignatureTip::Row> SignatureTip::rows(int width) const
{
    std::vector<Row> found;
    for(const auto& [start, end] : wrapped(m_signature.text, width))
    {
        found.push_back({true, 0, start, end});
    }
    for(std::size_t line{}; line < m_signature.doc.size(); ++ line)
    {
        for(const auto& [start, end] : wrapped(m_signature.doc[line], width))
        {
            found.push_back({false, line, start, end});
        }
    }
    return found;
}

Size SignatureTip::measure(const oui::Constraints& constraints) const
{
    // A frame round it, and a cell of room each side inside.
    const auto room{std::max(1, std::min(widest, oui::whole(constraints.max_width) - 4))};
    auto inner{1};
    const auto shown{rows(room)};
    for(const auto& row : shown)
    {
        const std::string_view text{row.signature ? m_signature.text : m_signature.doc[row.line]};
        inner = std::max(inner, cells_of(text.substr(row.start, row.end - row.start)));
    }
    return constraints.constrain({static_cast<double>(inner + 4), static_cast<double>(shown.size() + 2)});
}

void SignatureTip::arrange(const Rect& screen)
{
    const auto size{measure({screen.width, screen.height})};
    m_rows = rows(oui::whole(size.width) - 4);
    // Its text starts in the anchor's column, the frame and its room before it.
    const auto x{std::clamp(m_anchor.x - 2, screen.x, screen.x + std::max(0.0, screen.width - size.width))};
    // A row apart from the line, for the shadow under it.
    auto y{m_anchor.y - size.height - 1};
    if(y < screen.y)
    {
        y = std::min(m_anchor.y + 1, screen.y + std::max(0.0, screen.height - size.height));
    }
    Widget::arrange({x, y, size.width, size.height});
}

void SignatureTip::paint(oui::Painter& painter, const oui::Theme& theme) const
{
    const auto& area{bounds()};
    painter.draw_popup(area, theme);
    const oui::TextStyle normal{theme.menu_foreground, theme.menu_background};
    const oui::TextStyle marked{theme.menu_foreground, theme.menu_background, oui::text_bold | oui::text_underline};
    const oui::TextStyle doc{theme.menu_foreground, theme.menu_background, oui::text_italic};
    const auto parameter{m_signature.parameter_of(m_argument)};
    const auto mark{parameter ? m_signature.parameters[*parameter] : std::pair<std::size_t, std::size_t>{}};
    for(std::size_t index{}; index < m_rows.size() && static_cast<int>(index) < area.height - 2; ++ index)
    {
        const auto& row{m_rows[index]};
        Point at{area.x + 2, area.y + 1 + static_cast<int>(index)};
        if(!row.signature)
        {
            painter.draw_text(at, std::string_view{m_signature.doc[row.line]}.substr(row.start, row.end - row.start),
                              doc);
            continue;
        }
        // In the cells' font, as code is; the parameter the argument goes to stands out.
        const std::string_view text{m_signature.text};
        const std::size_t cuts[]{row.start, std::clamp(mark.first, row.start, row.end),
                                 std::clamp(mark.second, row.start, row.end), row.end};
        for(std::size_t piece{}; piece < 3; ++ piece)
        {
            const auto stretch{text.substr(cuts[piece], cuts[piece + 1] - cuts[piece])};
            if(stretch.empty())
            {
                continue;
            }
            painter.draw_text(at, stretch, piece == 1 ? marked : normal);
            at.x += cells_of(stretch);
        }
    }
    painter.draw_popup_edge(area, theme);
}

bool SignatureTip::handle_event(const oui::Event& event)
{
    if(const auto* const key{std::get_if<oui::KeyEvent>(&event)})
    {
        if(key->key == oui::Key::escape && !key->control && !key->alt && !key->shift)
        {
            m_cancelled.fire();
            return true;
        }
        return false;
    }
    // The mouse over it is its own, and does nothing.
    return std::holds_alternative<oui::MouseEvent>(event);
}

} // namespace olagarro::oui
