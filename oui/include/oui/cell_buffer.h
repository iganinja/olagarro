#pragma once

#include <oui/painter.h>
#include <vector>

namespace olagarro::oui
{

struct Cell
{
    char32_t glyph{U' '};
    Color foreground{255, 255, 255};
    Color background{0, 0, 0};
    std::uint8_t attributes{0};
    bool continuation{false};

    [[nodiscard]] constexpr bool operator==(const Cell& other) const
    {
        return glyph == other.glyph && foreground == other.foreground && background == other.background &&
               attributes == other.attributes && continuation == other.continuation;
    }

    [[nodiscard]] constexpr bool operator!=(const Cell& other) const { return !(*this == other); }
};

class CellBuffer
{
  public:
    CellBuffer(int width, int height);
    [[nodiscard]] int width() const;
    [[nodiscard]] int height() const;
    [[nodiscard]] const Cell& cell_at(int x, int y) const;
    void set_cell(int x, int y, const Cell& cell);
    void clear(const Cell& cell = {});

  private:
    [[nodiscard]] std::size_t index_of(int x, int y) const;

    int m_width;
    int m_height;
    std::vector<Cell> m_cells;
};

class CellBufferPainter final : public Painter
{
  public:
    explicit CellBufferPainter(CellBuffer& buffer);
    void fill_rect(const Rect& rect, Color color) override;
    void shade(const Rect& rect, Color foreground, Color background) override;
    void draw_text(Point position, std::string_view text, const TextStyle& style) override;
    void push_clip(const Rect& rect) override;
    void pop_clip() override;

  private:
    [[nodiscard]] static bool contains(const Rect& rect, Point point);
    [[nodiscard]] static Rect intersect(const Rect& first, const Rect& second);

    CellBuffer& m_buffer;
    Rect m_clip;
    std::vector<Rect> m_clips;
};

} // namespace olagarro::oui
