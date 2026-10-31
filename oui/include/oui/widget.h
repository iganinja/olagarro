#pragma once

#include <oui/geometry.h>
#include <oui/event.h>
#include <oui/painter.h>
#include <oui/theme.h>

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace olagarro::oui
{

class Container;
class Application;

template <typename Type> using OptionalRef = std::optional<std::reference_wrapper<Type>>;

class Widget
{
  public:
    virtual ~Widget() = default;

    Widget(const Widget&) = delete;
    Widget& operator=(const Widget&) = delete;
    Widget(Widget&&) = delete;
    Widget& operator=(Widget&&) = delete;

    [[nodiscard]] virtual Size measure(const Constraints& constraints) const = 0;
    virtual void arrange(const Rect& bounds);

    virtual void paint(Painter&, const Theme&) const;

    [[nodiscard]] virtual bool is_focusable() const;

    virtual bool handle_event(const Event&);
    // Whether this widget, focused, takes `key`, a KeyEvent or a TextEvent, before the application's shortcuts: keys
    // that are what the widget is for, as a file panel's F5 copies or a terminal's Ctrl+W deletes a word, whatever
    // the application binds the same key to elsewhere.
    [[nodiscard]] virtual bool claims_key(const Event& key) const;
    // What a pointer resting at `point` is told about the widget there, if anything: the text set_tooltip
    // gave unless a widget says something about each part of itself, as a row of tabs about each tab.
    [[nodiscard]] virtual std::optional<std::string> tooltip_at(Point point) const;
    // What the pointer looks like over `point`, or anywhere while this widget is dragging something with it.
    [[nodiscard]] virtual Pointer pointer_at(Point point) const;
    // Whether it hears the pointer as it is, as a canvas drawn on does: every pixel it moves and not only every cell,
    // the middle button, and drags with the right and the middle buttons. Other widgets hear none of that.
    [[nodiscard]] virtual bool takes_raw_pointer() const;
    void set_tooltip(std::string text);

    virtual void collect_focusable(std::vector<Widget*>& widgets);

    [[nodiscard]] bool is_focused() const;
    [[nodiscard]] bool is_visible() const;
    void set_visible(bool is_visible);
    // Whether it answers: a disabled widget takes no click, no key, no letter with Alt and no focus, and shows so;
    // one in a disabled container is disabled with it.
    [[nodiscard]] bool is_enabled() const;
    void set_enabled(bool enabled);
    // What hears a key, or text typed, before the widget does, while it or what it holds has the focus: true keeps
    // it from going further. A program gives a widget keys of its own so, without making another kind of it.
    void set_key_handler(std::function<bool(const Event&)> handler);
    // Whether its key handler kept `event`.
    [[nodiscard]] bool key_handler_keeps(const Event& event) const;
    // What hears the mouse before the widget does, when the widget is under the pointer or holds it from a press:
    // true keeps the event from going further. A program gives a widget a drag of its own so, as a number field gives
    // its text field.
    void set_pointer_handler(std::function<bool(const MouseEvent&)> handler);
    // Whether its pointer handler kept `event`.
    [[nodiscard]] bool pointer_handler_keeps(const Event& event) const;
    // What hears the widget take the focus, with true, and lose it, with false, however that happens: Tab, a click
    // elsewhere, a dialog opening. A program ends there what was typed in a field, as one undo step for all of it.
    void set_focus_handler(std::function<void(bool)> handler);

    void set_id(std::string id);

    [[nodiscard]] std::string_view id() const;

    [[nodiscard]] virtual Widget* find_by_id(std::string_view id);

    [[nodiscard]] virtual bool contains(const Widget* target) const;

    [[nodiscard]] virtual Widget* hit_test(double x, double y);

    [[nodiscard]] virtual bool is_selected_child(const Widget&) const;

    [[nodiscard]] OptionalRef<Widget> find(std::string_view id)
    {
        if(auto* const result{find_by_id(id)})
        {
            return *result;
        }
        return std::nullopt;
    }

    template <typename Type> [[nodiscard]] OptionalRef<Type> find(std::string_view id)
    {
        static_assert(std::is_base_of_v<Widget, Type>, "find<T> requires a widget type.");
        if(auto* const result{find_by_id(id)})
        {
            if(auto* const typed_result{dynamic_cast<Type*>(result)})
            {
                return *typed_result;
            }
        }
        return std::nullopt;
    }

    template <typename Type> Type& get(std::string_view id)
    {
        if(const auto result{find<Type>(id)})
        {
            return result->get();
        }
        throw std::runtime_error{"The requested widget id is missing or has a different type."};
    }

    [[nodiscard]] const Rect& bounds() const;
    [[nodiscard]] virtual Rect paint_bounds() const;
    [[nodiscard]] virtual bool is_overlay() const;
    // Expires when the widget is destroyed: to hold on to it without holding it up, and to tell it from another made
    // later in the same place.
    [[nodiscard]] std::weak_ptr<void> lifetime() const;
    virtual void dismiss_overlays_at(double, double);

    [[nodiscard]] virtual Widget* keyboard_target();

    // What is typed while this widget has the focus, characters and Backspace, goes to `target` instead,
    // and the focus with it: a list under a filter field sends typing to the filter, and keeps the
    // arrows, Page Up/Down and Enter for itself. The target must outlive this widget's use of it; once it
    // is gone, typing stays here.
    void delegate_typing_to(Widget& target);

    [[nodiscard]] const Container* parent() const;

    [[nodiscard]] Container* parent();

  protected:
    Widget() = default;

  private:
    friend class Container;
    friend class Application;

    void set_parent(Container* parent);

  protected:
    void set_parent_for_subclass(Container* parent) { m_parent = parent; }

    void set_focused(bool is_focused);

    Rect m_bounds{};
    Container* m_parent{nullptr};
    bool m_is_focused{false};
    bool m_is_visible{true};
    bool m_is_enabled{true};
    std::function<bool(const Event&)> m_key_handler;
    std::function<bool(const MouseEvent&)> m_pointer_handler;
    std::function<void(bool)> m_focus_handler;
    std::string m_id;
    std::string m_tooltip;
    std::shared_ptr<void> m_lifetime{std::make_shared<int>(0)};
    Widget* m_typing_delegate{nullptr};
    std::weak_ptr<void> m_typing_delegate_lifetime;
};

// A widget held on to without being kept: get() gives nothing once it is gone, even when another is made where it was,
// as a document of a closed tab and the next one opened. For whatever must outlive the event that found the widget.
template <typename Kind> class WidgetRef
{
  public:
    WidgetRef() = default;
    WidgetRef(Kind* widget)
        : m_widget{widget}, m_lifetime{widget != nullptr ? widget->lifetime() : std::weak_ptr<void>{}}
    {
    }

    [[nodiscard]] Kind* get() const { return m_lifetime.expired() ? nullptr : m_widget; }
    Kind* operator->() const { return get(); }
    explicit operator bool() const { return get() != nullptr; }
    // Whether it is `widget`, still there; never a widget made later where it was.
    [[nodiscard]] bool refers_to(const Kind* widget) const { return widget != nullptr && get() == widget; }

  private:
    Kind* m_widget{};
    std::weak_ptr<void> m_lifetime;
};

} // namespace olagarro::oui
