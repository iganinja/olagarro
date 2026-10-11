#include "interactive_view.h"

#include "ui_descriptions.h"

#include <oui/grid.h>
#include <oui/translation.h>

#include <algorithm>
#include <memory>
#include <utility>

using namespace olagarro;

namespace
{

[[nodiscard]] std::string first_line(std::string_view text)
{
    return std::string{text.substr(0, text.find('\n'))};
}

} // namespace

ErrorLine::ErrorLine(std::function<void()> clicked) : m_clicked{std::move(clicked)}
{
}

void ErrorLine::set_text(std::string text)
{
    m_text = std::move(text);
}

oui::Size ErrorLine::measure(const oui::Constraints& constraints) const
{
    return constraints.constrain({std::max(1, constraints.max_width), 1});
}

void ErrorLine::paint(oui::Painter& painter, const oui::Theme& theme) const
{
    const oui::TextStyle style{theme.status_foreground, theme.status_background, oui::text_bold};
    painter.fill_rect(bounds(), style.background);
    painter.push_clip(bounds());
    painter.draw_label({bounds().x + 1, bounds().y}, m_text, style);
    painter.pop_clip();
}

bool ErrorLine::handle_event(const oui::Event& event)
{
    if(const auto* const mouse{std::get_if<oui::MouseEvent>(&event)}; mouse != nullptr && mouse->pressed)
    {
        m_clicked();
        return true;
    }
    return false;
}

InteractiveView::InteractiveView(std::unique_ptr<oui::TextEdit> code)
{
    auto ui{build_oui_code_ui(OuiCodeUi::interactive)};
    m_code = &ui.put("code", std::move(code));
    m_result_title = &ui.get<oui::Label>("result_title");
    m_apply_button = &ui.get<oui::Button>("apply");
    m_result_text = &ui.get<oui::TextEdit>("result");
    m_log_pane = &ui.get<oui::Grid>("log");
    m_log_text = &ui.get<oui::TextEdit>("log_text");
    m_input_pane = &ui.get<oui::Grid>("input_pane");
    m_input = &ui.get<oui::TextEdit>("input");
    static_cast<void>(m_apply_button->on_clicked([this] { m_apply.fire(); }));
    static_cast<void>(ui.get<oui::Button>("show_log").on_clicked([this] { set_log_visible(!log_visible()); }));
    m_error_line_view = &ui.put("error_line", std::make_unique<ErrorLine>(
                                                  [this]
                                                  {
                                                      if(m_error_line)
                                                      {
                                                          m_error_chosen.fire(*m_error_line);
                                                      }
                                                  }));
    m_error_line_view->set_visible(false);
    add(ui.take());
}

oui::TextEdit& InteractiveView::code()
{
    return *m_code;
}

void InteractiveView::show(const LiveResult& result, std::string_view input_name)
{
    const auto title{input_name.empty()
                         ? oui::translate("Interactive", "Result")
                         : oui::format_text(oui::translate("Interactive", "Result on {0}"), {input_name})};
    m_log_text->set_text(result.log);
    if(result.error)
    {
        // What it made last stays, said to be old, until the script works again.
        m_result_title->set_text(title + "  " + oui::translate("Interactive", "(from before the error)"));
        m_error_line = result.error_line;
        m_error_line_view->set_text(
            result.error_line ? oui::format_text(oui::translate("Interactive", "Line {0}: {1}"),
                                                 {std::to_string(*result.error_line), first_line(*result.error)})
                              : first_line(*result.error));
        m_error_line_view->set_visible(true);
        return;
    }
    m_error_line.reset();
    m_error_line_view->set_visible(false);
    m_result_title->set_text(title);
    m_result = result.text;
    m_result_text->set_text(result.text.value_or(""));
    m_apply_button->set_visible(m_applies && result.text.has_value() && !input_name.empty());
}

oui::TextEdit& InteractiveView::input()
{
    return *m_input;
}

void InteractiveView::show_input(std::string_view text)
{
    m_input->set_text(text);
    m_input_pane->set_visible(true);
}

void InteractiveView::set_applies(bool applies)
{
    m_applies = applies;
}

const std::optional<std::string>& InteractiveView::result() const
{
    return m_result;
}

void InteractiveView::set_log_visible(bool visible)
{
    m_log_pane->set_visible(visible);
}

bool InteractiveView::log_visible() const
{
    return m_log_pane->is_visible();
}

oui::Size InteractiveView::measure(const oui::Constraints& constraints) const
{
    return constraints.constrain({1, 1});
}

void InteractiveView::arrange(const oui::Rect& bounds)
{
    Widget::arrange(bounds);
    if(!children().empty())
    {
        children().front()->arrange(bounds);
    }
}
