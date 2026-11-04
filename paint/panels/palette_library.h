#pragma once

#include <paint/palette.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace olagarro::paint
{

// The palettes kept by name as the .gpl files of a folder, the same folder for every program on Olagarro, so a palette
// saved in one is there in the others. "Default", `Palette::standard()`, is always there and is no file: it cannot be
// replaced, renamed or removed. A file is named after its palette, as much as a file name can be, and the palette's
// name is the one written in it.
class PaletteLibrary
{
  public:
    static constexpr std::string_view default_name{"Default"};

    // `<the user's configuration folder>/olagarro/palettes`.
    [[nodiscard]] static std::filesystem::path default_folder();

    explicit PaletteLibrary(std::filesystem::path folder = default_folder());

    [[nodiscard]] const std::filesystem::path& folder() const { return m_folder; }
    // "Default" first, then the folder's palettes by name.
    [[nodiscard]] std::vector<std::string> names() const;
    [[nodiscard]] bool contains(std::string_view name) const;
    // None when no palette has the name or its file cannot be read.
    [[nodiscard]] std::optional<Palette> load(std::string_view name) const;
    // Writes the palette under its name, replacing the one there, the folder made when missing. False for "Default", an
    // empty name, or when the file cannot be written.
    bool save(const Palette& palette);
    // False when `from` is not there, `to` is, or either is "Default".
    bool rename(std::string_view from, std::string_view to);
    bool remove(std::string_view name);

  private:
    struct Entry
    {
        std::string name;
        std::filesystem::path path;
    };

    std::filesystem::path m_folder;

    // The folder's palettes, by name.
    [[nodiscard]] std::vector<Entry> entries() const;
    [[nodiscard]] std::optional<Entry> find(std::string_view name) const;
    // A file for a palette of that name not used by another one.
    [[nodiscard]] std::filesystem::path new_path(std::string_view name) const;
};

} // namespace olagarro::paint
