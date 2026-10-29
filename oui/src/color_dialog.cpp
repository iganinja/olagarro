#include <oui/button.h>
#include <oui/color_dialog.h>
#include <oui/grid.h>
#include <oui/label.h>
#include <oui/layout.h>
#include <oui/line_edit.h>
#include <oui/number_field.h>
#include <oui/painter.h>
#include <oui/theme.h>
#include <oui/translation.h>
#include <oui/window.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace olagarro::oui
{

namespace
{

// A colour between bytes, each channel from 0 to 1, so the square and the bar move it finer than a byte.
struct Rgb
{
    double red{0.0};
    double green{0.0};
    double blue{0.0};
};

// Hue from 0 to 360, saturation and value from 0 to 1.
struct Hsv
{
    double hue{0.0};
    double saturation{0.0};
    double value{0.0};
};

[[nodiscard]] Rgb to_rgb(Color color)
{
    return {color.red / 255.0, color.green / 255.0, color.blue / 255.0};
}

[[nodiscard]] Color to_color(const Rgb& rgb)
{
    const auto byte{[](double value)
                    { return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0, 1.0) * 255.0)); }};
    return {byte(rgb.red), byte(rgb.green), byte(rgb.blue)};
}

[[nodiscard]] Hsv to_hsv(const Rgb& rgb)
{
    const auto high{std::max({rgb.red, rgb.green, rgb.blue})};
    const auto low{std::min({rgb.red, rgb.green, rgb.blue})};
    const auto span{high - low};
    Hsv hsv{0.0, high > 0.0 ? span / high : 0.0, high};
    if(span <= 0.0)
    {
        return hsv;
    }
    if(high == rgb.red)
    {
        hsv.hue = 60.0 * std::fmod((rgb.green - rgb.blue) / span + 6.0, 6.0);
    }
    else if(high == rgb.green)
    {
        hsv.hue = 60.0 * ((rgb.blue - rgb.red) / span + 2.0);
    }
    else
    {
        hsv.hue = 60.0 * ((rgb.red - rgb.green) / span + 4.0);
    }
    return hsv;
}

[[nodiscard]] Rgb from_hsv(const Hsv& hsv)
{
    const auto hue{std::fmod(std::fmod(hsv.hue, 360.0) + 360.0, 360.0) / 60.0};
    const auto chroma{hsv.value * hsv.saturation};
    const auto second{chroma * (1.0 - std::abs(std::fmod(hue, 2.0) - 1.0))};
    const auto base{hsv.value - chroma};
    double red{};
    double green{};
    double blue{};
    switch(static_cast<int>(hue))
    {
    case 0:
        red = chroma, green = second;
        break;
    case 1:
        red = second, green = chroma;
        break;
    case 2:
        green = chroma, blue = second;
        break;
    case 3:
        green = second, blue = chroma;
        break;
    case 4:
        red = second, blue = chroma;
        break;
    default:
        red = chroma, blue = second;
        break;
    }
    return {red + base, green + base, blue + base};
}

[[nodiscard]] std::string to_hex(Color color)
{
    std::array<char, 8> text{};
    std::snprintf(text.data(), text.size(), "#%02x%02x%02x", color.red, color.green, color.blue);
    return text.data();
}

// "#rrggbb", or the six digits alone, in either case.
[[nodiscard]] std::optional<Color> from_hex(std::string_view text)
{
    if(!text.empty() && text.front() == '#')
    {
        text.remove_prefix(1);
    }
    if(text.size() != 6)
    {
        return std::nullopt;
    }
    const auto digit{[](char character) -> int
                     {
                         if(character >= '0' && character <= '9')
                         {
                             return character - '0';
                         }
                         if(character >= 'a' && character <= 'f')
                         {
                             return character - 'a' + 10;
                         }
                         if(character >= 'A' && character <= 'F')
                         {
                             return character - 'A' + 10;
                         }
                         return -1;
                     }};
    std::array<std::uint8_t, 3> bytes{};
    for(std::size_t index{}; index < bytes.size(); ++ index)
    {
        const auto high{digit(text[index * 2])};
        const auto low{digit(text[index * 2 + 1])};
        if(high < 0 || low < 0)
        {
            return std::nullopt;
        }
        bytes[index] = static_cast<std::uint8_t>(high * 16 + low);
    }
    return Color{bytes[0], bytes[1], bytes[2]};
}

