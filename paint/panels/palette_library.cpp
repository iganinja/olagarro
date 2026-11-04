#include <paint/panels/palette_library.h>

#include <oui/file_system.h>
#include <oui/settings.h>

#include <algorithm>
#include <cstdio>
#include <system_error>
#include <tuple>
#include <utility>

namespace olagarro::paint
{

namespace
{

constexpr std::string_view extension{".gpl"};

// The name as a file's, without what no system's file names take.
[[nodiscard]] std::string file_stem(std::string_view name)
{
    std::string stem;
    for(const auto character : name)
    {
        const auto forbidden{static_cast<unsigned char>(character) < 32 ||
                             std::string_view{"/\\:*?\"<>|"}.find(character) != std::string_view::npos};
        stem += forbidden ? '_' : character;
    }
    while(!stem.empty() && (stem.back() == '.' || stem.back() == ' '))
    {
        stem.pop_back();
    }
    return stem.empty() ? std::string{"palette"} : stem;
}

[[nodiscard]] bool write_file(const std::filesystem::path& path, const std::string& bytes)
{
    auto* const file{oui::open_file(path, "wb")};
    if(file == nullptr)
    {
        return false;
    }
    const auto written{std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size()};
    return std::fclose(file) == 0 && written;
}

} // namespace

std::filesystem::path PaletteLibrary::default_folder()
{
    return oui::user_settings_folder("olagarro") / "palettes";
}

PaletteLibrary::PaletteLibrary(std::filesystem::path folder) : m_folder{std::move(folder)}
{
}

std::vector<PaletteLibrary::Entry> PaletteLibrary::entries() const
{
    std::vector<Entry> found;
    std::error_code error;
    for(std::filesystem::directory_iterator files{m_folder, error}, end; !error && files != end; files.increment(error))
    {
        const auto& path{files->path()};
        if(oui::utf8_of(path.extension()) != extension || !files->is_regular_file(error))
        {
            continue;
        }
        const auto text{oui::read_file(path)};
        const auto palette{text ? palette_from_gpl(*text) : std::nullopt};
        if(!palette)
        {
            continue;
        }
        found.push_back({palette->name.empty() ? oui::utf8_of(path.stem()) : palette->name, path});
    }
    // By name; of two files with one name, the first by path counts.
    std::sort(found.begin(), found.end(), [](const Entry& one, const Entry& other)
              { return std::tie(one.name, one.path) < std::tie(other.name, other.path); });
    found.erase(std::unique(found.begin(), found.end(),
                            [](const Entry& one, const Entry& other) { return one.name == other.name; }),
                found.end());
    found.erase(
        std::remove_if(found.begin(), found.end(), [](const Entry& entry) { return entry.name == default_name; }),
        found.end());
    return found;
}

std::optional<PaletteLibrary::Entry> PaletteLibrary::find(std::string_view name) const
{
    for(auto& entry : entries())
    {
        if(entry.name == name)
        {
            return std::move(entry);
        }
    }
    return std::nullopt;
}

std::filesystem::path PaletteLibrary::new_path(std::string_view name) const
{
    const auto stem{file_stem(name)};
    for(auto count{1};; ++ count)
    {
        const auto path{m_folder /
                        oui::path_from_utf8(stem + (count == 1 ? std::string{} : " " + std::to_string(count)) +
                                            std::string{extension})};
        std::error_code error;
        if(!std::filesystem::exists(path, error))
        {
            return path;
        }
    }
}

std::vector<std::string> PaletteLibrary::names() const
{
    std::vector<std::string> names{std::string{default_name}};
    for(const auto& entry : entries())
    {
        names.push_back(entry.name);
    }
    return names;
}

bool PaletteLibrary::contains(std::string_view name) const
{
    return name == default_name || find(name).has_value();
}

std::optional<Palette> PaletteLibrary::load(std::string_view name) const
{
    if(name == default_name)
    {
        return Palette::standard();
    }
    const auto entry{find(name)};
    const auto text{entry ? oui::read_file(entry->path) : std::nullopt};
    auto palette{text ? palette_from_gpl(*text) : std::nullopt};
    if(palette)
    {
        palette->name = entry->name;
    }
    return palette;
}

bool PaletteLibrary::save(const Palette& palette)
{
    if(palette.name.empty() || palette.name == default_name)
    {
        return false;
    }
    std::error_code error;
    std::filesystem::create_directories(m_folder, error);
    const auto entry{find(palette.name)};
    return write_file(entry ? entry->path : new_path(palette.name), to_gpl(palette));
}

bool PaletteLibrary::rename(std::string_view from, std::string_view to)
{
    if(from == default_name || to == default_name || to.empty() || contains(to))
    {
        return false;
    }
    const auto entry{find(from)};
    auto palette{load(from)};
    if(!entry || !palette)
    {
        return false;
    }
    palette->name = std::string{to};
    if(!write_file(new_path(to), to_gpl(*palette)))
    {
        return false;
    }
    std::error_code error;
    return std::filesystem::remove(entry->path, error);
}

bool PaletteLibrary::remove(std::string_view name)
{
    const auto entry{name == default_name ? std::nullopt : find(name)};
    std::error_code error;
    return entry && std::filesystem::remove(entry->path, error);
}

} // namespace olagarro::paint
