#pragma once

#include <paint/panels/icons.h>
#include <paint/tool.h>

#include <oui/command.h>
#include <oui/widget.h>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace olagarro::paint
{

// A button of the tool panel: the tool's id, as `tool_id` gives Paint's, the icon it shows, its title and Paint.NET's
// key for it, which pressed again goes to the next tool with the same key.
struct ToolButton
{
    std::string id;
    std::string icon;
    std::string title;
    char32_t key;
};

// Paint's tools as OUI Paint's Tools panel has them, in its order, with their icons and Paint.NET's keys: B brush,
// P pencil, E eraser, F fill, G gradient, O line, shapes and arc, S the selections, K the picker, M the two moves.
[[nodiscard]] std::vector<ToolButton> paint_tool_buttons();
// The tool `key` picks among `buttons` when `current` is in use: the first with that key, or the one after `current`
// among them, round again; none for a key no tool has.
[[nodiscard]] std::optional<std::string> tool_for_key(const std::vector<ToolButton>& buttons, char32_t key,
                                                      std::string_view current);

// The tools as icon buttons in a grid, as Paint.NET's Tools window: one of them is the tool in use, and clicking
// another, or pressing its key, picks it. Paint's tools unless a program gives its own list, Paint's with its own tools
// among them.
class ToolPanel final : public oui::Widget
{
  public:
    // Each button is this many cells wide and one row tall.
    static constexpr int button_width{2};
    static constexpr int columns{2};

    explicit ToolPanel(Icons& icons);
    ToolPanel(std::vector<ToolButton> buttons, Icons& icons);

    [[nodiscard]] const std::vector<ToolButton>& buttons() const { return m_buttons; }
    void set_selected(std::string_view id);
    void set_selected(Tool tool);
    [[nodiscard]] const std::string& selected() const;
    // The tool selected, none when it is one of the program's own.
    [[nodiscard]] std::optional<Tool> selected_tool() const;
    // Picks the tool `key` gives, as a click on its button does; false for a key no tool has.
    bool press_key(char32_t key);
    // Called with the id of a button clicked or a tool picked by its key, after it became the selected one.
    void on_picked(std::function<void(const std::string&)> picked);

    // The button at a cell inside the panel, counted from its top left corner; none between or past the buttons.
    [[nodiscard]] std::optional<std::size_t> button_at(oui::Point cell) const;

    [[nodiscard]] oui::Size measure(const oui::Constraints& constraints) const override;
    void paint(oui::Painter& painter, const oui::Theme& theme) const override;
    bool handle_event(const oui::Event& event) override;
    [[nodiscard]] std::optional<std::string> tooltip_at(oui::Point point) const override;

  private:
    [[nodiscard]] oui::Rect button_bounds(std::size_t index) const;
    void pick(std::string id);

    std::vector<ToolButton> m_buttons;
    Icons& m_icons;
    std::string m_selected;
    std::function<void(const std::string&)> m_picked;
};

// Each tool's key as a command ("tool.key_b"), bound to it in `commands`, which picks the tool on `panel`, and again
// the next with the same key, as in Paint.NET. The panel outlives the commands.
void add_tool_commands(oui::CommandRegistry& commands, ToolPanel& panel);

} // namespace olagarro::paint
