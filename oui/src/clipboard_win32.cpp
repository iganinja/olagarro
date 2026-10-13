#include <oui/backend.h>
#include <oui/clipboard.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

namespace
{

// What a MIME type is made of, as on every system: a type is refused alike everywhere.
[[nodiscard]] bool is_type(std::string_view type)
{
    return !type.empty() && std::all_of(type.begin(), type.end(),
                                        [](char character)
                                        {
                                            return std::isalnum(static_cast<unsigned char>(character)) != 0 ||
                                                   std::string_view{"/+.-_"}.find(character) != std::string_view::npos;
                                        });
}

// A type as Windows' clipboard knows it: images as the formats other programs use, the rest by their own names.
[[nodiscard]] UINT format_of(std::string_view type)
{
    if(type == "image/bmp")
    {
        return CF_DIB;
    }
    if(type == "image/png")
    {
        return RegisterClipboardFormatW(L"PNG");
    }
    return RegisterClipboardFormatW(to_wide(type).c_str());
}

// A format as a type: the reverse of format_of; none for formats with no type.
[[nodiscard]] std::optional<std::string> type_of(UINT format)
{
    if(format == CF_DIB || format == CF_DIBV5 || format == CF_BITMAP)
    {
        return "image/bmp";
    }
    wchar_t name[256]{};
    const auto length{GetClipboardFormatNameW(format, name, 256)};
    if(length <= 0)
    {
        return std::nullopt;
    }
    const auto text{to_utf8({name, static_cast<std::size_t>(length)})};
    if(text == "PNG")
    {
        return "image/png";
    }
    if(text.find('/') != std::string::npos)
    {
        return text;
    }
    return std::nullopt;
}

[[nodiscard]] std::uint32_t read32(const std::string& bytes, std::size_t at)
{
    return static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[at])) |
           static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[at + 1])) << 8u |
           static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[at + 2])) << 16u |
           static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[at + 3])) << 24u;
}

// A device-independent bitmap as a BMP file: its 14 bytes of file header before it, saying where the pixels start,
// after the information and any masks or colour table.
[[nodiscard]] std::string bmp_file(const std::string& dib)
{
    if(dib.size() < 40)
    {
        return {};
    }
    const auto header_size{read32(dib, 0)};
    const auto bits{
        static_cast<std::uint32_t>(static_cast<unsigned char>(dib[14]) | static_cast<unsigned char>(dib[15]) << 8u)};
    const auto compression{read32(dib, 16)};
    auto colors{read32(dib, 32)};
    if(colors == 0 && bits <= 8)
    {
        colors = 1u << bits;
    }
    // BI_BITFIELDS keeps three masks after a header of 40 bytes; the longer headers hold them.
    const auto masks{compression == 3 && header_size == 40 ? 12u : 0u};
    const auto offset{14u + header_size + masks + colors * 4u};
    std::string file{"BM"};
    const auto put32{[&file](std::uint32_t value)
                     {
                         for(int shift{}; shift < 32; shift += 8)
                         {
                             file += static_cast<char>((value >> shift) & 0xffu);
                         }
                     }};
    put32(static_cast<std::uint32_t>(14u + dib.size()));
    put32(0);
    put32(offset);
    return file + dib;
}