// A rectangle painted by a colour for each place in it, from (0, 0) at the top left to (1, 1) at the bottom right,
// with a ring where the colour picked is. A click or a drag in it says where, pixel by pixel where there are pixels and
// cell by cell in a terminal; focused, the arrows move the ring by a step, ten with Shift.
class ColorArea final : public Widget
{
  public:
    using Paint = std::function<Rgb(double across, double down)>;
    using Picked = std::function<void(double across, double down)>;

    // `steps` across and down: how many places the arrows stop at from one side to the other; none across or down
    // for a bar that only goes the other way.
    ColorArea(Size cells, std::pair<int, int> steps, Paint paint, Picked picked)
        : m_cells{cells}, m_steps{steps}, m_paint{std::move(paint)}, m_picked{std::move(picked)}
    {
    }

    void set_marker(double across, double down)
    {
        m_across = across;
        m_down = down;
    }

    [[nodiscard]] Size measure(const Constraints& constraints) const override { return constraints.constrain(m_cells); }

    void paint(Painter& painter, const Theme& theme) const override
    {
        m_pixels = painter.pixel_bounds(m_bounds);
        if(m_pixels && m_pixels->width > 1 && m_pixels->height > 1)
        {
            paint_pixels(painter, theme);
        }
        else
        {
            paint_cells(painter);
        }
        painter.draw_field_frame(m_bounds, is_focused(), theme);
    }

    [[nodiscard]] bool is_focusable() const override { return is_enabled(); }

    [[nodiscard]] bool claims_key(const Event& key) const override
    {
        const auto* const pressed{std::get_if<KeyEvent>(&key)};
        return pressed != nullptr && step_of(*pressed).has_value();
    }

    bool handle_event(const Event& event) override
    {
        if(const auto* const key{std::get_if<KeyEvent>(&event)})
        {
            const auto step{step_of(*key)};
            if(!step)
            {
                return false;
            }
            const auto times{key->shift ? 10 : 1};
            const auto moved{
                [times](double at, int steps, int by)
                {
                    if(steps <= 0)
                    {
                        return at;
                    }
                    return std::clamp(std::round(at * steps + by * times), 0.0, static_cast<double>(steps)) / steps;
                }};
            m_picked(moved(m_across, m_steps.first, step->first), moved(m_down, m_steps.second, step->second));
            return true;
        }
        const auto* const mouse{std::get_if<MouseEvent>(&event)};
        if(mouse == nullptr || !(mouse->pressed || mouse->dragging) || mouse->right)
        {
            return false;
        }
        double across{};
        double down{};
        if(mouse->pixel && m_pixels && m_pixels->width > 1 && m_pixels->height > 1)
        {
            across = static_cast<double>(mouse->pixel->x - m_pixels->x) / (m_pixels->width - 1);
            down = static_cast<double>(mouse->pixel->y - m_pixels->y) / (m_pixels->height - 1);
        }
        else
        {
            across = static_cast<double>(mouse->x - m_bounds.x) / std::max(1, m_bounds.width - 1);
            down = static_cast<double>(mouse->y - m_bounds.y) / std::max(1, m_bounds.height - 1);
        }
        m_picked(std::clamp(across, 0.0, 1.0), std::clamp(down, 0.0, 1.0));
        return true;
    }

    [[nodiscard]] bool takes_raw_pointer() const override { return true; }

  private:
    // Which way an arrow moves the ring, a step across and a step down; none for another key or a way it cannot go.
    [[nodiscard]] std::optional<std::pair<int, int>> step_of(const KeyEvent& key) const
    {
        if(key.control || key.alt)
        {
            return std::nullopt;
        }
        switch(key.key)
        {
        case Key::left:
            return m_steps.first > 0 ? std::optional{std::pair{-1, 0}} : std::nullopt;
        case Key::right:
            return m_steps.first > 0 ? std::optional{std::pair{1, 0}} : std::nullopt;
        case Key::up:
            return m_steps.second > 0 ? std::optional{std::pair{0, -1}} : std::nullopt;
        case Key::down:
            return m_steps.second > 0 ? std::optional{std::pair{0, 1}} : std::nullopt;
        default:
            return std::nullopt;
        }
    }

    [[nodiscard]] static std::uint32_t packed(const Rgb& rgb)
    {
        const auto color{to_color(rgb)};
        return static_cast<std::uint32_t>(color.red) << 16u | static_cast<std::uint32_t>(color.green) << 8u |
               color.blue;
    }

