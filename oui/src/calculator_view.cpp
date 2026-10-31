#include <oui/calculator_view.h>

#include <oui/application.h>
#include <oui/grid.h>
#include <oui/markdown_view.h>
#include <oui/button.h>
#include <oui/check_button.h>
#include <oui/layout.h>
#include <oui/syntax_highlighter.h>
#include <oui/text_layout.h>
#include <oui/text_edit.h>
#include <oui/translation.h>
#include <oui/window.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>

namespace olagarro::oui
{

// Defined in the source CMake generates from oui/help: the help in English, then in Spanish.
std::vector<std::string_view> embedded_oui_calculator_help();
std::vector<std::string_view> embedded_oui_calculator_help_names();

namespace
{

// The line without spaces at its ends.
[[nodiscard]] std::string_view trimmed(std::string_view text)
{
    const auto first{text.find_first_not_of(" \t")};
    if(first == std::string_view::npos)
    {
        return {};
    }
    return text.substr(first, text.find_last_not_of(" \t") - first + 1);
}

// What help lists: how to write, the commands, the functions and the constants.
[[nodiscard]] std::string help_text()
{
    std::string text{translate(
        "CalculatorView", "Write as on paper: + - * / ^ ( ), 16% for a percentage, 5! for a factorial, x = 2 to keep a "
                          "value; F1 explains it all.\nCommands: deg, rad, clear, help, exit.\nFunctions:")};
    for(const auto& usage : calculator_functions())
    {
        text += " " + usage;
    }
    text += "\n" + translate("CalculatorView", "Constants:");
    for(const auto& name : calculator_constants())
    {
        text += " " + name;
    }
    return text;
}

// The log's colours: what was written in the quiet colour of a comment, what it came to in the bright one of a keyword,
// so the results stand out.
class LogColours final : public SyntaxHighlighter
{
  public:
    State highlight_line(std::string_view line, State state, std::vector<SyntaxSpan>& spans) const override
    {
        const auto written{line.substr(0, 2) == "> "};
        spans.push_back({0, line.size(), written ? SyntaxKind::comment : SyntaxKind::keyword});
        return state;
    }
};

} // namespace

// The unit angles are measured in, RAD or DEG, as a small button: a click switches it. It leaves the keyboard to the
// line being written.
class AngleSwitch final : public Widget
{
  public:
    explicit AngleSwitch(std::function<void()> clicked) : m_clicked{std::move(clicked)} {}

    void set_degrees(bool degrees) { m_degrees = degrees; }

    [[nodiscard]] Size measure(const Constraints& constraints) const override
    {
        return constraints.constrain({static_cast<double>(shown_width(text())), 1.0});
    }

    void paint(Painter& painter, const Theme& theme) const override
    {
        painter.fill_rect(bounds(), theme.button_background);
        painter.draw_label({bounds().x, bounds().y}, text(), {theme.button_foreground, theme.button_background});
    }

    bool handle_event(const Event& event) override
    {
        const auto* const mouse{std::get_if<MouseEvent>(&event)};
        if(mouse == nullptr || mouse->hover || mouse->wheel_delta != 0)
        {
            return false;
        }
        if(mouse->pressed && !mouse->dragging)
        {
            m_clicked();
        }
        return true;
    }

    [[nodiscard]] std::optional<std::string> tooltip_at(Point) const override
    {
        return m_degrees ? translate("CalculatorView", "Angles in degrees: click, or F2, for radians")
                         : translate("CalculatorView", "Angles in radians: click, or F2, for degrees");
    }

  private:
    [[nodiscard]] std::string text() const { return m_degrees ? " DEG " : " RAD "; }

