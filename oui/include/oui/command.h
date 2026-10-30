#pragma once

#include <oui/event.h>
#include <oui/translation.h>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace olagarro::oui
{

// A key combination that runs a command: either a character (letters in lowercase, Shift travels in
// `shift`) or a named key such as F3 or Page Down.
//
// Terminals send some Ctrl+letter combinations as the very bytes of other keys, so a program cannot tell them
// apart: Ctrl+M is Return, Ctrl+I is Tab, Ctrl+J a line feed and Ctrl+[ Escape. Shortcuts bound to those never
// arrive as themselves.
struct Shortcut
{
    char32_t character{};
    bool control{false};
    bool alt{false};
    bool shift{false};
    std::optional<Key> key{};
};

// Convenience for binding a named key: {Key::f3, false, false, true} is Shift+F3.
struct KeyShortcut
{
    Key key;
    bool control{false};
    bool alt{false};
    bool shift{false};
};

struct CommandInfo
{
    std::string id;
    std::string title;
    std::optional<Shortcut> shortcut;
    std::optional<bool> checked;
    bool enabled{true};
    bool palette_visible{true};
};

// What format_shortcut calls the modifiers and named keys: English, or their translation.
struct ShortcutNames
{
    std::string control{translate("Shortcut", "Ctrl+")};
    std::string alt{translate("Shortcut", "Alt+")};
    std::string shift{translate("Shortcut", "Shift+")};
    std::vector<std::string> keys{translate("Shortcut", "Tab"),
                                  translate("Shortcut", "Shift+Tab"),
                                  translate("Shortcut", "Enter"),
                                  translate("Shortcut", "Space"),
                                  translate("Shortcut", "Backspace"),
                                  translate("Shortcut", "Del"),
                                  translate("Shortcut", "Esc"),
                                  translate("Shortcut", "Left"),
                                  translate("Shortcut", "Right"),
                                  translate("Shortcut", "Up"),
                                  translate("Shortcut", "Down"),
                                  translate("Shortcut", "Home"),
                                  translate("Shortcut", "End"),
                                  translate("Shortcut", "PgUp"),
                                  translate("Shortcut", "PgDn"),
                                  translate("Shortcut", "Ins"),
                                  "F1",
                                  "F2",
                                  "F3",
                                  "F4",
                                  "F5",
                                  "F6",
                                  "F7",
                                  "F8",
                                  "F9",
                                  "F10",
                                  "F11",
                                  "F12"};
};

[[nodiscard]] std::string format_shortcut(const Shortcut& shortcut, const ShortcutNames& names = {});
// A shortcut written as text, as format_shortcut writes it in English: "Ctrl+Alt+U", "Shift+F3",
// "Ctrl+PageDown". Names are read in any case, and the usual other spellings work too: Control, Escape,
// Delete, Return. Nothing for text that is not a shortcut.
[[nodiscard]] std::optional<Shortcut> parse_shortcut(std::string_view text);

// Titles mark their mnemonic with '&', as in "&Open": the letter after it is shown underlined and
// activates the entry from the keyboard. "&&" is a literal ampersand.
[[nodiscard]] std::string strip_mnemonic(std::string_view title);
// The mnemonic letter in lowercase, or 0 when the title has none.
[[nodiscard]] char32_t mnemonic(std::string_view title);
// Where the mnemonic letter falls in strip_mnemonic(title), in bytes.
[[nodiscard]] std::optional<std::size_t> mnemonic_offset(std::string_view title);

class CommandRegistry
{
  public:
    using Callback = std::function<void()>;
    void register_command(std::string_view id, std::string_view title, Callback callback);
    // Removes the command and its shortcuts.
    void unregister_command(std::string_view id);
    void set_title(std::string_view id, std::string_view title);
    void set_tooltip(std::string_view id, std::string_view tooltip);
    void set_checked(std::string_view id, bool checked);
    // A disabled command does not run, from its shortcut or otherwise; menus show it dimmed and the
    // palette leaves it out.
    void set_enabled(std::string_view id, bool enabled);
    void set_palette_visible(std::string_view id, bool visible);
    // A reserved command's keys are its own even where the focus is on a widget that takes every key as its own, as a
    // terminal does for the program in it: the way out of there. Few should be.
    void reserve(std::string_view id);
    // The command must be registered first; binding an unknown one throws std::logic_error.
    void bind(std::string_view id, Shortcut shortcut);
    void bind(std::string_view id, KeyShortcut shortcut);
    [[nodiscard]] bool execute(std::string_view id);
    [[nodiscard]] std::optional<std::string_view> title(std::string_view id) const;
    [[nodiscard]] std::optional<std::string_view> tooltip(std::string_view id) const;
    [[nodiscard]] std::optional<bool> checked(std::string_view id) const;
    [[nodiscard]] bool enabled(std::string_view id) const;
    // The commands the palette offers: visible there and enabled.
    [[nodiscard]] std::vector<CommandInfo> commands() const;
    // Every command in the order they were registered, enabled or not and offered by the palette or not, as a list of
    // the program's keys wants them.
    [[nodiscard]] std::vector<CommandInfo> all_commands() const;
    // The first shortcut bound to the command.
    [[nodiscard]] std::optional<Shortcut> shortcut(std::string_view id) const;
    // Every shortcut bound to the command, in the order they were bound.
    [[nodiscard]] std::vector<Shortcut> shortcuts(std::string_view id) const;
    [[nodiscard]] bool dispatch(const Event& event);
    // Runs the reserved command the event is the shortcut of, when there is one.
    [[nodiscard]] bool dispatch_reserved(const Event& event);

  private:
    [[nodiscard]] bool dispatch(const Event& event, bool reserved_only);
    struct Command
    {
        std::string id;
        std::string title;
        std::string tooltip;
        Callback callback;
        std::optional<bool> checked;
        bool palette_visible{true};
        bool enabled{true};
        bool reserved{false};
    };
    struct Binding
    {
        std::string command_id;
        Shortcut shortcut;
    };
    [[nodiscard]] Command* find(std::string_view id);
    [[nodiscard]] const Command* find(std::string_view id) const;
    std::vector<Command> m_commands;
    std::vector<Binding> m_bindings;
};

} // namespace olagarro::oui