// What of a block the clipboard gave is the data: Windows may give a block bigger than what was put in it.
[[nodiscard]] std::string trimmed(std::string bytes, std::string_view type)
{
    if(type == "image/png")
    {
        // A PNG ends with its IEND chunk and the chunk's 4 bytes of check.
        if(const auto end{bytes.rfind("IEND")}; end != std::string::npos && end + 8 <= bytes.size())
        {
            bytes.resize(end + 8);
        }
    }
    else if(type == "image/bmp" && bytes.size() >= 40)
    {
        const auto header_size{read32(bytes, 0)};
        const auto width{static_cast<std::int32_t>(read32(bytes, 4))};
        const auto height{static_cast<std::int32_t>(read32(bytes, 8))};
        const auto bits{static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[14]) |
                                                   static_cast<unsigned char>(bytes[15]) << 8u)};
        const auto compression{read32(bytes, 16)};
        auto colors{read32(bytes, 32)};
        if(colors == 0 && bits <= 8)
        {
            colors = 1u << bits;
        }
        // Uncompressed rows only: each a whole number of four bytes.
        if(compression == 0 || compression == 3)
        {
            const auto masks{compression == 3 && header_size == 40 ? 12u : 0u};
            const auto stride{(static_cast<std::uint64_t>(std::abs(width)) * bits + 31u) / 32u * 4u};
            const auto size{header_size + masks + colors * 4u + stride * static_cast<std::uint64_t>(std::abs(height))};
            if(size <= bytes.size())
            {
                bytes.resize(static_cast<std::size_t>(size));
            }
        }
    }
    return bytes;
}

} // namespace

bool Clipboard::set_data(const std::vector<Item>& items)
{
    if(items.empty())
    {
        return false;
    }
    for(const auto& item : items)
    {
        if(!is_type(item.type))
        {
            return false;
        }
    }
    m_fallback_type = items.front().type;
    m_fallback_data = items.front().bytes;
    if(!OpenClipboard(nullptr))
    {
        return false;
    }
    EmptyClipboard();
    auto offered{false};
    for(const auto& item : items)
    {
        // A BMP file goes as the bitmap inside it, past its file header.
        const std::string_view bytes{item.type == "image/bmp" && item.bytes.size() > 14
                                         ? std::string_view{item.bytes}.substr(14)
                                         : std::string_view{item.bytes}};
        HGLOBAL memory{GlobalAlloc(GMEM_MOVEABLE, std::max<std::size_t>(1, bytes.size()))};
        if(memory == nullptr)
        {
            continue;
        }
        auto* const target{static_cast<char*>(GlobalLock(memory))};
        if(target == nullptr)
        {
            GlobalFree(memory);
            continue;
        }
        std::copy(bytes.begin(), bytes.end(), target);
        GlobalUnlock(memory);
        if(SetClipboardData(format_of(item.type), memory) == nullptr)
        {
            GlobalFree(memory);
            continue;
        }
        offered = true;
    }
    CloseClipboard();
    return offered;
}

bool Clipboard::set_data(std::string_view type, std::string_view bytes)
{
    return set_data({Item{std::string{type}, std::string{bytes}}});
}

std::vector<std::string> Clipboard::types() const
{
    std::vector<std::string> result;
    if(!OpenClipboard(nullptr))
    {
        return m_fallback_type.empty() ? result : std::vector<std::string>{m_fallback_type};
    }
    for(auto format{EnumClipboardFormats(0)}; format != 0; format = EnumClipboardFormats(format))
    {
        if(const auto type{type_of(format)}; type && std::find(result.begin(), result.end(), *type) == result.end())
        {
            result.push_back(*type);
        }
    }
    CloseClipboard();
    return result;
}

std::optional<std::string> Clipboard::data(std::string_view type) const
{
    if(!is_type(type))
    {
        return std::nullopt;
    }
    if(!OpenClipboard(nullptr))
    {
        return type == m_fallback_type && !type.empty() ? std::optional<std::string>{m_fallback_data} : std::nullopt;
    }
    std::optional<std::string> result;
    // An image as PNG may be under the format other programs use or under its type's own name.
    std::vector<UINT> formats{format_of(type)};
    if(type == "image/png")
    {
        formats.push_back(RegisterClipboardFormatW(L"image/png"));
    }
    for(const auto format : formats)
    {
        if(HANDLE memory{GetClipboardData(format)}; memory != nullptr)
        {
            const auto* const source{static_cast<const char*>(GlobalLock(memory))};
            if(source != nullptr)
            {
                auto bytes{trimmed(std::string{source, GlobalSize(memory)}, type)};
                GlobalUnlock(memory);
                result = type == "image/bmp" ? bmp_file(bytes) : bytes;
                break;
            }
        }
    }
    CloseClipboard();
    return result;
}

} // namespace olagarro::oui
