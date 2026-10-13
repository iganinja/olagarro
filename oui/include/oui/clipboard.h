#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

    // Data of a type, as an image as image/png, in place of what the clipboard held: through wl-copy on Wayland or
    // xclip on X11, so other programs can paste it, and kept in the clipboard itself where there is neither. A type
    // is letters, digits and "/+.-_" only; false for one that is not, or when the system's clipboard refused it.
    [[nodiscard]] bool set_data(std::string_view type, std::string_view bytes);
    // The same data in several types at once, as an image as PNG and as BMP for programs that take nothing else. On
    // Windows every one is offered: image/png as the "PNG" format other programs use, image/bmp, a BMP file, as a
    // device-independent bitmap (CF_DIB). wl-copy and xclip take one type at a time, so there only the first.
    struct Item
    {
        std::string type;
        std::string bytes;
    };
    [[nodiscard]] bool set_data(const std::vector<Item>& items);
    // The types of data the clipboard holds now, as another program or this one offered them.
    [[nodiscard]] std::vector<std::string> types() const;
    // What it holds of `type`, none when it holds nothing of it. On Windows image/bmp is a device-independent bitmap
    // given back as a BMP file, as screenshots and older programs offer images.
    [[nodiscard]] std::optional<std::string> data(std::string_view type) const;

  private:
    struct Tool
    {
        std::string copy;
        std::string paste;
        // With {} for the type; empty for a tool that only carries text.
        std::string copy_data;
        std::string paste_data;
        std::string list_types;
    };
    [[nodiscard]] const std::optional<Tool>& tool() const;

    Backend* m_terminal;
    std::string m_fallback;
    // Data kept here where no program carries it: its type and its bytes.
    std::string m_fallback_type;
    std::string m_fallback_data;
    mutable std::optional<std::optional<Tool>> m_tool;
};

} // namespace olagarro::oui
