#include <oui/command.h>

#include <stdexcept>
#include <oui/utf8.h>

#include <algorithm>
#include <cctype>

namespace olagarro::oui
{

std::string format_shortcut(const Shortcut& shortcut, const ShortcutNames& names)
{
    std::string result;
    if(shortcut.control)
    {
        result += names.control;
    }
    if(shortcut.alt)
    {
        result += names.alt;
    }
    if(shortcut.shift)
    {
        result += names.shift;
    }
    if(shortcut.key)
    {
        const auto index{static_cast<std::size_t>(*shortcut.key)};
        result += index < names.keys.size() ? names.keys.at(index) : std::string{"?"};
    }
    else
    {
        const auto character{shortcut.character};
        result += encode_utf8(character < 0x80 ? static_cast<char32_t>(std::toupper(static_cast<int>(character)))
                                               : character);
    }
    return result;
}

std::optional<Shortcut> parse_shortcut(std::string_view text)
{
    const auto lower{[](std::string_view part)
                     {
                         std::string result{part};
                         for(auto& character : result)
                         {
                             character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
                         }
                         return result;
                     }};
    static const std::vector<std::pair<std::string_view, Key>> keys{{"tab", Key::tab},
                                                                    {"enter", Key::enter},
                                                                    {"return", Key::enter},
                                                                    {"space", Key::space},
                                                                    {"backspace", Key::backspace},
                                                                    {"del", Key::delete_},
                                                                    {"delete", Key::delete_},
                                                                    {"esc", Key::escape},
                                                                    {"escape", Key::escape},
                                                                    {"left", Key::left},
                                                                    {"right", Key::right},
                                                                    {"up", Key::up},
                                                                    {"down", Key::down},
                                                                    {"home", Key::home},
                                                                    {"end", Key::end},
                                                                    {"pgup", Key::page_up},
                                                                    {"pageup", Key::page_up},
                                                                    {"pgdn", Key::page_down},
                                                                    {"pagedown", Key::page_down},
                                                                    {"ins", Key::insert_},
                                                                    {"insert", Key::insert_},
                                                                    {"f1", Key::f1},
                                                                    {"f2", Key::f2},
                                                                    {"f3", Key::f3},
                                                                    {"f4", Key::f4},
                                                                    {"f5", Key::f5},
                                                                    {"f6", Key::f6},
                                                                    {"f7", Key::f7},
                                                                    {"f8", Key::f8},
                                                                    {"f9", Key::f9},
                                                                    {"f10", Key::f10},
                                                                    {"f11", Key::f11},
                                                                    {"f12", Key::f12}};
    Shortcut shortcut;
    // A trailing '+' is the key itself, as in Ctrl++.
    std::size_t start{};
    while(true)
    {
        const auto plus{text.find('+', start)};
        if(plus == std::string_view::npos || plus + 1 == text.size() || plus == start)
        {
            break;
        }
        const auto modifier{lower(text.substr(start, plus - start))};
        if(modifier == "ctrl" || modifier == "control")
        {
            shortcut.control = true;
        }
        else if(modifier == "alt")
        {
            shortcut.alt = true;
        }
        else if(modifier == "shift")
        {
            shortcut.shift = true;
        }
        else
        {
            return std::nullopt;
        }
        start = plus + 1;
    }
    const auto key{text.substr(start)};
    const auto name{lower(key)};
    for(const auto& [spelling, value] : keys)
    {
        if(spelling == name)
        {
            shortcut.key = value;
            return shortcut;
        }
    }
    std::size_t index{};
    const auto character{decode_utf8(key, index)};
    if(key.empty() || index != key.size())
    {
        return std::nullopt;
    }
    // Shortcuts are bound to lowercase letters; Shift travels in its flag.
    shortcut.character =
        character < 0x80 ? static_cast<char32_t>(std::tolower(static_cast<int>(character))) : character;
    return shortcut;
}

std::string strip_mnemonic(std::string_view title)
{
    std::string result;
    for(std::size_t index{}; index < title.size(); ++ index)
    {
        if(title.at(index) == '&' && index + 1 < title.size())
        {
            ++ index;
        }
        result += title.at(index);
    }
    return result;
}

std::optional<std::size_t> mnemonic_offset(std::string_view title)
{
    std::size_t shown{};
    for(std::size_t index{}; index < title.size(); ++ index)
    {
        if(title.at(index) == '&' && index + 1 < title.size())
        {
            if(title.at(index + 1) != '&')
            {
                return shown;
            }
            ++ index;
        }
        ++ shown;
    }
    return std::nullopt;
}

char32_t mnemonic(std::string_view title)
{
    const auto offset{mnemonic_offset(title)};
    if(!offset)
    {
        return 0;
    }
    const auto shown{strip_mnemonic(title)};
    auto index{*offset};
    const auto character{decode_utf8(shown, index)};
    return character < 0x80 ? static_cast<char32_t>(std::tolower(static_cast<int>(character))) : character;
}

void CommandRegistry::register_command(std::string_view id, std::string_view title, Callback callback)
{
    if(auto* const command{find(id)})
    {
        command->title = title;
        command->callback = std::move(callback);
        return;
    }
    m_commands.push_back({std::string{id}, std::string{title}, {}, std::move(callback), std::nullopt, true, true});
}
void CommandRegistry::unregister_command(std::string_view id)
{
    m_commands.erase(
        std::remove_if(m_commands.begin(), m_commands.end(), [id](const Command& command) { return command.id == id; }),
        m_commands.end());
    m_bindings.erase(std::remove_if(m_bindings.begin(), m_bindings.end(),
                                    [id](const Binding& binding) { return binding.command_id == id; }),
                     m_bindings.end());
}
void CommandRegistry::set_title(std::string_view id, std::string_view title)
{
    if(auto* const command{find(id)})
    {
        command->title = title;
    }
}
void CommandRegistry::set_tooltip(std::string_view id, std::string_view tooltip)
{
    if(auto* const command{find(id)})
    {
        command->tooltip = tooltip;
    }
}
void CommandRegistry::set_checked(std::string_view id, bool checked)
{
    if(auto* const command{find(id)})
    {
        command->checked = checked;
    }
}
void CommandRegistry::set_enabled(std::string_view id, bool enabled)
{
    if(auto* const command{find(id)})
    {
        command->enabled = enabled;
    }
}
void CommandRegistry::set_palette_visible(std::string_view id, bool visible)
{
    if(auto* const command{find(id)})
    {
        command->palette_visible = visible;
    }
}
void CommandRegistry::reserve(std::string_view id)
{
    auto* const command{find(id)};
    if(command == nullptr)
    {
        throw std::logic_error{"Cannot reserve the keys of the unknown command \"" + std::string{id} + "\"."};
    }
    command->reserved = true;
}
void CommandRegistry::bind(std::string_view id, Shortcut shortcut)
{
    // Keys for a command that does not exist would never do anything: a slip, such as binding before
    // registering, that should show at once.
    if(find(id) == nullptr)
    {
        throw std::logic_error{"Cannot bind keys to the unknown command \"" + std::string{id} + "\"."};
    }
    m_bindings.push_back({std::string{id}, shortcut});
}
void CommandRegistry::bind(std::string_view id, KeyShortcut shortcut)
{
    bind(id, Shortcut{0, shortcut.control, shortcut.alt, shortcut.shift, shortcut.key});
}
bool CommandRegistry::execute(std::string_view id)
{
    auto* const command{find(id)};
    if(command == nullptr || !command->callback || !command->enabled)
    {
        return false;
    }
    command->callback();
    return true;
}
std::optional<std::string_view> CommandRegistry::title(std::string_view id) const
{
    const auto* const command{find(id)};
    return command == nullptr ? std::nullopt : std::optional<std::string_view>{command->title};
}
std::optional<std::string_view> CommandRegistry::tooltip(std::string_view id) const
{
    const auto* const command{find(id)};
    return command == nullptr || command->tooltip.empty() ? std::nullopt
                                                          : std::optional<std::string_view>{command->tooltip};
}
std::optional<bool> CommandRegistry::checked(std::string_view id) const
{
    const auto* const command{find(id)};
    return command == nullptr ? std::nullopt : command->checked;
}
bool CommandRegistry::enabled(std::string_view id) const
{
    const auto* const command{find(id)};
    return command != nullptr && command->enabled;
}
std::vector<CommandInfo> CommandRegistry::commands() const
{
    std::vector<CommandInfo> result;
    result.reserve(m_commands.size());
    for(const auto& command : m_commands)
    {
        if(command.palette_visible && command.enabled)
        {
            result.push_back({command.id, command.title, shortcut(command.id), command.checked, command.enabled});
        }
    }
    return result;
}
std::vector<CommandInfo> CommandRegistry::all_commands() const
{
    std::vector<CommandInfo> result;
    result.reserve(m_commands.size());
    for(const auto& command : m_commands)
    {
        result.push_back({command.id, command.title, shortcut(command.id), command.checked, command.enabled,
                          command.palette_visible});
    }
    return result;
}
std::optional<Shortcut> CommandRegistry::shortcut(std::string_view id) const
{
    for(const auto& binding : m_bindings)
    {
        if(binding.command_id == id)
        {
            return binding.shortcut;
        }
    }
    return std::nullopt;
}
std::vector<Shortcut> CommandRegistry::shortcuts(std::string_view id) const
{
    std::vector<Shortcut> result;
    for(const auto& binding : m_bindings)
    {
        if(binding.command_id == id)
        {
            result.push_back(binding.shortcut);
        }
    }
    return result;
}
bool CommandRegistry::dispatch(const Event& event)
{
    return dispatch(event, false);
}
bool CommandRegistry::dispatch_reserved(const Event& event)
{
    return dispatch(event, true);
}
bool CommandRegistry::dispatch(const Event& event, bool reserved_only)
{
    std::optional<Key> key;
    char32_t character{};
    auto control{false};
    auto alt{false};
    auto shift{false};
    if(const auto* const text{std::get_if<TextEvent>(&event)})
    {
        character = text->codepoint;
        control = text->control;
        alt = text->alt;
        shift = text->shift;
        // The Windows console reports Ctrl+letter as the control character itself, and Ctrl+/ as 0x1f.
        if(control && character >= 1 && character <= 26)
        {
            character = U'a' + character - 1;
        }
        else if(control && character == 0x1f)
        {
            character = U'/';
        }
    }
    else if(const auto* const key_event{std::get_if<KeyEvent>(&event)})
    {
        key = key_event->key;
        control = key_event->control;
        alt = key_event->alt;
        shift = key_event->shift;
    }
    else
    {
        return false;
    }
    // An exact match wins; with Ctrl held, a binding without Shift also answers Ctrl+Shift, so Ctrl+Shift+Z
    // still undoes when nothing is bound to it.
    for(const auto check_shift : {true, false})
    {
        if(!check_shift && !control)
        {
            break;
        }
        for(const auto& binding : m_bindings)
        {
            const auto& shortcut{binding.shortcut};
            const auto same_key{shortcut.key == key && (key.has_value() || shortcut.character == character)};
            const auto same_modifiers{shortcut.control == control && shortcut.alt == alt &&
                                      (check_shift ? shortcut.shift == shift : !shortcut.shift)};
            if(same_key && same_modifiers && enabled(binding.command_id) &&
               (!reserved_only || find(binding.command_id)->reserved))
            {
                return execute(binding.command_id);
            }
        }
    }
    return false;
}
CommandRegistry::Command* CommandRegistry::find(std::string_view id)
{
    for(auto& command : m_commands)
    {
        if(command.id == id)
        {
            return &command;
        }
    }
    return nullptr;
}
const CommandRegistry::Command* CommandRegistry::find(std::string_view id) const
{
    for(const auto& command : m_commands)
    {
        if(command.id == id)
        {
            return &command;
        }
    }
    return nullptr;
}

} // namespace olagarro::oui
