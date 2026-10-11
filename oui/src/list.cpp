#include <oui/list.h>

#include <oui/label.h>

namespace olagarro::oui
{

List::List()
{
    set_stretch_column(0);
    set_colors(&Theme::list_background, &Theme::list_foreground);
}

Label& List::add_item(std::string_view text)
{
    return add(label(text));
}

std::unique_ptr<List> list()
{
    return std::make_unique<List>();
}

List& list(Container& parent)
{
    return parent.add(list());
}

} // namespace olagarro::oui
