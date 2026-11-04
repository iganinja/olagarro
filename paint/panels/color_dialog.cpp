#include <paint/panels/color_dialog.h>

#include <paint/color_math.h>

#include <oui/translation.h>
#include <oui/button.h>
#include <oui/grid.h>
#include <oui/label.h>
#include <oui/layout.h>
#include <oui/line_edit.h>
#include <oui/number_field.h>
#include <oui/painter.h>
#include <oui/theme.h>
#include <oui/window.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

namespace olagarro::paint
{

namespace
{

[[nodiscard]] std::uint32_t screen(const Color& color)
{
    const auto byte{[](double value)
                    { return static_cast<std::uint32_t>(std::lround(std::clamp(value, 0.0, 1.0) * 255.0)); }};
    return byte(color.red) << 16u | byte(color.green) << 8u | byte(color.blue);
}

// A rectangle painted by a colour for each place in it, from (0, 0) at the top left to (1, 1) at the bottom right,
// with a ring where the colour picked is; a click or a drag in it says where, pixel by pixel.
class ColorArea final : public oui::Widget
{
  public:
    using Paint = std::function<Color(double across, double down)>;
    using Picked = std::function<void(double across, double down)>;

    ColorArea(oui::Size cells, Paint paint, Picked picked)
        : m_cells{cells}, m_paint{std::move(paint)}, m_picked{std::move(picked)}
    {
    }

    void set_marker(double across, double down)
    {
        m_across = across;
        m_down = down;
    }

    [[nodiscard]] oui::Size measure(const oui::Constraints& constraints) const override
    {
        return constraints.constrain(m_cells);
    }

    void paint(oui::Painter& painter, const oui::Theme&) const override
    {
        m_pixels = painter.pixel_bounds(m_bounds);
        if(!m_pixels || m_pixels->width <= 1 || m_pixels->height <= 1)
        {
            return;
        }
        const auto width{oui::whole(m_pixels->width)};
        const auto height{oui::whole(m_pixels->height)};
        m_buffer.resize(static_cast<std::size_t>(width) * height);
        for(int y{}; y < height; ++ y)
        {
            for(int x{}; x < width; ++ x)
            {
                m_buffer[static_cast<std::size_t>(y) * width + x] =
                    screen(m_paint(static_cast<double>(x) / (width - 1), static_cast<double>(y) / (height - 1)));
            }
        }
        // A ring, black and white, where the colour picked is.
        const auto marker_x{static_cast<int>(std::lround(m_across * (width - 1)))};
        const auto marker_y{static_cast<int>(std::lround(m_down * (height - 1)))};
        for(int y{-5}; y <= 5; ++ y)
        {
            for(int x{-5}; x <= 5; ++ x)
            {
                const auto distance{std::hypot(x, y)};
                const auto px{marker_x + x};
                const auto py{marker_y + y};
                if(px < 0 || py < 0 || px >= width || py >= height || distance > 5.0 || distance < 3.0)
                {
                    continue;
                }
                m_buffer[static_cast<std::size_t>(py) * width + px] = distance < 4.0 ? 0xffffffu : 0x000000u;
            }
        }
        painter.draw_image(m_bounds, {m_buffer.data(), width, height, width}, {});
    }

    bool handle_event(const oui::Event& event) override
    {
        const auto* const mouse{std::get_if<oui::MouseEvent>(&event)};
        if(mouse == nullptr || !(mouse->pressed || mouse->dragging) || mouse->right)
        {
            return false;
        }
        double across{};
        double down{};
        if(mouse->pixel && m_pixels && m_pixels->width > 1 && m_pixels->height > 1)
        {
            across = (mouse->pixel->x - m_pixels->x) / (m_pixels->width - 1);
            down = (mouse->pixel->y - m_pixels->y) / (m_pixels->height - 1);
        }
        else
        {
            across = (mouse->x - m_bounds.x) / std::max(1.0, m_bounds.width - 1);
            down = (mouse->y - m_bounds.y) / std::max(1.0, m_bounds.height - 1);
        }
        m_picked(std::clamp(across, 0.0, 1.0), std::clamp(down, 0.0, 1.0));
        return true;
    }

