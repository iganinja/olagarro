#pragma once

#include <paint/tool.h>

#include <oui/container.h>

#include <functional>
#include <string_view>

namespace olagarro::oui
{
class CheckButton;
class ComboBox;
class NumberField;
class Row;
class Slider;
} // namespace olagarro::oui

namespace olagarro::paint
{

// The bar of the tool in use's options, as Paint.NET's tool bar: the brush's width, hardness and antialiasing, the
// fill's mode and tolerance, the gradient's kind and mode, a line's arrowheads, a shape's kind and style; each tool
// shows those it uses. The colours are the colour panel's: the bar keeps those it is given as they are.
class ToolOptionsBar final : public oui::Container
{
  public:
    explicit ToolOptionsBar(ToolOptions options = {});

    // Shows the options of the tool called `tool`, by its id: Paint's, or OUI Paint's clone stamp ("clone_stamp"),
    // recolor ("recolor") and text ("text"), which paint with some of them.
    void show_for(std::string_view tool);
    void show_for(Tool tool);

    // The options as the bar shows them.
    [[nodiscard]] const ToolOptions& options() const { return m_options; }
    // Shows `options`, without telling on_changed.
    void set_options(const ToolOptions& options);
    // Width by `step` pixels, kept from 1 to the widest, as [ and ] do.
    void change_width(double step);
    // Called after an option was changed in the bar.
    void on_changed(std::function<void(const ToolOptions&)> changed);
    // The one line the options are in: a program's own go at its end, shown and hidden as its tools need them.
    [[nodiscard]] oui::Row& bar() { return *m_bar; }

    [[nodiscard]] oui::Size measure(const oui::Constraints& constraints) const override;
    void arrange(const oui::Rect& bounds) override;
    void paint(oui::Painter& painter, const oui::Theme& theme) const override;

  private:
    void changed();
    // Called by each field as it changes: `update` puts its value in the options, then on_changed hears of it, unless
    // the bar is showing options it was given.
    void change(const std::function<void()>& update);

    ToolOptions m_options;
    oui::Row* m_bar{};
    oui::Row* m_width_group{};
    oui::Row* m_hardness_group{};
    oui::Row* m_antialias_group{};
    oui::Row* m_fill_group{};
    oui::Row* m_gradient_group{};
    oui::Row* m_line_group{};
    oui::Row* m_shape_group{};
    oui::NumberField* m_width{};
    oui::Slider* m_width_slider{};
    oui::NumberField* m_hardness{};
    oui::CheckButton* m_antialias{};
    oui::ComboBox* m_flood_mode{};
    oui::NumberField* m_tolerance{};
    oui::ComboBox* m_gradient_kind{};
    oui::ComboBox* m_gradient_mode{};
    oui::CheckButton* m_arrow_start{};
    oui::CheckButton* m_arrow_end{};
    oui::ComboBox* m_shape_kind{};
    oui::ComboBox* m_shape_style{};
    std::function<void(const ToolOptions&)> m_changed;
    bool m_updating{false};
};

} // namespace olagarro::paint
