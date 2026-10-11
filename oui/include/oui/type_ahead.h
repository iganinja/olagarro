#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>

namespace olagarro::oui
{

// Finds a row from what the user types, as file managers do: characters typed in quick succession select
// the first row whose text starts with them, ignoring case, and one character typed again and again steps
// through the rows that start with it. A pause starts a new search. Lists keep one and ask it on each
// character typed.
class TypeAhead
{
  public:
    explicit TypeAhead(std::chrono::milliseconds pause = std::chrono::milliseconds{1000});

    // The row to select for `character` among `count` rows whose text `text_of` gives, with `current`
    // selected now; nothing when no row matches, and the selection should stay.
    [[nodiscard]] std::optional<std::size_t> find(char32_t character, std::size_t count, std::size_t current,
                                                  const std::function<std::string(std::size_t row)>& text_of);

  private:
    std::chrono::milliseconds m_pause;
    std::chrono::steady_clock::time_point m_last{};
    std::string m_typed;
};

} // namespace olagarro::oui