    void paint_pixels(Painter& painter, const Theme& theme) const
    {
        const auto width{m_pixels->width};
        const auto height{m_pixels->height};
        m_buffer.resize(static_cast<std::size_t>(width) * height);
        for(int y{}; y < height; ++ y)
        {
            for(int x{}; x < width; ++ x)
            {
                m_buffer[static_cast<std::size_t>(y) * width + x] =
                    packed(m_paint(static_cast<double>(x) / (width - 1), static_cast<double>(y) / (height - 1)));
            }
        }
        // A ring, white inside black, where the colour picked is.
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
        painter.draw_image(m_bounds, {m_buffer.data(), width, height, width}, theme);
    }

    // A terminal: each cell the colour at its middle, and a ring on the cell picked, black or white as it shows.
    void paint_cells(Painter& painter) const
    {
        const auto across_of{[this](int x) { return m_bounds.width > 1 ? x / (m_bounds.width - 1.0) : 0.5; }};
        const auto down_of{[this](int y) { return m_bounds.height > 1 ? y / (m_bounds.height - 1.0) : 0.5; }};
        for(int y{}; y < m_bounds.height; ++ y)
        {
            for(int x{}; x < m_bounds.width; ++ x)
            {
                painter.fill_rect({m_bounds.x + x, m_bounds.y + y, 1, 1}, to_color(m_paint(across_of(x), down_of(y))));
            }
        }
        const auto x{static_cast<int>(std::lround(m_across * std::max(0, m_bounds.width - 1)))};
        const auto y{static_cast<int>(std::lround(m_down * std::max(0, m_bounds.height - 1)))};
        const auto under{to_color(m_paint(across_of(x), down_of(y)))};
        const auto light{0.299 * under.red + 0.587 * under.green + 0.114 * under.blue > 128.0};
        painter.draw_text({m_bounds.x + x, m_bounds.y + y}, "○",
                          {light ? Color{0, 0, 0} : Color{255, 255, 255}, under, 0});
    }

    Size m_cells;
    std::pair<int, int> m_steps;
    Paint m_paint;
    Picked m_picked;
    double m_across{0.0};
    double m_down{0.0};
    mutable std::optional<Rect> m_pixels;
    mutable std::vector<std::uint32_t> m_buffer;
};

// Two squares side by side: the colour there was and the colour picked.
class Swatches final : public Widget
{
  public:
    Swatches(Color before, Color now) : m_before{before}, m_now{now} {}

    [[nodiscard]] Size measure(const Constraints& constraints) const override { return constraints.constrain({12, 2}); }

    void paint(Painter& painter, const Theme&) const override
    {
        painter.fill_rect({m_bounds.x, m_bounds.y, m_bounds.width / 2, m_bounds.height}, m_before);
        painter.fill_rect(
            {m_bounds.x + m_bounds.width / 2, m_bounds.y, m_bounds.width - m_bounds.width / 2, m_bounds.height}, m_now);
    }

    void set_now(Color now) { m_now = now; }

  private:
    Color m_before;
    Color m_now;
};

// The fields, in the order they show.
enum Field : std::size_t
{
    red,
    green,
    blue,
    hue,
    saturation,
    value,
    count
};

} // namespace

