#pragma once

#include <oui/table.h>

#include <memory>
#include <string_view>

namespace olagarro::oui
{

class Label;

// A Table set up as a plain list: one column that takes the whole width, no headers, in the theme's list colours.
// Each child is a row, any widget; add_item adds a row of text.
class List : public Table
{
  public:
    List();

    Label& add_item(std::string_view text);
};

[[nodiscard]] std::unique_ptr<List> list();
List& list(Container& parent);

} // namespace olagarro::oui
