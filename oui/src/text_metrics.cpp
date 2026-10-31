#include <oui/text_layout.h>
#include <oui/text_metrics.h>
#include <oui/utf8.h>

#include <algorithm>
#include <cmath>

namespace olagarro::oui
{

namespace
{

const TextMetrics terminal_metrics;
const TextMetrics* current_metrics{&terminal_metrics};

} // namespace

int TextMetrics::cell_width() const
{
    return 1;
}

double TextMetrics::column_step() const
{
    return 1.0;
}

double TextMetrics::row_step() const
{
    return 1.0;
}

std::optional<Size> TextMetrics::cell_pixels() const
{
    return std::nullopt;
}

int TextMetrics::width(std::string_view text) const
{
    return text_width(text);
}

double TextMetrics::label_width(std::string_view text) const
{
    return static_cast<double>(text_width(text));
}

int TextMetrics::field_padding() const
{
    return 0;
}

int TextMetrics::popup_frame_rows() const
{
    return 1;
}

int TextMetrics::panel_gap() const
{
    return 0;
}
int TextMetrics::window_top_rows() const
{
    return 1;
}
int TextMetrics::window_side_columns() const
{
    return 2;
}

bool TextMetrics::window_close_button() const
{
    return false;
}

std::size_t TextMetrics::byte_at(std::string_view text, int position, bool through) const
{
    std::size_t index{};
    auto column{0};
    while(index < text.size())
    {
        const auto start{index};
        column += std::max(0, terminal_cell_width(decode_utf8(text, index)));
        if(column > position)
        {
            return through ? index : start;
        }
    }
    return text.size();
}

namespace
{

std::optional<DragImage> current_drag_image;

} // namespace

void set_drag_image(std::optional<DragImage> image)
{
    current_drag_image = std::move(image);
}

const std::optional<DragImage>& drag_image()
{
    return current_drag_image;
}

const TextMetrics& text_metrics()
{
    return *current_metrics;
}

void set_text_metrics(const TextMetrics* metrics)
{
    current_metrics = metrics != nullptr ? metrics : &terminal_metrics;
}

double snap(double value, double step)
{
    return std::floor(value / step + 1e-6) * step;
}

double snap_up(double value, double step)
{
    return std::ceil(value / step - 1e-6) * step;
}

double snap_x(double value)
{
    return snap(value, text_metrics().column_step());
}

double snap_y(double value)
{
    return snap(value, text_metrics().row_step());
}

namespace
{

[[nodiscard]] double length_in_cells(Length length, std::optional<double> cell_pixels, double step)
{
    switch(length.kind())
    {
    case Length::Kind::cells:
        return std::max(0.0, static_cast<double>(length.value()));
    case Length::Kind::pixels:
        if(cell_pixels && *cell_pixels > 0)
        {
            return std::max(0.0, snap(length.pixels() / *cell_pixels, step));
        }
        return std::max(0.0, static_cast<double>(length.value()));
    default:
        return 0.0;
    }
}

} // namespace

double length_across(Length length)
{
    const auto& metrics{text_metrics()};
    const auto cell{metrics.cell_pixels()};
    return length_in_cells(length, cell ? std::optional{cell->width} : std::nullopt, metrics.column_step());
}

double length_down(Length length)
{
    const auto& metrics{text_metrics()};
    const auto cell{metrics.cell_pixels()};
    return length_in_cells(length, cell ? std::optional{cell->height} : std::nullopt, metrics.row_step());
}

} // namespace olagarro::oui
