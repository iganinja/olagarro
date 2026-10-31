#include <oui/desktop.h>

#include "desktop_link.h"

#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace olagarro::oui
{

namespace
{

// A window of cells, on a desktop in a terminal.
class CellDesktopClient final : public Backend
{
  public:
    explicit CellDesktopClient(std::unique_ptr<DesktopLink> link) : m_link{std::move(link)} {}

    [[nodiscard]] Size size() const override
    {
        return {static_cast<double>(m_link->columns()), static_cast<double>(m_link->rows())};
    }
    std::optional<Event> poll_event() override { return m_link->poll_event(); }
    void wait_for_input(std::optional<std::chrono::steady_clock::time_point> deadline) override
    {
        m_link->wait_for_input(deadline);
    }
    void wake() override { m_link->wake(); }
    [[nodiscard]] bool keeps_clipboard() const override { return true; }
    [[nodiscard]] std::string clipboard_text() const override { return m_link->clipboard_text(); }
    void set_clipboard_text(std::string_view text) override { m_link->set_clipboard_text(text); }
    void set_title(std::string_view title) override { m_link->set_title(title); }
    void set_pointer(Pointer pointer) override { m_link->set_pointer(pointer); }

    void present(const CellBuffer& buffer) override
    {
        m_frame.resize(static_cast<std::size_t>(buffer.width()) * static_cast<std::size_t>(buffer.height()));
        auto* wire{m_frame.data()};
        for(int y{}; y < buffer.height(); ++ y)
        {
            for(int x{}; x < buffer.width(); ++ x)
            {
                const auto& cell{buffer.cell_at(x, y)};
                *wire++ = {static_cast<std::uint32_t>(cell.glyph),
                           {cell.foreground.red, cell.foreground.green, cell.foreground.blue},
                           {cell.background.red, cell.background.green, cell.background.blue},
                           cell.attributes,
                           static_cast<std::uint8_t>(cell.continuation ? 1 : 0)};
            }
        }
        m_link->show(m_frame.data(), m_frame.size() * sizeof(protocol::WireCell), buffer.width(), buffer.height());
    }

  private:
    std::unique_ptr<DesktopLink> m_link;
    std::vector<protocol::WireCell> m_frame;
};

} // namespace

std::unique_ptr<Backend> desktop_cell_backend(std::unique_ptr<DesktopLink> link)
{
    return std::make_unique<CellDesktopClient>(std::move(link));
}

std::unique_ptr<Backend> desktop_backend(DesktopClientOptions options)
{
    auto link{std::make_unique<DesktopLink>(std::move(options))};
    if(!link->shows_cells())
    {
        errno = ENOTSUP;
        desktop_failure("it shows pixels, and this build draws only characters; the build with a window or the "
                        "whole screen shows there");
    }
    return desktop_cell_backend(std::move(link));
}

} // namespace olagarro::oui