    [[nodiscard]] bool takes_raw_pointer() const override { return true; }

  private:
    oui::Size m_cells;
    Paint m_paint;
    Picked m_picked;
    double m_across{0.0};
    double m_down{0.0};
    mutable std::optional<oui::Rect> m_pixels;
    mutable std::vector<std::uint32_t> m_buffer;
};

// Two squares side by side: the colour there was and the colour picked.
class Swatches final : public oui::Widget
{
  public:
    Swatches(const Color& before, const Color& now) : m_before{before}, m_now{now} {}

    [[nodiscard]] oui::Size measure(const oui::Constraints& constraints) const override
    {
        return constraints.constrain({12, 2});
    }

    void paint(oui::Painter& painter, const oui::Theme&) const override
    {
        const auto shown{[](const Color& color)
                         {
                             const auto byte{[&color](double value) {
                                 return static_cast<std::uint8_t>(
                                     std::lround((value * color.alpha + 0.8 * (1.0 - color.alpha)) * 255.0));
                             }};
                             return oui::Color{byte(color.red), byte(color.green), byte(color.blue)};
                         }};
        const auto half{std::floor(m_bounds.width / 2)};
        painter.fill_rect({m_bounds.x, m_bounds.y, half, m_bounds.height}, shown(m_before));
        painter.fill_rect({m_bounds.x + half, m_bounds.y, m_bounds.width - half, m_bounds.height}, shown(m_now));
    }

    void set_now(const Color& now) { m_now = now; }

