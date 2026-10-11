#include <oui/text_file.h>
#include <oui/file_system.h>
#include <oui/utf8.h>
#include <oui/translation.h>

#include <array>
#include <cerrno>
#include <iterator>
#include <system_error>

#include "atomic_file.h"

namespace olagarro::oui
{

namespace
{

constexpr std::string_view byte_order_mark{"\xEF\xBB\xBF"};

// Windows-1252 bytes 0x80..0x9F. The five bytes the code page leaves undefined map to the C1 control of
// the same value, as Latin-1 would, so any byte sequence survives a load and a save unchanged.
constexpr std::array<char32_t, 32> windows_1252_high{
    U'€',      U'\u0081', U'‚', U'ƒ', U'„', U'…', U'†', U'‡', U'ˆ', U'‰', U'Š', U'‹', U'Œ', U'\u008D', U'Ž', U'\u008F',
    U'\u0090', U'‘',      U'’', U'“', U'”', U'•', U'–', U'—', U'˜', U'™', U'š', U'›', U'œ', U'\u009D', U'ž', U'Ÿ'};

[[nodiscard]] bool is_continuation(unsigned char byte)
{
    return (byte & 0xC0) == 0x80;
}

// Strict UTF-8: no overlong forms, no surrogates, nothing past U+10FFFF.
[[nodiscard]] bool is_valid_utf8(std::string_view bytes)
{
    for(std::size_t index{}; index < bytes.size();)
    {
        const auto first{static_cast<unsigned char>(bytes.at(index))};
        std::size_t length{1};
        unsigned char low{0x80};
        unsigned char high{0xBF};
        if(first < 0x80)
        {
            ++ index;
            continue;
        }
        if(first >= 0xC2 && first <= 0xDF)
        {
            length = 2;
        }
        else if(first >= 0xE0 && first <= 0xEF)
        {
            length = 3;
            low = first == 0xE0 ? 0xA0 : 0x80;
            high = first == 0xED ? 0x9F : 0xBF;
        }
        else if(first >= 0xF0 && first <= 0xF4)
        {
            length = 4;
            low = first == 0xF0 ? 0x90 : 0x80;
            high = first == 0xF4 ? 0x8F : 0xBF;
        }
        else
        {
            return false;
        }
        if(index + length > bytes.size())
        {
            return false;
        }
        const auto second{static_cast<unsigned char>(bytes.at(index + 1))};
        if(second < low || second > high)
        {
            return false;
        }
        for(std::size_t offset{2}; offset < length; ++ offset)
        {
            if(!is_continuation(static_cast<unsigned char>(bytes.at(index + offset))))
            {
                return false;
            }
        }
        index += length;
    }
    return true;
}

[[nodiscard]] std::string windows_1252_to_utf8(std::string_view bytes)
{
    std::string result;
    result.reserve(bytes.size() + bytes.size() / 8);
    for(const auto character : bytes)
    {
        const auto byte{static_cast<unsigned char>(character)};
        if(byte < 0x80)
        {
            result += character;
        }
        else if(byte < 0xA0)
        {
            result += encode_utf8(windows_1252_high.at(byte - 0x80));
        }
        else
        {
            result += encode_utf8(byte);
        }
    }
    return result;
}

[[nodiscard]] std::optional<std::string> utf8_to_windows_1252(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    for(std::size_t index{}; index < text.size();)
    {
        const auto codepoint{decode_utf8(text, index)};
        if(codepoint < 0x80 || (codepoint >= 0xA0 && codepoint <= 0xFF))
        {
            result += static_cast<char>(codepoint);
            continue;
        }
        auto found{false};
        for(std::size_t slot{}; slot < windows_1252_high.size(); ++ slot)
        {
            if(windows_1252_high.at(slot) == codepoint)
            {
                result += static_cast<char>(0x80 + slot);
                found = true;
                break;
            }
        }
        if(!found)
        {
            return std::nullopt;
        }
    }
    return result;
}

[[nodiscard]] std::string crlf_to_lf(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    for(std::size_t index{}; index < text.size(); ++ index)
    {
        if(text.at(index) == '\r' && index + 1 < text.size() && text.at(index + 1) == '\n')
        {
            continue;
        }
        result += text.at(index);
    }
    return result;
}

[[nodiscard]] std::string lf_to_crlf(std::string_view text)
{
    std::string result;
    result.reserve(text.size() + text.size() / 16);
    for(const auto character : text)
    {
        if(character == '\n')
        {
            result += '\r';
        }
        result += character;
    }
    return result;
}

} // namespace

DecodedText decode_text(std::string_view bytes)
{
    DecodedText result;
    if(bytes.substr(0, byte_order_mark.size()) == byte_order_mark)
    {
        bytes.remove_prefix(byte_order_mark.size());
        result.format.encoding = TextEncoding::utf8_bom;
        result.text = std::string{bytes};
    }
    else if(is_valid_utf8(bytes))
    {
        result.text = std::string{bytes};
    }
    else
    {
        result.format.encoding = TextEncoding::windows_1252;
        result.text = windows_1252_to_utf8(bytes);
    }
    std::size_t crlf{};
    std::size_t lf{};
    for(std::size_t index{}; index < result.text.size(); ++ index)
    {
        if(result.text.at(index) != '\n')
        {
            continue;
        }
        if(index > 0 && result.text.at(index - 1) == '\r')
        {
            ++ crlf;
        }
        else
        {
            ++ lf;
        }
    }
    if(crlf > lf)
    {
        result.format.line_ending = LineEnding::crlf;
        result.text = crlf_to_lf(result.text);
    }
    return result;
}

std::optional<std::string> encode_text(std::string_view text, const TextFormat& format)
{
    const auto lines{format.line_ending == LineEnding::crlf ? lf_to_crlf(text) : std::string{text}};
    switch(format.encoding)
    {
    case TextEncoding::utf8:
        return lines;
    case TextEncoding::utf8_bom:
        return std::string{byte_order_mark} + lines;
    case TextEncoding::windows_1252:
        return utf8_to_windows_1252(lines);
    }
    return std::nullopt;
}

std::string_view encoding_name(TextEncoding encoding)
{
    switch(encoding)
    {
    case TextEncoding::utf8:
        return "UTF-8";
    case TextEncoding::utf8_bom:
        return "UTF-8 BOM";
    case TextEncoding::windows_1252:
        return "Windows-1252";
    }
    return "?";
}

std::string_view line_ending_name(LineEnding line_ending)
{
    return line_ending == LineEnding::crlf ? "CRLF" : "LF";
}

std::optional<FileStamp> file_stamp(const std::filesystem::path& path)
{
    std::error_code error;
    const auto modified{std::filesystem::last_write_time(path, error)};
    if(error)
    {
        return std::nullopt;
    }
    const auto size{std::filesystem::file_size(path, error)};
    if(error)
    {
        return std::nullopt;
    }
    return FileStamp{modified, size};
}

std::string describe(const FileError& error)
{
    switch(error.kind)
    {
    case FileError::Kind::none:
        return {};
    case FileError::Kind::not_found:
        return translate("FileError", "the file does not exist");
    case FileError::Kind::is_a_folder:
        return translate("FileError", "it is a folder, not a file");
    case FileError::Kind::not_encodable:
        return format_text(translate("FileError", "the text has characters that {0} cannot store"),
                           {encoding_name(error.encoding)});
    case FileError::Kind::system:
        return error.system_message;
    }
    return error.system_message;
}

std::optional<TextFile> load_text_file(const std::filesystem::path& path, FileError& error)
{
    std::error_code status_error;
    if(!std::filesystem::exists(path, status_error))
    {
        error = status_error ? FileError{FileError::Kind::system, status_error.message(), {}}
                             : FileError{FileError::Kind::not_found, {}, {}};
        return std::nullopt;
    }
    if(std::filesystem::is_directory(path, status_error))
    {
        error = {FileError::Kind::is_a_folder, {}, {}};
        return std::nullopt;
    }
    const auto stamp{file_stamp(path)};
    const auto bytes{read_file(path)};
    if(!bytes || !stamp)
    {
        error = {FileError::Kind::system, std::error_code{errno, std::generic_category()}.message(), {}};
        return std::nullopt;
    }
    return TextFile{decode_text(*bytes), *stamp};
}

std::optional<FileStamp> save_text_file(const std::filesystem::path& path, std::string_view text,
                                        const TextFormat& format, FileError& error)
{
    const auto bytes{encode_text(text, format)};
    if(!bytes)
    {
        error = {FileError::Kind::not_encodable, {}, format.encoding};
        return std::nullopt;
    }
    std::string system_message;
    if(!replace_file_contents(path, *bytes, system_message))
    {
        error = {FileError::Kind::system, system_message, {}};
        return std::nullopt;
    }
    const auto stamp{file_stamp(path)};
    if(!stamp)
    {
        error = {FileError::Kind::system, "the file was written but cannot be read back", {}};
    }
    return stamp;
}

} // namespace olagarro::oui
