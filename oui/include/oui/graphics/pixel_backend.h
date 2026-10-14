#pragma once

#include <oui/backend.h>
#include <oui/graphics/canvas.h>
#include <oui/graphics/font.h>
#include <oui/graphics/graphical_painter.h>
#include <oui/text_metrics.h>

#include <filesystem>
#include <memory>
#include <vector>

namespace olagarro::oui
{

// What every graphical backend shares: it paints each frame in pixels with a graphical style, and lays the
// application out in cells of the cell font's size. A window, a full screen or an image in memory derive from it.
class PixelBackend : public Backend
{
  public:
    // Interface text is measured in its pixels while the backend lives, so text fields place their cursor by them.
    explicit PixelBackend(GraphicalStyle style = classic_style());
    ~PixelBackend() override;
    PixelBackend(const PixelBackend&) = delete;
    PixelBackend& operator=(const PixelBackend&) = delete;

    [[nodiscard]] Size size() const override;
    void render(const std::function<void(Painter&)>& paint) override;
    // Shows a buffer of cells as it is, in the cell font.
    void present(const CellBuffer& buffer) override;

    void set_style(GraphicalStyle style);
    [[nodiscard]] const GraphicalStyle& style() const;
    // The size of a cell in pixels.
    [[nodiscard]] Size cell_size() const;
    // How big text and cells are, from 0, the smallest, to zoom_levels() - 1; default_zoom() unless set. The
    // application lays itself out again in the cells the new size gives on its next frame.
    void set_zoom(int level);
    [[nodiscard]] int zoom() const;
    [[nodiscard]] static int zoom_levels();
    // The columns an arrow pointer `pointer_size` pixels big covers from its tip, `cell_offset` pixels into a cell
    // `cell_width` wide: an arrow reaches about three fifths of its size to the right of its tip, in the themes there
    // are.
    [[nodiscard]] static int columns_covered(int cell_offset, int cell_width, int pointer_size);
    [[nodiscard]] int pointer_columns(int cell_offset) const override;
    // The system's pointer's size in pixels; 24, the usual, unless a backend knows it.
    [[nodiscard]] virtual int pointer_size() const { return 24; }
    [[nodiscard]] static int default_zoom();
    [[nodiscard]] bool zooms() const override;
    bool zoom_by(int step) override;
    bool zoom_reset() override;
    // Where the application shows, in pixels: the whole canvas unless a backend keeps some of it for itself, as a
    // window does for its title bar.
    [[nodiscard]] const PixelRect& content_area() const;
    [[nodiscard]] const Canvas& canvas() const;

  protected:
    // The size of what shows the application, in pixels; what was drawn is lost. The content area becomes the whole
    // canvas.
    void resize(int width, int height);
    void set_content_area(const PixelRect& area);
    // Draws what the backend keeps for itself around the content area, after each frame.
    virtual void decorate(Canvas& canvas, const GraphicalStyle& style, Font& interface_font);
    // Called when the fonts change size, before the next frame.
    virtual void fonts_changed();
    [[nodiscard]] Font& interface_font();
    // Called after each frame is painted and decorated.
    virtual void show(const Canvas& canvas) = 0;
    // Decorates and shows the last frame again, as when only the decoration changed.
    void redecorate();

  private:
    void load_fonts();

    GraphicalStyle m_style;
    int m_zoom;
    std::unique_ptr<Font> m_cell_font;
    std::unique_ptr<Font> m_interface_font;
    Size m_cell;
    // After m_cell, which it is sized to.
    std::unique_ptr<Font> m_symbols;
    Canvas m_canvas;
    PixelRect m_content;
    std::unique_ptr<TextMetrics> m_metrics;
};

// A graphical backend that paints into memory and replays a script of events, for tests and pictures.
class ImageBackend final : public PixelBackend
{
  public:
    ImageBackend(Size pixels, std::vector<Event> events = {});

    void push(const Event& event);
    std::optional<Event> poll_event() override;
    [[nodiscard]] std::size_t frame_count() const;

  protected:
    void show(const Canvas& canvas) override;

  private:
    std::vector<Event> m_events;
    std::size_t m_next{};
    std::size_t m_frame_count{};
};

[[nodiscard]] std::unique_ptr<ImageBackend> image_backend(Size pixels, std::vector<Event> events = {});

// Writes the canvas as a PNG image, uncompressed.
void write_png(const Canvas& canvas, const std::filesystem::path& path);

} // namespace olagarro::oui
