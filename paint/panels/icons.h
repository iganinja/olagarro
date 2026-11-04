#pragma once

#include <oui/color.h>
#include <oui/graphics/svg.h>

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

namespace olagarro::paint
{

// `svg`, a Godot editor icon drawn for a dark interface, in the colours the Godot editor gives it on a light one.
[[nodiscard]] std::string for_light_interface(std::string_view svg);

// The icons of Paint's tools, built in, by name ("Bucket"), and those a program adds of its own, drawn at the sizes
// asked and kept: for a light interface, or as Godot draws them, for a dark one.
class Icons
{
  public:
    // An icon of the program's own, an SVG drawn as Godot's are, 16 pixels a side for a dark interface; it takes the
    // place of a built-in one of the same name.
    void add(std::string name, std::string svg);
    // Whether the interface is dark, as a theme makes it; the icons drawn so far are drawn again.
    void set_dark_interface(bool dark);
    [[nodiscard]] bool dark_interface() const { return m_dark; }
    // `name` drawn as big as fits in `width` by `height` pixels, a whole multiple of its own size where that fits, so
    // it stays sharp, and in the middle; none for a name there is no icon of. Over `background`, when given, light
    // where it is dark and dark where it is light, whatever the interface, as on a list's selected row.
    [[nodiscard]] const oui::Picture* picture(std::string_view name, int width, int height,
                                              std::optional<oui::Color> background = std::nullopt);

  private:
    [[nodiscard]] std::optional<std::string_view> source_of(std::string_view name) const;

    std::map<std::string, std::string, std::less<>> m_added;
    std::map<std::tuple<std::string, int, int, bool>, oui::Picture> m_drawn;
    bool m_dark{false};
};

} // namespace olagarro::paint
