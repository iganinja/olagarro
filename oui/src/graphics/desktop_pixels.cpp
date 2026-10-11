#include <oui/graphics/desktop.h>

#include "../desktop_link.h"

#include <oui/graphics/canvas.h>

#include <memory>
#include <string>
#include <utility>

namespace olagarro::oui
{

namespace
{

// A window of pixels, on a desktop on a screen or in a window.
class PixelDesktopClient final : public PixelBackend
{
  public:
    PixelDesktopClient(std::unique_ptr<DesktopLink> link, GraphicalStyle style)
        : PixelBackend{std::move(style)}, m_link{std::move(link)}
    {
        const auto cell{cell_size()};
        resize(m_link->columns() * cell.width, m_link->rows() * cell.height);
        m_link->resized = [this](int columns, int rows)
        {
            const auto size{cell_size()};
            resize(columns * size.width, rows * size.height);
        };
    }

    [[nodiscard]] bool reports_modifiers() const override { return true; }
    std::optional<Event> poll_event() override { return m_link->poll_event(); }
    void wait_for_input(std::optional<std::chrono::steady_clock::time_point> deadline) override
    {
        m_link->wait_for_input(deadline);
    }
    void wake() override { m_link->wake(); }
    // The desktop's clipboard, shared by all its programs: what was copied last in any of them.
    [[nodiscard]] bool keeps_clipboard() const override { return true; }
    [[nodiscard]] std::string clipboard_text() const override { return m_link->clipboard_text(); }
    void set_clipboard_text(std::string_view text) override { m_link->set_clipboard_text(text); }
    void set_title(std::string_view title) override { m_link->set_title(title); }
    void set_pointer(Pointer pointer) override { m_link->set_pointer(pointer); }

  protected:
    void show(const Canvas& canvas) override
    {
        m_link->show(canvas.pixels().data(), canvas.pixels().size() * sizeof(std::uint32_t), canvas.width(),
                     canvas.height());
    }

  private:
    std::unique_ptr<DesktopLink> m_link;
};

} // namespace

std::unique_ptr<Backend> desktop_backend(DesktopClientOptions options, GraphicalStyle style)
{
    auto link{std::make_unique<DesktopLink>(std::move(options))};
    if(link->shows_cells())
    {
        return desktop_cell_backend(std::move(link));
    }
    return std::make_unique<PixelDesktopClient>(std::move(link), std::move(style));
}

} // namespace olagarro::oui
