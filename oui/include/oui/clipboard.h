#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace olagarro::oui
{

class Backend;

// The system clipboard: the Windows clipboard, or wl-copy, xclip or xsel on a Linux desktop, found the first
// time they are needed. Text it cannot hand to the system stays in the clipboard itself, so copy and paste
// still work within the application. With a backend it also offers copied text to the terminal, which can
// reach the user's clipboard even over SSH. A backend that keeps a clipboard of its own, as OUI's desktop does for
// all the programs on it, has it instead of the system.
class Clipboard
{
  public:
    explicit Clipboard(Backend* terminal = nullptr);

    // The backend it offers copied text to from now on, as when the application's is replaced by another.
    void set_backend(Backend* terminal);

    [[nodiscard]] bool set_text(std::string_view text);
    [[nodiscard]] std::string text() const;

  private:
    struct Tool
    {
        std::string copy;
        std::string paste;
    };
    [[nodiscard]] const std::optional<Tool>& tool() const;

    Backend* m_terminal;
    std::string m_fallback;
    mutable std::optional<std::optional<Tool>> m_tool;
};

} // namespace olagarro::oui
