#pragma once
#include <oui/command.h>
#include <oui/container.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
namespace olagarro::oui
{
class Menu final : public Widget
{
  public:
    explicit Menu(CommandRegistry& commands);
    void add_item(std::string_view command_id);
    [[nodiscard]] bool is_focusable() const override;
    bool handle_event(const Event& event) override;
    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void paint(Painter& painter, const Theme& theme) const override;

  private:
    struct Item
    {
        std::string command_id;
        std::string title;
    };
    CommandRegistry& m_commands;
    std::vector<Item> m_items;
    std::size_t m_selected{};
};
[[nodiscard]] std::unique_ptr<Menu> menu(CommandRegistry& commands);
Menu& menu(CommandRegistry& commands, Container& parent);
} // namespace olagarro::oui
