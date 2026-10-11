#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace olagarro::oui
{

enum class TextEncoding
{
    utf8,
    utf8_bom,
    // What a file that is not valid UTF-8 is read as: the Latin-1 of Windows, which every byte decodes.
    windows_1252
};

enum class LineEnding
{
    lf,
    crlf
};

// How a text is stored on disk. In memory text is UTF-8 with '\n' line breaks; this says how to convert.
struct TextFormat
{
    TextEncoding encoding{TextEncoding::utf8};
    LineEnding line_ending{LineEnding::lf};
};

struct DecodedText
{
    std::string text;
    TextFormat format;
};

// Detects the encoding and the prevailing line ending of text read from a file. With CRLF prevailing every CRLF becomes
// '\n'; otherwise the text keeps its bytes, so an odd CRLF in an LF file survives a save untouched.
[[nodiscard]] DecodedText decode_text(std::string_view bytes);
// The bytes to store for a text, or nothing when it holds a character the encoding cannot hold.
[[nodiscard]] std::optional<std::string> encode_text(std::string_view text, const TextFormat& format);

[[nodiscard]] std::string_view encoding_name(TextEncoding encoding);
[[nodiscard]] std::string_view line_ending_name(LineEnding line_ending);

// Enough to notice that someone else changed a file since it was loaded or saved.
struct FileStamp
{
    std::filesystem::file_time_type modified{};
    std::uintmax_t size{};

    [[nodiscard]] bool operator==(const FileStamp& other) const
    {
        return modified == other.modified && size == other.size;
    }
    [[nodiscard]] bool operator!=(const FileStamp& other) const { return !(*this == other); }
};

struct TextFile
{
    DecodedText content;
    FileStamp stamp;
};

// Why loading or saving failed. The kind lets an application word the message its own way or react to the
// cause; system_message is what the operating system said, in its own language.
struct FileError
{
    enum class Kind
    {
        none,
        not_found,
        is_a_folder,
        // The text holds characters the file's encoding cannot store; `encoding` says which one.
        not_encodable,
        system
    };
    Kind kind{Kind::none};
    std::string system_message;
    TextEncoding encoding{TextEncoding::utf8};
};

// A sentence for the error, such as "the file does not exist", translated when a catalog says how.
[[nodiscard]] std::string describe(const FileError& error);

[[nodiscard]] std::optional<FileStamp> file_stamp(const std::filesystem::path& path);
// On failure these return nothing and fill `error`.
[[nodiscard]] std::optional<TextFile> load_text_file(const std::filesystem::path& path, FileError& error);
// Writes a temporary file beside the target and renames it over the target, so a failure at any point
// leaves the previous contents intact.
[[nodiscard]] std::optional<FileStamp> save_text_file(const std::filesystem::path& path, std::string_view text,
                                                      const TextFormat& format, FileError& error);

} // namespace olagarro::oui
