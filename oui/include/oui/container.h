#pragma once

#include <oui/widget.h>

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace olagarro::oui
{

class Container : public Widget
{
  public:
    template <typename Child> Child& add(std::unique_ptr<Child>&& child)
    {
        static_assert(std::is_base_of_v<Widget, Child>, "A container can only adopt widgets.");

        Child& result{*child};
        result.set_parent(this);
        m_children.push_back(std::move(child));
        return result;
    }

    [[nodiscard]] std::size_t child_count() const;

    [[nodiscard]] std::unique_ptr<Widget> remove_at(std::size_t index);
    // Takes `child` out and hands it back; a widget that is not a child is an error, std::logic_error.
    [[nodiscard]] std::unique_ptr<Widget> remove(const Widget& child);
    // Puts `replacement` where `child` is and hands `child` back: to nest something in its place, or to take it
    // out. A widget that is not a child is an error, std::logic_error.
    [[nodiscard]] std::unique_ptr<Widget> replace(const Widget& child, std::unique_ptr<Widget> replacement);

    [[nodiscard]] Widget& child_at(std::size_t index);

    [[nodiscard]] const Widget& child_at(std::size_t index) const;

    void paint(Painter& painter, const Theme& theme) const override;

    void collect_focusable(std::vector<Widget*>& widgets) override;

    [[nodiscard]] Widget* find_by_id(std::string_view id) override;

    [[nodiscard]] bool contains(const Widget* target) const override;

    [[nodiscard]] Widget* hit_test(int x, int y) override;

    void dismiss_overlays_at(int x, int y) override;

    [[nodiscard]] Widget* keyboard_target() override;

  protected:
    [[nodiscard]] std::size_t index_of(const Widget& child) const;
    [[nodiscard]] const std::vector<std::unique_ptr<Widget>>& children() const;
    std::vector<std::unique_ptr<Widget>>& children_mutable();

    void replace_child(std::size_t index, std::unique_ptr<Widget> child)
    {
        m_children.at(index) = std::move(child);
        m_children.at(index)->set_parent(this);
    }

    std::vector<std::unique_ptr<Widget>> m_children;
};

} // namespace olagarro::oui
