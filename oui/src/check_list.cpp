#include <oui/check_list.h>

#include <oui/label.h>

namespace olagarro::oui
{

namespace
{

void mark(Label& label, bool checked)
{
    label.set_marker(checked ? "[x] " : "[ ] ", CheckList::box_columns, checked ? Icon::task_done : Icon::task_open);
}

} // namespace

void CheckList::add_item(std::string_view text, bool checked)
{
    mark(List::add_item(text), checked);
}

bool CheckList::is_checked(std::size_t row) const
{
    const auto* const label{row < child_count() ? dynamic_cast<const Label*>(&child_at(row)) : nullptr};
    const auto box{label != nullptr ? label->marker() : std::nullopt};
    return box && box->icon == Icon::task_done;
}

void CheckList::set_checked(std::size_t row, bool checked)
{
    if(auto* const label{row < child_count() ? dynamic_cast<Label*>(&child_at(row)) : nullptr})
    {
        mark(*label, checked);
    }
}

void CheckList::toggle(std::size_t row)
{
    const auto checked{!is_checked(row)};
    set_checked(row, checked);
    m_toggled.fire(row, checked);
}

bool CheckList::handle_event(const Event& event)
{
    // A click on a row's box ticks it, once the click has chosen the row.
    if(const auto* const mouse{std::get_if<MouseEvent>(&event)};
       mouse != nullptr && mouse->pressed && !mouse->right && !mouse->dragging && mouse->clicks == 1 &&
       mouse->x >= bounds().x && mouse->x < bounds().x + box_columns)
    {
        if(const auto row{row_at(mouse->y)})
        {
            const auto handled{List::handle_event(event)};
            toggle(*row);
            return handled;
        }
    }
    // Space, as a key or as typed, ticks the row chosen.
    const auto* const key{std::get_if<KeyEvent>(&event)};
    const auto* const text{std::get_if<TextEvent>(&event)};
    if(((key != nullptr && key->key == Key::space && !key->control && !key->alt) ||
        (text != nullptr && text->codepoint == U' ' && !text->control && !text->alt)) &&
       selected_row())
    {
        toggle(*selected_row());
        return true;
    }
    return List::handle_event(event);
}

std::unique_ptr<CheckList> check_list()
{
    return std::make_unique<CheckList>();
}

} // namespace olagarro::oui