std::optional<Color> ask_color(Application& application, const std::string& title, Color start,
                               const std::function<void(Color)>& changed)
{
    // Hue and saturation kept apart from the colour, so they stay where they were when it turns grey or black.
    auto rgb{to_rgb(start)};
    auto hsv{to_hsv(rgb)};
    auto told{start};

    auto dialog{window(title)};
    auto& dialog_reference{*dialog};
    auto& content{column(*dialog)};
    content.set_gap(Spacing::tight);
    auto& top{row(content)};
    top.set_gap(Spacing::tight);

    std::array<NumberField*, Field::count> fields{};
    LineEdit* hex{};
    Swatches* swatches{};
    ColorArea* square{};
    ColorArea* hue_bar{};
    auto updating{false};
    // Shows the colour everywhere but in `typed_in`, the field it is being typed in, and tells it when it changed.
    std::function<void(const void* typed_in)> refresh;

    square = &top.add(std::make_unique<ColorArea>(
        Size{24, 10}, std::pair{100, 100},
        [&hsv](double across, double down) { return from_hsv({hsv.hue, across, 1.0 - down}); },
        [&](double across, double down)
        {
            hsv.saturation = across;
            hsv.value = 1.0 - down;
            rgb = from_hsv(hsv);
            refresh(nullptr);
        }));
    square->set_id("color.square");
    hue_bar = &top.add(std::make_unique<ColorArea>(
        Size{3, 10}, std::pair{0, 360}, [](double, double down) { return from_hsv({360.0 * (1.0 - down), 1.0, 1.0}); },
        [&](double, double down)
        {
            hsv.hue = 360.0 * (1.0 - down);
            rgb = from_hsv(hsv);
            refresh(nullptr);
        }));
    hue_bar->set_id("color.hue_bar");

    auto& side{column(top)};
    side.set_gap(Spacing::none);
    swatches = &side.add(std::make_unique<Swatches>(start, start));
    auto& form{grid(side)};
    form.set_columns({Length::content(), Length::content()});
    form.set_gap(Spacing::none);
    const std::array<std::pair<std::string, double>, Field::count> shown{{{translate("Dialogs", "&Red:"), 255},
                                                                          {translate("Dialogs", "&Green:"), 255},
                                                                          {translate("Dialogs", "&Blue:"), 255},
                                                                          {translate("Dialogs", "H&ue:"), 360},
                                                                          {translate("Dialogs", "&Saturation:"), 100},
                                                                          {translate("Dialogs", "&Value:"), 100}}};
    const std::array<const char*, Field::count> ids{"color.red", "color.green",      "color.blue",
                                                    "color.hue", "color.saturation", "color.value"};
    for(std::size_t index{}; index < Field::count; ++ index)
    {
        auto& name{label(shown[index].first, form)};
        auto& field{number_field(form)};
        field.set_range(0, shown[index].second);
        field.set_id(ids[index]);
        name.set_buddy(field.text_field());
        fields[index] = &field;
    }
    auto& hex_name{label(translate("Dialogs", "He&x:"), form)};
    hex = &line_edit(form);
    hex->set_id("color.hex");
    hex_name.set_buddy(*hex);

    refresh = [&](const void* typed_in)
    {
        updating = true;
        const auto color{to_color(rgb)};
        square->set_marker(hsv.saturation, 1.0 - hsv.value);
        hue_bar->set_marker(0.5, 1.0 - hsv.hue / 360.0);
        swatches->set_now(color);
        if(typed_in != hex)
        {
            hex->set_text(to_hex(color));
        }
        const std::array<double, Field::count> values{rgb.red * 255.0, rgb.green * 255.0,      rgb.blue * 255.0,
                                                      hsv.hue,         hsv.saturation * 100.0, hsv.value * 100.0};
        for(std::size_t index{}; index < Field::count; ++ index)
        {
            if(typed_in != fields[index])
            {
                fields[index]->set_value(values[index]);
            }
        }
        updating = false;
        if(color != told)
        {
            told = color;
            if(changed)
            {
                changed(color);
            }
        }
    };
    for(std::size_t index{}; index < Field::count; ++ index)
    {
        static_cast<void>(fields[index]->on_changed(
            [&, index](double typed)
            {
                if(updating)
                {
                    return;
                }
                if(index <= Field::blue)
                {
                    (index == Field::red ? rgb.red : index == Field::green ? rgb.green : rgb.blue) = typed / 255.0;
                    const auto kept{hsv};
                    hsv = to_hsv(rgb);
                    // A grey or black has no hue of its own, and black no saturation: they stay as they were.
                    if(hsv.saturation <= 0.0)
                    {
                        hsv.hue = kept.hue;
                    }
                    if(hsv.value <= 0.0)
                    {
                        hsv.saturation = kept.saturation;
                    }
                }
                else
                {
                    (index == Field::hue          ? hsv.hue
                     : index == Field::saturation ? hsv.saturation
                                                  : hsv.value) = index == Field::hue ? typed : typed / 100.0;
                    rgb = from_hsv(hsv);
                }
                refresh(fields[index]);
            }));
    }
    static_cast<void>(hex->on_changed(
        [&](std::string_view typed)
        {
            if(updating)
            {
                return;
            }
            // What is not a colour yet changes nothing, so typing it goes through halves.
            if(const auto parsed{from_hex(typed)})
            {
                rgb = to_rgb(*parsed);
                hsv = to_hsv(rgb);
                refresh(hex);
            }
        }));
    refresh(nullptr);

    auto& actions{row(content)};
    actions.set_gap(Spacing::tight);
    auto accepted{false};
    button(translate("Dialogs", "OK"), actions,
           [&]
           {
               accepted = true;
               dialog_reference.close(DialogResult::accepted);
           })
        .set_default(true);
    button(translate("Dialogs", "Cancel"), actions,
           [&dialog_reference] { dialog_reference.close(DialogResult::cancelled); });
    application.post([&application, square] { application.focus(*square); });
    static_cast<void>(application.show_modal(std::move(dialog)));
    if(!accepted)
    {
        return std::nullopt;
    }
    return to_color(rgb);
}

} // namespace olagarro::oui