  private:
    Color m_before;
    Color m_now;
};

} // namespace

std::optional<Color> ask_color(oui::Application& application, const std::string& title, const Color& start,
                               const std::function<void(const Color&)>& changed)
{
    // Hue kept apart, so it stays where it was when the colour turns grey.
    auto hsv{to_hsv(start)};
    auto color{start};

    auto window{oui::window(title)};
    auto& content{oui::column(*window)};
    content.set_gap(oui::Spacing::tight);
    auto& top{oui::row(content)};
    top.set_gap(oui::Spacing::tight);

    ColorArea* square{};
    ColorArea* hue_bar{};
    Swatches* swatches{};
    oui::LineEdit* hex{};
    std::vector<oui::NumberField*> fields;
    auto updating{false};
    std::function<void(bool)> refresh;

    square = &top.add(std::make_unique<ColorArea>(
        oui::Size{24, 10}, [&hsv](double across, double down) { return from_hsv({hsv.hue, across, 1.0 - down}); },
        [&](double across, double down)
        {
            hsv.saturation = across;
            hsv.value = 1.0 - down;
            color = from_hsv(hsv, color.alpha);
            refresh(true);
        }));
    hue_bar = &top.add(std::make_unique<ColorArea>(
        oui::Size{3, 10}, [](double, double down) { return from_hsv({360.0 * (1.0 - down), 1.0, 1.0}); },
        [&](double, double down)
        {
            hsv.hue = 360.0 * (1.0 - down);
            color = from_hsv(hsv, color.alpha);
            refresh(true);
        }));

    auto& side{oui::column(top)};
    side.set_gap(oui::Spacing::none);
    swatches = &side.add(std::make_unique<Swatches>(start, start));
    auto& form{oui::grid(side)};
    form.set_columns({oui::Length::content(), oui::Length::content()});
    form.set_gap(oui::Spacing::none);
    auto& hex_label{oui::label("He&x:", form)};
    hex = &oui::line_edit(form);
    hex_label.set_buddy(*hex);
    // Red, green, blue and alpha from 0 to 255, hue from 0 to 360, saturation and value from 0 to 100, grey from 0 to
    // 65535; each takes any value between, so 16-bit colours keep their precision.
    struct Field
    {
        std::string name;
        double maximum;
    };
    for(const auto& [name, maximum] :
        {Field{oui::translate("Dialogs", "&Red:"), 255}, Field{oui::translate("Dialogs", "&Green:"), 255},
         Field{oui::translate("Dialogs", "&Blue:"), 255}, Field{oui::translate("Dialogs", "&Alpha:"), 255},
         Field{oui::translate("Dialogs", "H&ue:"), 360}, Field{oui::translate("Dialogs", "&Saturation:"), 100},
         Field{oui::translate("Dialogs", "&Value:"), 100}, Field{oui::translate("Dialogs", "Gray, &16 bits:"), 65535}})
    {
        auto& label{oui::label(name, form)};
        auto& field{oui::number_field(form)};
        field.set_range(0, maximum);
        field.set_step(0);
        label.set_buddy(field.text_field());
        fields.push_back(&field);
    }

    refresh = [&](bool from_areas)
    {
        updating = true;
        square->set_marker(hsv.saturation, 1.0 - hsv.value);
        hue_bar->set_marker(0.5, 1.0 - hsv.hue / 360.0);
        swatches->set_now(color);
        hex->set_text(to_hex(color));
        const double values[]{color.red * 255.0,
                              color.green * 255.0,
                              color.blue * 255.0,
                              color.alpha * 255.0,
                              hsv.hue,
                              hsv.saturation * 100.0,
                              hsv.value * 100.0,
                              (0.299 * color.red + 0.587 * color.green + 0.114 * color.blue) * 65535.0};
        for(std::size_t index{}; index < fields.size(); ++ index)
        {
            fields[index]->set_value(values[index]);
        }
        static_cast<void>(from_areas);
        updating = false;
        if(changed)
        {
            changed(color);
        }
    };
    for(std::size_t index{}; index < fields.size(); ++ index)
    {
        static_cast<void>(fields[index]->on_changed(
            [&, index](double value)
            {
                if(updating)
                {
                    return;
                }
                switch(index)
                {
                case 0:
                    color.red = value / 255.0;
                    break;
                case 1:
                    color.green = value / 255.0;
                    break;
                case 2:
                    color.blue = value / 255.0;
                    break;
                case 3:
                    color.alpha = value / 255.0;
                    break;
                case 7:
                    color.red = color.green = color.blue = value / 65535.0;
                    break;
                default:
                    break;
                }
                if(index >= 4 && index <= 6)
                {
                    (index == 4   ? hsv.hue
                     : index == 5 ? hsv.saturation
                                  : hsv.value) = index == 4 ? value : value / 100.0;
                    color = from_hsv(hsv, color.alpha);
                }
                else if(index != 3)
                {
                    const auto hue{hsv.hue};
                    hsv = to_hsv(color);
                    if(hsv.saturation <= 0.0)
                    {
                        hsv.hue = hue;
                    }
                }
                refresh(false);
            }));
    }
    static_cast<void>(hex->on_submitted(
        [&](std::string_view text)
        {
            if(const auto parsed{from_hex(text)})
            {
                color = *parsed;
                hsv = to_hsv(color);
            }
            refresh(false);
        }));
    refresh(false);

    auto& buttons{oui::row(content)};
    buttons.set_gap(oui::Spacing::normal);
    auto& window_reference{*window};
    auto accepted{false};
    oui::button(oui::translate("Dialogs", "OK"), buttons,
                [&]
                {
                    accepted = true;
                    window_reference.close(oui::DialogResult::accepted);
                })
        .set_default(true);
    oui::button(oui::translate("Dialogs", "Cancel"), buttons,
                [&] { window_reference.close(oui::DialogResult::cancelled); });
    static_cast<void>(application.show_modal_and_keep(window));
    if(!accepted)
    {
        return std::nullopt;
    }
    return color;
}

} // namespace olagarro::paint
