#pragma once

#include <oui/application.h>
#include <oui/translation.h>

#include <optional>
#include <string>
#include <vector>

namespace olagarro::oui
{

struct QuickPickItem
{
    // What the filter matches and the list shows.
    std::string label;
    // Shown after the label, such as a shortcut or a folder, and not matched.
    std::string detail;
};

struct QuickPickOptions
{
    std::string title;
    std::string placeholder{translate("QuickPick", "Type to filter")};
    // The share of the screen's height the dialog opens with; a long list scrolls inside it.
    double height_share{0.8};
    // As tall as the items need instead, for a short list such as a menu; no taller than the screen.
    bool fit_to_items{false};
    // The most rows shown at once, best matches first, so a long list stays quick to filter.
    std::size_t row_limit{500};
    // Remembers where the user left the dialog, through Application::set_window_placement_store.
    std::string placement_key;
};

// A modal filter field above a list, as code editors pick a file or a command: typing narrows the items to
// those matching fuzzily, best first, and Enter, a click or Enter in the list picks one. Typing in the list
// goes to the filter. A filter with '*' or '?' is a wildcard pattern instead, as in a shell: "hola*.h" matches
// the last part of paths, "src/*.h" whole paths, and "*edit*" any name with "edit" in it. Returns the picked
// item's index in `items`, or nothing when cancelled.
[[nodiscard]] std::optional<std::size_t> quick_pick(Application& application, const std::vector<QuickPickItem>& items,
                                                    const QuickPickOptions& options);

} // namespace olagarro::oui
