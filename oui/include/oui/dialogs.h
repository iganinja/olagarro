#pragma once

#include <oui/application.h>
#include <oui/container.h>
#include <oui/translation.h>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace olagarro::oui
{

// A modal with a message, one line per '\n', and a row of buttons. Returns the index of the button the
// user pressed, or nothing when they dismissed it with Escape. The first button starts focused.
[[nodiscard]] std::optional<std::size_t>
message_box(Application& application, std::string_view title, std::string_view message,
            const std::vector<std::string>& buttons = {translate("Dialogs", "OK")});

struct PromptOptions
{
    std::string title;
    std::string label;
    std::string text;
    // The bytes of `text` selected when the prompt opens, so typing replaces them; all of it when not set.
    std::optional<std::pair<std::size_t, std::size_t>> selection;
    std::string placeholder;
    std::string accept{translate("Dialogs", "OK")};
    std::string cancel{translate("Dialogs", "Cancel")};
    // How many cells wide the dialog opens, at least, for a long text; as its content needs when not set. It grows
    // with what is typed up to this, or 60 cells, and then the field scrolls.
    std::optional<int> width;
    // Checks the text before the dialog accepts it; a returned message is shown and the dialog stays open.
    std::function<std::optional<std::string>(std::string_view text)> validate;
    // Adds more to ask under the field, such as a choice. What it adds goes with the dialog, so it hands what the
    // user chose to the caller as it changes.
    std::function<void(Container& under_field)> extra;
};

// A modal asking for one line of text. Returns the text accepted with Enter or the accept button, or
// nothing when the user cancelled.
[[nodiscard]] std::optional<std::string> prompt(Application& application, const PromptOptions& options);

} // namespace olagarro::oui
