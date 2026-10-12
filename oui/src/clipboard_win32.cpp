#include <oui/backend.h>
#include <oui/clipboard.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <string>
#include <string_view>

namespace olagarro::oui
{

namespace
{

[[nodiscard]] std::wstring to_wide(std::string_view text)
{
    const int size{MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0)};
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

[[nodiscard]] std::string to_utf8(std::wstring_view text)
{
    const int size{
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr)};
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
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
    // Windows has one clipboard API and needs no helper program.
    m_tool = std::optional<Tool>{};
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
    const auto wide{to_wide(text)};
    if(!OpenClipboard(nullptr))
    {
        return false;
    }
    EmptyClipboard();
    HGLOBAL memory{GlobalAlloc(GMEM_MOVEABLE, (wide.size() + 1u) * sizeof(wchar_t))};
    if(memory == nullptr)
    {
        CloseClipboard();
        return false;
    }
    wchar_t* const target{static_cast<wchar_t*>(GlobalLock(memory))};
    if(target == nullptr)
    {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }
    std::copy(wide.begin(), wide.end(), target);
    target[wide.size()] = L'\0';
    GlobalUnlock(memory);
    if(SetClipboardData(CF_UNICODETEXT, memory) == nullptr)
    {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }
    CloseClipboard();
    return true;
}

std::string Clipboard::text() const
{
    if(m_terminal != nullptr && m_terminal->keeps_clipboard())
    {
        return m_terminal->clipboard_text();
    }
    if(OpenClipboard(nullptr))
    {
        HANDLE memory{GetClipboardData(CF_UNICODETEXT)};
        if(memory != nullptr)
        {
            const wchar_t* const source{static_cast<const wchar_t*>(GlobalLock(memory))};
            if(source != nullptr)
            {
                const auto result{to_utf8(source)};
                GlobalUnlock(memory);
                CloseClipboard();
                return result;
            }
        }
        CloseClipboard();
    }
    return m_fallback;
}

} // namespace olagarro::oui