    std::function<void()> m_clicked;
    bool m_degrees{false};
};

CalculatorView::CalculatorView(Application& application) : m_application{application}
{
    // Its commands are not names, so a line with one alone is always the command.
    for(const auto* const command : {"deg", "rad", "clear", "help", "exit"})
    {
        m_calculator.reserve(command);
    }
    auto log{text_edit()};
    m_log = log.get();
    m_log->set_read_only(true);
    m_log->set_word_wrap(true);
    m_log->set_syntax_highlighter(std::make_shared<LogColours>());

    // What the line comes to, across, and how angles are measured, at the end.
    auto status{grid()};
    status->set_columns({Length::fill(), Length::content()});
    // Apart, so a long line does not run into the angles.
    status->set_gap(Spacing::tight);
    m_preview = &label("", *status);
    m_angles = &status->add(
        std::make_unique<AngleSwitch>([this] { use_angles(m_calculator.angle_unit() != AngleUnit::degrees); }));

    auto line{grid()};
    line->set_columns({Length::content(), Length::fill()});
    line->set_gap(Spacing::none);
    label("> ", *line);
    auto prompt{line_edit()};
    m_prompt = prompt.get();
    line->add(std::move(prompt));
    m_prompt->on_changed([this](std::string_view) { show_preview(); });
    m_prompt->on_submitted([this](std::string_view) { submit(); });

    auto layout{grid()};
    layout->set_columns({Length::fill()});
    layout->set_rows({Length::flex(), Length::content(), Length::content()});
    layout->set_gap(Spacing::none);
    layout->add(std::move(log));
    layout->add(std::move(status));
    layout->add(std::move(line));
    add(std::move(layout));
    show_preview();
}

LineEdit& CalculatorView::prompt()
{
    return *m_prompt;
}

std::string_view CalculatorView::log() const
{
    return m_log->text();
}

const std::vector<std::string>& CalculatorView::entries() const
{
    return m_entries;
}

void CalculatorView::replay(const std::vector<std::string>& entries)
{
    for(const auto& entry : entries)
    {
        m_prompt->set_text(entry);
        submit();
    }
    m_prompt->set_text("");
    show_preview();
}

void CalculatorView::add_to_log(std::string_view text)
{
    const auto end{m_log->text().size()};
    m_log->set_read_only(false);
    m_log->select_range(end, end);
    m_log->paste(std::string{end == 0 ? "" : "\n"} + std::string{text});
    m_log->set_read_only(true);
}

void CalculatorView::show_preview()
{
    const auto degrees{m_calculator.angle_unit() == AngleUnit::degrees};
    m_angles->set_degrees(degrees);
    const auto written{trimmed(m_prompt->text())};
    m_preview->set_color(&Theme::muted_text);
    if(written.empty())
    {
        m_preview->set_text(
            translate("CalculatorView",
                      "Write an expression, as (800*0.16) - (0.32*500), and press Enter; help lists the rest."));
        return;
    }
    if(written == "deg" || written == "rad" || written == "clear" || written == "help" || written == "?" ||
       written == "exit")
    {
        m_preview->set_text("");
        return;
    }
    const auto result{m_calculator.preview(m_prompt->text())};
    if(result.value)
    {
        m_preview->set_text("= " + format_number(*result.value));
    }
    else if(result.error)
    {
        // While it is being written an expression is often unfinished: said quietly, in the colour of an error only
        // once Enter is pressed.
        m_preview->set_text(result.error->message);
    }
}

void CalculatorView::submit()
{
    const std::string written{trimmed(m_prompt->text())};
    if(written.empty())
    {
        return;
    }
    if(m_history.empty() || m_history.back() != written)
    {
        m_history.push_back(written);
    }
    m_recalled = m_history.size();
    m_draft.clear();
    if(written == "exit")
    {
        m_prompt->set_text("");
        // Last: whoever hears it may close the calculator.
        m_exit.fire();
        return;
    }
    if(written == "clear")
    {
        clear_log();
    }
    else if(written == "deg" || written == "rad")
    {
        use_angles(written == "deg");
    }
    else if(written == "help" || written == "?")
    {
        m_entries.push_back(written);
        add_to_log("> " + written + "\n" + help_text());
    }
    else
    {
        const auto result{m_calculator.evaluate(m_prompt->text())};
        if(result.error)
        {
            // The line stays as it is, the cursor where it went wrong, and the error says why.
            m_preview->set_text(result.error->message);
            m_preview->set_color(&Theme::error_foreground);
            m_prompt->select_range(result.error->position, result.error->position);
            return;
        }
        if(!result.value)
        {
            return;
        }
        m_entries.push_back(written);
        const auto value{format_number(*result.value)};
        add_to_log("> " + written + "\n" + (result.assigned ? *result.assigned + " = " + value : "= " + value));
    }
    m_prompt->set_text("");
    show_preview();
}

void CalculatorView::clear_log()
{
    m_entries.clear();
    m_log->set_read_only(false);
    m_log->set_text("");
    m_log->set_read_only(true);
}

void CalculatorView::show_options()
{
    const auto degrees{m_calculator.angle_unit() == AngleUnit::degrees};
    auto dialog{window(translate("CalculatorView", "Calculator Options"))};
    auto& dialog_reference{*dialog};
    auto content{column()};
    auto& in_degrees{check_button(translate("CalculatorView", "Measure angles in &degrees"), *content)};
    in_degrees.set_checked(degrees);
    auto& actions{row(*content)};
    actions.set_gap(Spacing::tight);
    actions.set_centered(true);
    // What OK keeps, read before the dialog goes.
    std::optional<bool> chosen;
    button(translate("Dialogs", "OK"), actions,
           [&dialog_reference, &in_degrees, &chosen]
           {
               chosen = in_degrees.is_checked();
               dialog_reference.close(DialogResult::accepted);
           })
        .set_default(true);
    button(translate("Dialogs", "Cancel"), actions,
           [&dialog_reference] { dialog_reference.close(DialogResult::cancelled); });
    dialog->add(std::move(content));
    static_cast<void>(m_application.show_modal(std::move(dialog)));
    if(chosen && *chosen != degrees)
    {
        use_angles(*chosen);
    }
    m_application.focus(*m_prompt);
}

void CalculatorView::use_angles(bool degrees)
{
    const std::string written{degrees ? "deg" : "rad"};
    m_calculator.set_angle_unit(degrees ? AngleUnit::degrees : AngleUnit::radians);
    m_entries.push_back(written);
    add_to_log("> " + written + "\n" +
               (degrees ? translate("CalculatorView", "Angles in degrees.")
                        : translate("CalculatorView", "Angles in radians.")));
    show_preview();
}

void CalculatorView::recall(int step)
{
    if(m_history.empty())
    {
        return;
    }
    if(m_recalled == m_history.size())
    {
        m_draft = std::string{m_prompt->text()};
    }
    if(step < 0 && m_recalled > 0)
    {
        -- m_recalled;
    }
    else if(step > 0 && m_recalled < m_history.size())
    {
        ++ m_recalled;
    }
    m_prompt->set_text(m_recalled == m_history.size() ? m_draft : m_history[m_recalled]);
    const auto end{m_prompt->text().size()};
    m_prompt->select_range(end, end);
}

bool CalculatorView::handle_event(const Event& event)
{
    if(const auto* const key{std::get_if<KeyEvent>(&event)};
       key != nullptr && key->key == Key::f2 && !key->control && !key->alt && !key->shift)
    {
        show_options();
        return true;
    }
    // Up and Down that the line leaves to its parents go through what was written.
    if(const auto* const key{std::get_if<KeyEvent>(&event)};
       key != nullptr && !key->control && !key->alt && !key->shift && (key->key == Key::up || key->key == Key::down) &&
       m_prompt->is_focused())
    {
        recall(key->key == Key::up ? -1 : 1);
        return true;
    }
    return Container::handle_event(event);
}

Size CalculatorView::measure(const Constraints& constraints) const
{
    return constraints.constrain({std::max(1.0, constraints.max_width), std::max(1.0, constraints.max_height)});
}

void CalculatorView::arrange(const Rect& bounds)
{
    Widget::arrange(bounds);
    if(!children().empty())
    {
        children().front()->arrange(bounds);
    }
}

std::string calculator_help(const std::vector<CalculatorKey>& program_keys)
{
    std::string markdown{
        text_for_language(embedded_oui_calculator_help(), embedded_oui_calculator_help_names(), installed_language())};
    // The program's keys go in the table of keys, after the calculator's last, F1.
    const auto f1{markdown.find("| F1 |")};
    auto at{f1 == std::string::npos ? markdown.size() : markdown.find('\n', f1) + 1};
    for(const auto& [keys, action] : program_keys)
    {
        const auto row{"| " + keys + " | " + action + " |\n"};
        markdown.insert(at, row);
        at += row.size();
    }
    return markdown;
}

void show_calculator_help(Application& application, const std::vector<CalculatorKey>& program_keys)
{
    auto dialog{window(translate("CalculatorView", "Calculator Help"))};
    auto& view{dialog->add(markdown_view(calculator_help(program_keys)))};
    // Apart from the calculator, as a manual page.
    view.set_background(&Theme::reading_background);
    view.set_foreground(&Theme::reading_foreground);
    dialog->set_preferred_size({100, 0});
    dialog->set_maximum_size({110, 1 << 15});
    dialog->set_height_share(0.9);
    dialog->set_minimum_size({40, 10});
    dialog->set_resizable(true);
    application.post([&application, &view] { application.focus(view); });
    static_cast<void>(application.show_modal(std::move(dialog)));
}

} // namespace olagarro::oui
