#include <paint/panels/tool_options_bar.h>

#include <oui/check_button.h>
#include <oui/combo_box.h>
#include <oui/label.h>
#include <oui/layout.h>
#include <oui/line_edit.h>
#include <oui/number_field.h>
#include <oui/painter.h>
#include <oui/slider.h>
#include <oui/theme.h>
#include <oui/translation.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace olagarro::paint
{

namespace
{

constexpr double widest{2000.0};

oui::Row& group(oui::Container& parent)
{
    auto& row{oui::row(parent)};
    row.set_gap(oui::Spacing::tight);
    return row;
}

oui::ComboBox& choice(oui::Row& row, std::string_view label, std::vector<std::string> options)
{
    auto& name{oui::label(label, row)};
    auto& combo{row.add(std::make_unique<oui::ComboBox>(std::move(options), 0))};
    name.set_buddy(combo);
    return combo;
}

std::string shown_name(GradientKind kind)
{
    switch(kind)
    {
    case GradientKind::linear:
        return oui::translate("Tools", "Linear");
    case GradientKind::linear_reflected:
        return oui::translate("Tools", "Linear Reflected");
    case GradientKind::diamond:
        return oui::translate("Tools", "Linear Diamond");
    case GradientKind::radial:
        return oui::translate("Tools", "Radial");
    case GradientKind::conical:
        break;
    }
    return oui::translate("Tools", "Conical");
}

std::size_t gradient_index(GradientKind kind)
{
    const auto& kinds{all_gradient_kinds()};
    for(std::size_t index{}; index < kinds.size(); ++ index)
    {
        if(kinds[index] == kind)
        {
            return index;
        }
    }
    return 0;
}

} // namespace

ToolOptionsBar::ToolOptionsBar(ToolOptions options) : m_options{options}
{
    m_bar = &oui::row(*this);
    m_bar->set_gap(oui::Spacing::normal);

    m_width_group = &group(*m_bar);
    auto& width_label{oui::label(oui::translate("Tools", "Brush &width:"), *m_width_group)};
    m_width = &oui::number_field(*m_width_group);
    m_width->set_range(1, widest);
    width_label.set_buddy(m_width->text_field());
    m_width_slider = &oui::slider(*m_width_group);
    m_width_slider->set_range(1, 200);

    m_hardness_group = &group(*m_bar);
    auto& hardness_label{oui::label(oui::translate("Tools", "&Hardness:"), *m_hardness_group)};
    m_hardness = &oui::number_field(*m_hardness_group);
    m_hardness->set_range(0, 100);
    hardness_label.set_buddy(m_hardness->text_field());
    oui::label("%", *m_hardness_group);

    m_antialias_group = &group(*m_bar);
    m_antialias = &oui::check_button(oui::translate("Tools", "&Antialiasing"), *m_antialias_group);

    m_fill_group = &group(*m_bar);
    m_flood_mode = &choice(*m_fill_group, oui::translate("Tools", "&Flood mode:"),
                           {oui::translate("Tools", "Contiguous"), oui::translate("Tools", "Global")});
    auto& tolerance_label{oui::label(oui::translate("Tools", "&Tolerance:"), *m_fill_group)};
    m_tolerance = &oui::number_field(*m_fill_group);
    m_tolerance->set_range(0, 100);
    tolerance_label.set_buddy(m_tolerance->text_field());
    oui::label("%", *m_fill_group);

    m_gradient_group = &group(*m_bar);
    std::vector<std::string> kinds;
    for(const auto kind : all_gradient_kinds())
    {
        kinds.push_back(shown_name(kind));
    }
    m_gradient_kind = &choice(*m_gradient_group, oui::translate("Tools", "&Gradient:"), kinds);
    m_gradient_mode = &choice(*m_gradient_group, oui::translate("Tools", "&Mode:"),
                              {oui::translate("Tools", "Color"), oui::translate("Tools", "Transparency")});

    m_line_group = &group(*m_bar);
    m_arrow_start = &oui::check_button(oui::translate("Tools", "Arrow at &start"), *m_line_group);
    m_arrow_end = &oui::check_button(oui::translate("Tools", "Arrow at &end"), *m_line_group);

    m_shape_group = &group(*m_bar);
    m_shape_kind = &choice(*m_shape_group, oui::translate("Tools", "&Shape:"),
                           {oui::translate("Tools", "Rectangle"), oui::translate("Tools", "Rounded Rectangle"),
                            oui::translate("Tools", "Ellipse")});
    m_shape_style = &choice(*m_shape_group, oui::translate("Tools", "St&yle:"),
                            {oui::translate("Tools", "Outline"), oui::translate("Tools", "Filled"),
                             oui::translate("Tools", "Filled with Outline")});

    set_options(m_options);

    static_cast<void>(m_width->on_changed(
        [this](double value)
        {
            change([this, value] { m_options.brush.width = value; });
            if(!m_updating)
            {
                m_updating = true;
                m_width_slider->set_value(value);
                m_updating = false;
            }
        }));
    static_cast<void>(m_width_slider->on_changed(
        [this](double value)
        {
            if(!m_updating)
            {
                m_updating = true;
                m_width->set_value(value);
                m_updating = false;
                change([this, value] { m_options.brush.width = value; });
            }
        }));
    static_cast<void>(m_hardness->on_changed([this](double value)
                                             { change([this, value] { m_options.brush.hardness = value / 100.0; }); }));
    static_cast<void>(m_antialias->on_toggled([this](bool checked)
                                              { change([this, checked] { m_options.brush.antialias = checked; }); }));
    static_cast<void>(m_flood_mode->on_selected([this](std::size_t index)
                                                { change([this, index] { m_options.fill.global = index == 1; }); }));
    static_cast<void>(m_tolerance->on_changed(
        [this](double value) { change([this, value] { m_options.fill.tolerance = value / 100.0; }); }));
    static_cast<void>(m_gradient_kind->on_selected(
        [this](std::size_t index) { change([this, index] { m_options.gradient = all_gradient_kinds()[index]; }); }));
    static_cast<void>(m_gradient_mode->on_selected(
        [this](std::size_t index) { change([this, index] { m_options.gradient_transparency = index == 1; }); }));
    static_cast<void>(m_arrow_start->on_toggled(
        [this](bool checked) { change([this, checked] { m_options.line.arrow_at_start = checked; }); }));
    static_cast<void>(m_arrow_end->on_toggled([this](bool checked)
                                              { change([this, checked] { m_options.line.arrow_at_end = checked; }); }));
    static_cast<void>(m_shape_kind->on_selected(
        [this](std::size_t index)
        { change([this, index] { m_options.shape.kind = static_cast<ShapeKind>(index); }); }));
    static_cast<void>(m_shape_style->on_selected(
        [this](std::size_t index)
        { change([this, index] { m_options.shape.style = static_cast<ShapeStyle>(index); }); }));
    show_for(Tool::brush);
}

void ToolOptionsBar::set_options(const ToolOptions& options)
{
    m_options = options;
    const auto was_updating{m_updating};
    m_updating = true;
    m_width->set_value(options.brush.width);
    m_width_slider->set_value(options.brush.width);
    m_hardness->set_value(options.brush.hardness * 100.0);
    m_antialias->set_checked(options.brush.antialias);
    m_flood_mode->set_selected(options.fill.global ? 1 : 0);
    m_tolerance->set_value(options.fill.tolerance * 100.0);
    m_gradient_kind->set_selected(gradient_index(options.gradient));
    m_gradient_mode->set_selected(options.gradient_transparency ? 1 : 0);
    m_arrow_start->set_checked(options.line.arrow_at_start);
    m_arrow_end->set_checked(options.line.arrow_at_end);
    m_shape_kind->set_selected(static_cast<std::size_t>(options.shape.kind));
    m_shape_style->set_selected(static_cast<std::size_t>(options.shape.style));
    m_updating = was_updating;
    // What the fields kept of it, as a width past the widest.
    m_options.brush.width = m_width->value();
}

void ToolOptionsBar::show_for(std::string_view tool)
{
    const auto brushing{tool == "brush" || tool == "eraser"};
    const auto drawing{tool == "line" || tool == "shapes" || tool == "arc"};
    m_width_group->set_visible(brushing || drawing || tool == "clone_stamp" || tool == "recolor");
    m_hardness_group->set_visible(brushing || tool == "clone_stamp" || tool == "recolor");
    m_antialias_group->set_visible(brushing || drawing || tool == "ellipse_select" || tool == "text");
    m_fill_group->set_visible(tool == "bucket" || tool == "magic_wand" || tool == "recolor");
    m_gradient_group->set_visible(tool == "gradient");
    m_line_group->set_visible(tool == "line");
    m_shape_group->set_visible(tool == "shapes");
}

void ToolOptionsBar::show_for(Tool tool)
{
    show_for(tool_id(tool));
}

void ToolOptionsBar::change_width(double step)
{
    m_width->set_value(m_options.brush.width + step);
}

void ToolOptionsBar::on_changed(std::function<void(const ToolOptions&)> changed)
{
    m_changed = std::move(changed);
}

void ToolOptionsBar::change(const std::function<void()>& update)
{
    if(m_updating)
    {
        return;
    }
    update();
    changed();
}

void ToolOptionsBar::changed()
{
    if(m_changed)
    {
        m_changed(m_options);
    }
}

oui::Size ToolOptionsBar::measure(const oui::Constraints& constraints) const
{
    return {constraints.max_width, 1.0};
}

void ToolOptionsBar::arrange(const oui::Rect& bounds)
{
    Widget::arrange(bounds);
    m_bar->arrange(bounds);
}

void ToolOptionsBar::paint(oui::Painter& painter, const oui::Theme& theme) const
{
    painter.fill_rect(m_bounds, theme.status_background);
    Container::paint(painter, theme);
}

} // namespace olagarro::paint
