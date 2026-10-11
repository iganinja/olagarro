#pragma once

#include <oui/theme.h>

#include <oui/container.h>

#include <signalslot/signalslot.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace olagarro::oui
{

enum class DialogResult
{
    accepted,
    cancelled
};

enum class WindowBorder
{
    none,
    single,
    double_
};

// Where the user left a window: the position only if they moved it, the size only if they resized it.
struct WindowPlacement
{
    std::optional<Point> position;
    std::optional<Size> size;
};

class Window final : public Container
{
  public:
    explicit Window(std::string_view title);

    [[nodiscard]] std::string_view title() const;
    void set_title(std::string title);
    [[nodiscard]] bool is_closed() const;
    void set_border(WindowBorder border);
    [[nodiscard]] WindowBorder border() const;
    void set_background(ColorRef background);
    // Caps the size the window picks for itself; a resizable window can still be dragged larger.
    void set_maximum_size(Size size);
    // The size the window opens at even when its content measures smaller, capped by the maximum size and
    // the screen. Dialogs built from flexible columns use it, since those measure as zero.
    void set_preferred_size(Size size);
    [[nodiscard]] Size preferred_size() const;
    // Opens at this share of the height the window may take, the screen for a dialog, whatever its content
    // measures: a long list scrolls inside it. Follows the screen as it resizes, until the user resizes the
    // window, and is still bound by the minimum and maximum sizes.
    void set_height_share(double share);
    // The smallest the window gets, by measure or by resizing.
    void set_minimum_size(Size size);
    // Lets the user resize the window by dragging its right edge, bottom edge, or bottom-right corner.
    void set_resizable(bool resizable);
    // Names the window so the application can remember its placement between showings.
    void set_placement_key(std::string key);
    [[nodiscard]] const std::string& placement_key() const;
    [[nodiscard]] WindowPlacement placement() const;
    // Applies a remembered placement on the next arrange, kept within the available area; a size only
    // applies to a resizable window.
    void restore_placement(const WindowPlacement& placement);
    // Placing from the keyboard, as the application starts it with Ctrl+L: the arrows move the window and,
    // when it is resizable, Ctrl and the arrows resize it; Enter keeps it there and Escape puts it back where
    // it was. The frame shows it is being placed. Returns whether placing goes on after `key`; keys that are
    // not the placement's are swallowed while it lasts.
    void start_placing();
    [[nodiscard]] bool place(const KeyEvent& key);
    [[nodiscard]] bool is_placing() const;
    [[nodiscard]] bool is_resizable() const;
    void close(DialogResult result);
    // Open again once closed, to be shown once more.
    void reopen();

    template <typename Callback> olagarro::signal_slot::Connection on_closed(Callback&& callback)
    {
        return m_closed.connect(std::forward<Callback>(callback));
    }

    void set_close_on_escape(bool close);
    [[nodiscard]] bool closes_on_escape() const;
    void set_close_on_enter(bool close);
    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void arrange(const Rect& bounds) override;
    void paint(Painter& painter, const Theme& theme) const override;
    bool handle_event(const Event& event) override;

  private:
    [[nodiscard]] bool has_close_button() const;
    void resize_to(Size requested);

    // What placing from the keyboard puts back on Escape.
    struct Placing
    {
        int offset_x;
        int offset_y;
        std::optional<Size> user_size;
        bool moved;
    };
    std::optional<Placing> m_placing;

    std::string m_title;
    bool m_is_closed{false};
    WindowBorder m_border{WindowBorder::double_};
    std::optional<ColorRef> m_background;
    // No cap by default: the screen limits a window, and set_maximum_size narrows it where wanted.
    Size m_maximum_size{1 << 15, 1 << 15};
    Size m_preferred_size{};
    std::optional<double> m_height_share;
    Size m_minimum_size{};
    bool m_resizable{false};
    bool m_close_pressed{false};
    // Set once the user resizes; from then on it replaces the measured size.
    std::optional<Size> m_user_size;
    std::string m_placement_key;
    bool m_moved{false};
    std::optional<Point> m_restored_position;
    bool m_resizing_width{false};
    bool m_resizing_height{false};
    Size m_resize_start_size{};
    Rect m_area{};
    int m_drag_offset_x{};
    int m_drag_offset_y{};
    int m_drag_start_x{};
    int m_drag_start_y{};
    int m_drag_origin_x{};
    int m_drag_origin_y{};
    bool m_dragging{false};
    bool m_close_on_escape{true};
    bool m_close_on_enter{false};
    olagarro::signal_slot::Signal<DialogResult> m_closed;
};

[[nodiscard]] std::unique_ptr<Window> window(std::string_view title);

} // namespace olagarro::oui
