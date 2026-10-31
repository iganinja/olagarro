#include <oui/button.h>
#include <oui/dialogs.h>
#include <oui/label.h>
#include <oui/layout.h>
#include <oui/line_edit.h>
#include <oui/spacer.h>
#include <oui/window.h>

#include <algorithm>

namespace olagarro::oui
{

namespace
{

// How wide a dialog's text runs before it wraps, rather than stretch the dialog across the screen.
constexpr int widest_by_default{60};

// One label per line; a line wider than the screen wraps rather than losing its end, which for a path is
// the part that names the file.
void add_lines(Column& column, std::string_view text)
{
    for(std::size_t start{}; start <= text.size();)
    {
        const auto end{std::min(text.find('\n', start), text.size())};
        column.add(label(text.substr(start, end - start))).set_wrap(true);
        start = end + 1;
    }
}

} // namespace

std::optional<std::size_t> message_box(Application& application, std::string_view title, std::string_view message,
                                       const std::vector<std::string>& buttons)
{
    std::optional<std::size_t> answer;
    auto dialog{window(title)};
    auto& dialog_reference{*dialog};
    dialog->set_maximum_size({widest_by_default, 1 << 15});
    auto content{column()};
    content->set_gap(Spacing::none);
    add_lines(*content, message);
    content->add(vspacer(1));
    auto& actions{row(*content)};
    actions.set_gap(Spacing::tight);
    // Under the message, in the middle, as a message box's answers are.
    actions.set_centered(true);
    Widget* first_button{};
    for(std::size_t index{}; index < buttons.size(); ++ index)
    {
        auto& added{button(buttons.at(index), actions,
                           [&answer, &dialog_reference, index]
                           {
                               answer = index;
                               dialog_reference.close(DialogResult::accepted);
                           })};
        if(first_button == nullptr)
        {
            // The first answer is the default, as OK is.
            added.set_default(true);
            first_button = &added;
        }
    }
    dialog->add(std::move(content));
    if(first_button != nullptr)
    {
        application.post([&application, first_button] { application.focus(*first_button); });
    }
    static_cast<void>(application.show_modal(std::move(dialog)));
    return answer;
}

std::optional<std::string> prompt(Application& application, const PromptOptions& options)
{
    std::optional<std::string> result;
    auto dialog{window(options.title)};
    auto& dialog_reference{*dialog};
    if(options.width)
    {
        dialog->set_preferred_size({static_cast<double>(*options.width), 0.0});
    }
    // Typing a long text scrolls it in the field rather than widening the dialog past this.
    dialog->set_maximum_size({static_cast<double>(std::max(options.width.value_or(0), widest_by_default)), 1 << 15});
    auto content{column()};
    content->set_gap(Spacing::none);
    // A question of one line names the field, so a letter marked in it with & goes there.
    Label* question{};
    if(options.label.find('\n') == std::string::npos && !options.label.empty())
    {
        question = &content->add(label(options.label));
    }
    else if(!options.label.empty())
    {
        add_lines(*content, options.label);
    }
    auto& field{content->add(line_edit())};
    if(question != nullptr)
    {
        question->set_buddy(field);
    }
    field.set_text(options.text);
    if(options.selection)
    {
        field.select_range(options.selection->first, options.selection->second);
    }
    else
    {
        field.select_all();
    }
    field.set_placeholder(options.placeholder);
    field.set_password(options.password);
    if(options.extra)
    {
        options.extra(*content);
    }
    auto& hint{content->add(label(""))};
    hint.set_color(&Theme::error_foreground);
    auto& actions{row(*content)};
    actions.set_gap(Spacing::tight);
    const auto accept{[&result, &dialog_reference, &field, &hint, &options]
                      {
                          const std::string text{field.text()};
                          if(options.validate)
                          {
                              if(const auto problem{options.validate(text)})
                              {
                                  hint.set_text(*problem);
                                  return;
                              }
                          }
                          result = text;
                          dialog_reference.close(DialogResult::accepted);
                      }};
    button(options.accept, actions, [&accept] { accept(); }).set_default(true);
    button(options.cancel, actions, [&dialog_reference] { dialog_reference.close(DialogResult::cancelled); });
    static_cast<void>(field.on_submitted([&accept](std::string_view) { accept(); }));
    dialog->add(std::move(content));
    application.post([&application, &field] { application.focus(field); });
    static_cast<void>(application.show_modal(std::move(dialog)));
    return result;
}

} // namespace olagarro::oui
