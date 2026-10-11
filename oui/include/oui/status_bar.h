#pragma once

#include <oui/container.h>
#include <oui/widget.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace olagarro::oui
{

// A key and what it does, as the start of a status line shows them: "F1 Help".
struct StatusKey
{
    std::string key;
    std::string label;
};

struct StatusText
{
    std::string left;
    std::string right;
    // Before `left`, as Turbo Vision's status line: the key in the colour of shortcuts, then what it does.
    std::vector<StatusKey> keys{};
};

// How many cells `keys` take at the start of a status line, with the line that sets them apart from the text after.
[[nodiscard]] int status_keys_width(const std::vector<StatusKey>& keys);

// A one-row bar with text at its left and right ends.
class StatusBar final : public Widget
{
  public:
    void set_left(std::string text);
    void set_right(std::string text);
    // When set, asked for both texts before every paint, so the bar follows whatever it describes without
    // being told about each change.
    void set_provider(std::function<StatusText()> provider);

    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void paint(Painter& painter, const Theme& theme) const override;

  private:
    StatusText m_text;
    std::function<StatusText()> m_provider;
};

[[nodiscard]] std::unique_ptr<StatusBar> status_bar();
StatusBar& status_bar(Container& parent);

} // namespace olagarro::oui
