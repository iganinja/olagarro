#include <oui/backend.h>
#include <oui/clipboard.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <string_view>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <unistd.h>

namespace olagarro::oui
{

namespace
{

[[nodiscard]] bool has_program(const char* const name)
{
    const std::string command{std::string{"command -v "} + name + " >/dev/null 2>&1"};
    return std::system(command.c_str()) == 0;
}

// A type goes into a shell command, so only what a MIME type is made of.
[[nodiscard]] bool is_type(std::string_view type)
{
    return !type.empty() && std::all_of(type.begin(), type.end(),
                                        [](char character)
                                        {
                                            return std::isalnum(static_cast<unsigned char>(character)) != 0 ||
                                                   std::string_view{"/+.-_"}.find(character) != std::string_view::npos;
                                        });
}

[[nodiscard]] std::string with_type(const std::string& command, std::string_view type)
{
    auto result{command};
    if(const auto at{result.find("{}")}; at != std::string::npos)
    {
        result.replace(at, 2, type);
    }
    return result;
}

// Everything a command writes, none when it fails.
[[nodiscard]] std::optional<std::string> output_of(const std::string& command)
{
    auto* const pipe{popen((command + " 2>/dev/null").c_str(), "r")};
    if(pipe == nullptr)
    {
        return std::nullopt;
    }
    std::string result;
    std::array<char, 65536> buffer{};
    while(const auto count{std::fread(buffer.data(), 1, buffer.size(), pipe)})
    {
        result.append(buffer.data(), count);
    }
    if(pclose(pipe) != 0)
    {
        return std::nullopt;
    }
    return result;
}

} // namespace

Clipboard::Clipboard(Backend* const terminal) : m_terminal{terminal}
{
}

void Clipboard::set_backend(Backend* const terminal)
{
    m_terminal = terminal;
}

const std::optional<Clipboard::Tool>& Clipboard::tool() const
{
    if(!m_tool)
    {
        if(std::getenv("WAYLAND_DISPLAY") != nullptr && has_program("wl-copy"))
        {
            m_tool = Tool{"wl-copy", "wl-paste --no-newline", "wl-copy --type {}", "wl-paste --no-newline --type {}",
                          "wl-paste --list-types"};
        }
        else if(std::getenv("DISPLAY") != nullptr && has_program("xclip"))
        {
            m_tool =
                Tool{"xclip -selection clipboard", "xclip -selection clipboard -o", "xclip -selection clipboard -t {}",
                     "xclip -selection clipboard -t {} -o", "xclip -selection clipboard -t TARGETS -o"};
        }
        else if(std::getenv("DISPLAY") != nullptr && has_program("xsel"))
        {
            m_tool = Tool{"xsel --clipboard --input", "xsel --clipboard --output", {}, {}, {}};
        }
        else
        {
            m_tool = std::optional<Tool>{};
        }
    }
    return *m_tool;
}

bool Clipboard::set_text(std::string_view text)
{
    m_fallback = text;
    if(m_terminal != nullptr && m_terminal->keeps_clipboard())
    {
        m_terminal->set_clipboard_text(text);
        return true;
    }
    if(m_terminal != nullptr)
    {
        m_terminal->set_clipboard_text(text);
    }
    if(const auto& found{tool()})
    {
        const std::string command{found->copy + " >/dev/null 2>&1"};
        if(auto* const pipe{popen(command.c_str(), "w")})
        {
            std::fwrite(text.data(), 1, text.size(), pipe);
            return pclose(pipe) == 0;
        }
    }
    return true;
}

std::string Clipboard::text() const
{
    if(m_terminal != nullptr && m_terminal->keeps_clipboard())
    {
        return m_terminal->clipboard_text();
    }
    const auto& found{tool()};
    if(!found)
    {
        return m_fallback;
    }
    const std::string command{found->paste + " 2>/dev/null"};
    auto* const pipe{popen(command.c_str(), "r")};
    if(pipe == nullptr)
    {
        return m_fallback;
    }
    std::string result;
    std::array<char, 4096> buffer{};
    while(const auto count{std::fread(buffer.data(), 1, buffer.size(), pipe)})
    {
        result.append(buffer.data(), count);
    }
    return pclose(pipe) == 0 ? result : m_fallback;
}

bool Clipboard::set_data(std::string_view type, std::string_view bytes)
{
    if(!is_type(type))
    {
        return false;
    }
    m_fallback_type = type;
    m_fallback_data = bytes;
    const auto& found{tool()};
    if(!found || found->copy_data.empty())
    {
        return true;
    }
    const auto command{with_type(found->copy_data, type) + " >/dev/null 2>&1"};
    auto* const pipe{popen(command.c_str(), "w")};
    if(pipe == nullptr)
    {
        return false;
    }
    std::fwrite(bytes.data(), 1, bytes.size(), pipe);
    return pclose(pipe) == 0;
}

std::vector<std::string> Clipboard::types() const
{
    const auto& found{tool()};
    if(!found || found->list_types.empty())
    {
        return m_fallback_type.empty() ? std::vector<std::string>{} : std::vector<std::string>{m_fallback_type};
    }
    std::vector<std::string> result;
    const auto listed{output_of(found->list_types)};
    if(!listed)
    {
        return result;
    }
    std::size_t start{};
    while(start < listed->size())
    {
        auto end{listed->find('\n', start)};
        if(end == std::string::npos)
        {
            end = listed->size();
        }
        auto line{listed->substr(start, end - start)};
        while(!line.empty() && (line.back() == '\r' || line.back() == ' '))
        {
            line.pop_back();
        }
        if(!line.empty())
        {
            result.push_back(std::move(line));
        }
        start = end + 1;
    }
    return result;
}

std::optional<std::string> Clipboard::data(std::string_view type) const
{
    if(!is_type(type))
    {
        return std::nullopt;
    }
    const auto& found{tool()};
    if(!found || found->paste_data.empty())
    {
        return type == m_fallback_type ? std::optional<std::string>{m_fallback_data} : std::nullopt;
    }
    // Asked only for a type it holds: some tools answer for any type with whatever they have.
    const auto held{types()};
    if(std::find(held.begin(), held.end(), type) == held.end())
    {
        return std::nullopt;
    }
    return output_of(with_type(found->paste_data, type));
}

} // namespace olagarro::oui
