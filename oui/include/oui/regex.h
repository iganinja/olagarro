#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace olagarro::oui
{

// UTF-8 text in UTF-16, as JavaScript sees strings, with the byte offset of each code unit, to match a
// Regex and read its matches back as bytes.
struct Utf16Text
{
    explicit Utf16Text(std::string_view text);

    std::u16string units;
    // One per unit, and one more for the end of the text.
    std::vector<std::size_t> byte_offsets;
};

// A JavaScript regular expression, as RegExp reads it, run from C++ by QuickJS's own engine, so a pattern
// means here exactly what it means in a script. Compiling and searching may happen on any thread, each with
// memory of its own.
class Regex
{
  public:
    // `flags` as RegExp takes them: i, m, s, u, v, y. Throws std::invalid_argument, with the engine's
    // message, for a pattern that does not compile.
    explicit Regex(std::string_view pattern, std::string_view flags = {});

    // The first match at or after unit `from`, as [start, end) in code units of `text`.
    [[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>> search(const std::u16string& text,
                                                                            std::size_t from) const;

  private:
    std::vector<std::uint8_t> m_bytecode;
};

} // namespace olagarro::oui
