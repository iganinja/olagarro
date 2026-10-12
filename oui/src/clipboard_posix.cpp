#include <oui/backend.h>
#include <oui/clipboard.h>

#include <array>
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
            m_tool = Tool{"wl-copy", "wl-paste --no-newline"};
        }
        else if(std::getenv("DISPLAY") != nullptr && has_program("xclip"))
        {
            m_tool = Tool{"xclip -selection clipboard", "xclip -selection clipboard -o"};
        }
        else if(std::getenv("DISPLAY") != nullptr && has_program("xsel"))
        {
            m_tool = Tool{"xsel --clipboard --input", "xsel --clipboard --output"};
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

} // namespace olagarro::oui
