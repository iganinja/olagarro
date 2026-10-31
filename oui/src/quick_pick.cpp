#include <oui/quick_pick.h>

#include <oui/fuzzy.h>
#include <oui/layout.h>
#include <oui/line_edit.h>
#include <oui/list.h>
#include <oui/text_layout.h>
#include <oui/window.h>

#include <algorithm>
#include <numeric>

namespace olagarro::oui
{

namespace
{

// The dialog opens wide enough to read long labels with their details.
constexpr int minimum_width{56};

[[nodiscard]] std::string row_text(const QuickPickItem& item)
{
    return item.detail.empty() ? item.label : item.label + "  " + item.detail;
}

// The last part of a label that is a path; the whole label otherwise.
[[nodiscard]] std::string_view last_part(std::string_view label)
{
    const auto slash{label.find_last_of("/\\")};
    return slash == std::string_view::npos ? label : label.substr(slash + 1);
}

// The items matching `filter`, best first: all of them in their order when it is empty. A filter with '*' or
// '?' is a wildcard pattern, as in a shell, and keeps the items' order: without a slash it matches the last
// part of a path, as find -name does, and with one the whole path.
[[nodiscard]] std::vector<std::size_t> matches(const std::vector<QuickPickItem>& items, std::string_view filter,
                                               std::size_t limit)
{
    const auto wildcard{filter.find_first_of("*?") != std::string_view::npos};
    std::vector<std::pair<int, std::size_t>> scored;
    for(std::size_t index{}; index < items.size(); ++ index)
    {
        if(wildcard)
        {
            const auto& label{items[index].label};
            const auto whole_path{filter.find_first_of("/\\") != std::string_view::npos};
            if(wildcard_match(filter, whole_path ? std::string_view{label} : last_part(label)))
            {
                scored.emplace_back(0, index);
            }
        }
        else if(const auto score{fuzzy_score(filter, items[index].label)})
        {
            scored.emplace_back(*score, index);
        }
    }
    if(!filter.empty() && !wildcard)
    {
        // Equal scores keep the shorter label first, then the original order.
        std::stable_sort(scored.begin(), scored.end(),
                         [&items](const auto& left, const auto& right)
                         {
                             if(left.first != right.first)
                             {
                                 return left.first > right.first;
                             }
                             return items[left.second].label.size() < items[right.second].label.size();
                         });
    }
    std::vector<std::size_t> result;
    for(std::size_t index{}; index < scored.size() && index < limit; ++ index)
    {
        result.push_back(scored[index].second);
    }
    return result;
}

} // namespace

std::optional<std::size_t> quick_pick(Application& application, const std::vector<QuickPickItem>& items,
                                      const QuickPickOptions& options)
{
    std::optional<std::size_t> picked;
    auto shown{matches(items, {}, options.row_limit)};
    auto dialog{window(options.title)};
    auto& dialog_reference{*dialog};
    auto content{column()};
    auto& filter{line_edit(*content)};
    filter.set_placeholder(options.placeholder);
    auto& results{list(*content)};
    results.set_colors(&Theme::table_background, &Theme::table_foreground);
    // The details, when there are any, in a column of their own after the labels, lined up as a menu's shortcuts.
    auto detail_width{0};
    for(const auto& item : items)
    {
        detail_width = std::max(detail_width, shown_width(item.detail));
    }
    // Two cells between them, as an empty column: a table's gap would part its rows as well.
    if(detail_width > 0)
    {
        results.set_column_width(0, 1);
        results.set_column_width(1, 2);
        results.set_column_width(2, detail_width);
    }
    const auto fill{[&results, &items, &shown, detail_width]
                    {
                        results.clear();
                        for(const auto index : shown)
                        {
                            if(detail_width > 0)
                            {
                                results.add_line({items[index].label, {}, items[index].detail});
                            }
                            else
                            {
                                results.add_item(items[index].label);
                            }
                        }
                        if(!shown.empty())
                        {
                            results.select_row(0);
                        }
                    }};
    fill();
    const auto pick{[&picked, &shown, &dialog_reference](std::size_t row)
                    {
                        if(row < shown.size())
                        {
                            picked = shown[row];
                            dialog_reference.close(DialogResult::accepted);
                        }
                    }};
    results.on_row_activated(pick);
    // Typing while the list has the focus filters it, as if the filter had the focus.
    results.delegate_typing_to(filter);
    filter.on_changed(
        [&items, &shown, &options, &fill](std::string_view text)
        {
            shown = matches(items, text, options.row_limit);
            fill();
        });
    // Enter in the filter picks the best match left.
    filter.on_submitted([&pick](std::string_view) { pick(0); });
    dialog->add(std::move(content));
    // The width is fixed from every item and the height from the screen: filtering shortens the list, and a
    // window that shrinks under the pointer as you type loses the user.
    auto width{minimum_width};
    for(const auto& item : items)
    {
        width = std::max(width, shown_width(row_text(item)) + 4);
    }
    dialog->set_preferred_size({static_cast<double>(width), 0.0});
    if(!options.fit_to_items)
    {
        dialog->set_height_share(options.height_share);
    }
    // Room for the filter and three rows.
    dialog->set_minimum_size({30, 6});
    dialog->set_resizable(true);
    if(!options.placement_key.empty())
    {
        dialog->set_placement_key(options.placement_key);
    }
    static_cast<void>(application.show_modal(std::move(dialog)));
    return picked;
}

} // namespace olagarro::oui
