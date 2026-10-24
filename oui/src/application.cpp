#include <oui/application.h>
#include <oui/button.h>
#include <oui/check_button.h>
#include <oui/label.h>
#include <oui/line_edit.h>
#include <oui/text_edit.h>
#include <oui/text_metrics.h>
#include <oui/terminal_backend.h>
#include <oui/text_layout.h>
#include <oui/translation.h>

#include <algorithm>
#include <cstddef>
#include <tuple>
#include <utility>

// What the C++ runtime calls to name the type of an exception nobody caught, as it ends the program. Defined here,
// in what every OUI program links, it keeps the runtime's own demangler out of them: 56 KB with libstdc++, 128 KB
// with libc++abi. The runtime then names the type as the compiler mangled it.
extern "C" char* __cxa_demangle(const char*, char*, std::size_t*, int* status)
{
    if(status != nullptr)
    {
        // "Not a valid name under the C++ ABI's rules", which the runtime answers with the name as it is.
        *status = -2;
    }
    return nullptr;
}

namespace olagarro::oui
{

namespace
{

// A character or Backspace, what Widget::delegate_typing_to hands on; Ctrl and Alt combinations are
// commands, not typing.
[[nodiscard]] bool is_typing(const Event& event)
{
    if(const auto* const text{std::get_if<TextEvent>(&event)})
    {
        return !text->control && !text->alt && text->codepoint >= U' ';
    }
    const auto* const key{std::get_if<KeyEvent>(&event)};
    return key != nullptr && key->key == Key::backspace && !key->control && !key->alt;
}

} // namespace

ModalLayer::ModalLayer(std::unique_ptr<Widget>&& background, std::unique_ptr<Window>&& dialog)
{
    m_dialog = &add(std::move(dialog));
    add(std::move(background));
}

Size ModalLayer::measure(const Constraints& constraints) const
{
    return constraints.constrain({1, 1});
}
void ModalLayer::arrange(const Rect& bounds)
{
    Widget::arrange(bounds);
    child_at(1).arrange(bounds);
    child_at(0).arrange(bounds);
}
void ModalLayer::paint(Painter& painter, const Theme& theme) const
{
    child_at(1).paint(painter, theme);
    child_at(0).paint(painter, theme);
}
bool ModalLayer::handle_event(const Event& event)
{
    return m_dialog->handle_event(event);
}
Widget* ModalLayer::hit_test(int x, int y)
{
    if(auto* const target{m_dialog->hit_test(x, y)})
    {
        return target;
    }
    return m_dialog;
}
bool ModalLayer::is_overlay() const
{
    return true;
}
Widget* ModalLayer::keyboard_target()
{
    return m_dialog->keyboard_target();
}
void ModalLayer::collect_focusable(std::vector<Widget*>& widgets)
{
    m_dialog->collect_focusable(widgets);
}
Window& ModalLayer::dialog()
{
    return *m_dialog;
}
std::unique_ptr<Widget> ModalLayer::release_background()
{
    return remove_at(1);
}
std::unique_ptr<Window> ModalLayer::release_dialog()
{
    auto dialog{remove_at(0)};
    m_dialog = nullptr;
    return std::unique_ptr<Window>{static_cast<Window*>(dialog.release())};
}

Application::Application() : Application{terminal_backend()}
{
}
Application::Application(std::unique_ptr<Backend> backend, Theme theme)
    : m_backend{std::move(backend)}, m_theme{std::move(theme)}
{
    offer_zoom();
}
Application::~Application()
{
    // What shows goes before the queue, the timers and the rest it may still reach as it goes.
    m_focused = nullptr;
    m_mouse_capture = nullptr;
    m_popup.reset();
    m_closed_popups.clear();
    m_content.reset();
}

void Application::offer_zoom()
{
    if(!m_backend || !m_backend->zooms() || m_commands.title("zoom.in"))
    {
        return;
    }
    // Ctrl and plus or minus, as in a browser, = being + without Shift.
    m_commands.register_command("zoom.in", translate("Application", "Zoom &In"), [this] { zoom(1); });
    m_commands.register_command("zoom.out", translate("Application", "Zoom &Out"), [this] { zoom(-1); });
    m_commands.register_command("zoom.reset", translate("Application", "&Normal Size"), [this] { zoom(0); });
    m_commands.bind("zoom.in", {U'+', true});
    m_commands.bind("zoom.in", {U'=', true});
    m_commands.bind("zoom.out", {U'-', true});
    m_commands.bind("zoom.reset", {U'0', true});
}

bool Application::zoom(int step)
{
    if(!m_backend || !(step == 0 ? m_backend->zoom_reset() : m_backend->zoom_by(step)))
    {
        return false;
    }
    m_needs_render = true;
    if(m_zoomed)
    {
        m_zoomed();
    }
    return true;
}
void Application::set_content(std::unique_ptr<Widget>&& content)
{
    if(content != nullptr)
    {
        content->set_parent(nullptr);
    }
    m_content = std::move(content);
    m_focused = nullptr;
    m_focused_lifetime.reset();
    drop_mouse_capture();
    focus_next();
    if(m_backend)
    {
        m_backend->reset();
    }
    m_needs_render = true;
}
void Application::set_window(std::unique_ptr<Window>&& window)
{
    auto* const reference{window.get()};
    set_content(std::move(window));
    reference->on_closed([this](DialogResult) { quit(); });
}
void Application::set_placement_shortcut(Shortcut shortcut)
{
    m_placement_shortcut = shortcut;
}
void Application::start_key_mode(std::string hint, std::function<KeyModeAnswer(const Event& key)> handler)
{
    m_key_mode = KeyMode{std::move(hint), std::move(handler)};
    ++ m_key_mode_count;
    m_needs_render = true;
}
bool Application::in_key_mode() const
{
    return m_key_mode.has_value();
}
void Application::set_key_mode_hint(std::string hint)
{
    if(m_key_mode)
    {
        m_key_mode->hint = std::move(hint);
        m_needs_render = true;
    }
}
bool Application::handle_key_mode(const Event& event)
{
    const auto is_key{std::holds_alternative<KeyEvent>(event) || std::holds_alternative<TextEvent>(event)};
    if(m_key_mode)
    {
        m_needs_render = true;
        if(!is_key)
        {
            return std::holds_alternative<MouseEvent>(event);
        }
        const auto mode{m_key_mode_count};
        const auto handler{m_key_mode->handler};
        const auto answer{handler(event)};
        if(answer != KeyModeAnswer::keep && mode == m_key_mode_count)
        {
            m_key_mode.reset();
        }
        return answer != KeyModeAnswer::end_and_pass;
    }
    if(m_modal_depth == 0 || m_content == nullptr || !is_key)
    {
        return false;
    }
    const auto& shortcut{m_placement_shortcut};
    const auto* const text{std::get_if<TextEvent>(&event)};
    const auto* const key{std::get_if<KeyEvent>(&event)};
    const auto matches{(text != nullptr && !shortcut.key && text->codepoint == shortcut.character &&
                        text->control == shortcut.control && text->alt == shortcut.alt &&
                        text->shift == shortcut.shift) ||
                       (key != nullptr && shortcut.key && key->key == *shortcut.key &&
                        key->control == shortcut.control && key->alt == shortcut.alt && key->shift == shortcut.shift)};
    if(!matches)
    {
        return false;
    }
    start_placing();
    return true;
}
// Placing the dialog on top from the keyboard is a key mode of its own.
void Application::start_placing()
{
    auto* const window{&static_cast<ModalLayer*>(m_content.get())->dialog()};
    window->start_placing();
    start_key_mode(window->is_resizable() ? translate("Window", "Move: ←↑→↓ · Resize: Ctrl+←↑→↓ "
                                                                "· Keep: Enter · Put back: Esc")
                                          : translate("Window", "Move: ←↑→↓ · Keep: Enter · Put back: Esc"),
                   [this, window](const Event& event)
                   {
                       // The dialog may have closed under it, by a posted callback or a timer.
                       if(m_content == nullptr || !m_content->contains(window))
                       {
                           return KeyModeAnswer::end;
                       }
                       const auto* const key{std::get_if<KeyEvent>(&event)};
                       return key == nullptr || window->place(*key) ? KeyModeAnswer::keep : KeyModeAnswer::end;
                   });
}
void Application::set_window_placement_store(PlacementLoader load, PlacementSaver save)
{
    m_load_placement = std::move(load);
    m_save_placement = std::move(save);
}
DialogResult Application::show_modal(std::unique_ptr<Window>&& window)
{
    auto result{DialogResult::cancelled};
    static_cast<void>(run_modal(std::move(window), result));
    return result;
}
DialogResult Application::show_modal_and_keep(std::unique_ptr<Window>& window)
{
    auto result{DialogResult::cancelled};
    window = run_modal(std::move(window), result);
    return result;
}
std::unique_ptr<Window> Application::run_modal(std::unique_ptr<Window>&& window, DialogResult& shown_result)
{
    std::optional<DialogResult> result;
    auto* const reference{window.get()};
    reference->reopen();
    const auto key{reference->placement_key()};
    if(!key.empty() && m_load_placement)
    {
        if(const auto placement{m_load_placement(key)})
        {
            reference->restore_placement(*placement);
        }
    }
    // Let go of as it closes: a window kept may close again, long after `result` is gone.
    const auto closed{reference->on_closed([&result](DialogResult value) { result = value; })};
    auto* const previous_focus{m_focused};
    const auto previous_focus_lifetime{m_focused_lifetime};
    ++ m_modal_depth;
    set_content(std::make_unique<ModalLayer>(std::move(m_content), std::move(window)));
    while(!result.has_value() && !m_shutting_down)
    {
        run_frame();
    }
    if(!key.empty() && m_save_placement)
    {
        m_save_placement(key, reference->placement());
    }
    closed.disconnect();
    auto* const layer{static_cast<ModalLayer*>(m_content.get())};
    auto background{layer->release_background()};
    auto given{layer->release_dialog()};
    // A window kept leaves with none of its widgets focused, to open again as it opened first.
    if(m_focused != nullptr && !m_focused_lifetime.expired() && given->contains(m_focused))
    {
        m_focused->set_focused(false);
        m_focused = nullptr;
    }
    set_content(std::move(background));
    -- m_modal_depth;
    if(previous_focus != nullptr && !previous_focus_lifetime.expired() && m_content->contains(previous_focus))
    {
        set_focus(previous_focus);
    }
    shown_result = result.value_or(DialogResult::cancelled);
    return given;
}
void Application::show_popup(std::unique_ptr<Widget> popup, std::function<void()> closed)
{
    close_popup();
    m_popup = std::move(popup);
    m_popup_closed = std::move(closed);
    m_needs_render = true;
}
void Application::close_popup()
{
    if(m_popup == nullptr)
    {
        return;
    }
    m_closed_popups.push_back(std::move(m_popup));
    m_needs_render = true;
    if(auto closed{std::exchange(m_popup_closed, {})})
    {
        closed();
    }
}
Widget* Application::popup() const
{
    return m_popup.get();
}
// The popup's turn at an event: keys it handles, and the mouse over it; a press elsewhere closes it and goes on.
bool Application::dispatch_to_popup(const Event& event)
{
    if(m_popup == nullptr)
    {
        return false;
    }
    // Where it is now, before it has been drawn too.
    const auto screen{m_backend->size()};
    m_popup->arrange({0, 0, screen.width, screen.height});
    if(const auto* const mouse{std::get_if<MouseEvent>(&event)})
    {
        const auto& area{m_popup->bounds()};
        if(mouse->x >= area.x && mouse->x < area.x + area.width && mouse->y >= area.y &&
           mouse->y < area.y + area.height)
        {
            m_popup->handle_event(event);
            m_needs_render = true;
            return true;
        }
        if(mouse->pressed)
        {
            close_popup();
        }
        return false;
    }
    if(std::holds_alternative<KeyEvent>(event) || std::holds_alternative<TextEvent>(event))
    {
        if(m_popup->handle_event(event))
        {
            m_needs_render = true;
            return true;
        }
    }
    return false;
}
namespace
{

// A letter as a mnemonic matches it, whatever the case of an ASCII letter.
[[nodiscard]] char32_t folded(char32_t letter)
{
    return letter >= U'A' && letter <= U'Z' ? letter - U'A' + U'a' : letter;
}

// The button or label under `widget` whose mnemonic is `letter`, in the order they show.
Widget* mnemonic_owner(Widget& widget, char32_t letter)
{
    if(!widget.is_visible() || !widget.is_enabled())
    {
        return nullptr;
    }
    if(const auto* const button{dynamic_cast<const Button*>(&widget)})
    {
        const auto own{button->mnemonic()};
        return own && folded(*own) == letter ? &widget : nullptr;
    }
    if(const auto* const label{dynamic_cast<const Label*>(&widget)})
    {
        const auto own{label->mnemonic()};
        return own && folded(*own) == letter ? &widget : nullptr;
    }
    if(const auto* const check{dynamic_cast<const CheckButton*>(&widget)})
    {
        const auto own{check->mnemonic()};
        return own && folded(*own) == letter ? &widget : nullptr;
    }
    if(auto* const container{dynamic_cast<Container*>(&widget)})
    {
        for(std::size_t index{}; index < container->child_count(); ++ index)
        {
            if(auto* const found{mnemonic_owner(container->child_at(index), letter)})
            {
                return found;
            }
        }
    }
    return nullptr;
}

} // namespace

Widget* Application::dialog_shown()
{
    return m_modal_depth == 0 || m_content == nullptr ? nullptr : &static_cast<ModalLayer*>(m_content.get())->dialog();
}

bool Application::activate_mnemonic(Widget& root, const Event& event, bool plain)
{
    const auto* const text{std::get_if<TextEvent>(&event)};
    if(text == nullptr || text->alt == plain || text->control)
    {
        return false;
    }
    auto* const owner{mnemonic_owner(root, folded(text->codepoint))};
    if(auto* const button{dynamic_cast<Button*>(owner)})
    {
        set_focus(button);
        button->click();
        return true;
    }
    if(auto* const check{dynamic_cast<CheckButton*>(owner)})
    {
        set_focus(check);
        check->toggle();
        return true;
    }
    if(auto* const label{dynamic_cast<Label*>(owner)})
    {
        if(auto* const buddy{label->buddy()}; buddy != nullptr && buddy->is_focusable())
        {
            set_focus(buddy);
            return true;
        }
    }
    return false;
}
void Application::push_overlay(std::unique_ptr<Window>&& window)
{
    close_popup();
    auto* const reference{window.get()};
    reference->on_closed([this](DialogResult) { post([this] { pop_overlay(); }); });
    m_previous_content = std::move(m_content);
    m_previous_focus = m_focused;
    m_previous_focus_lifetime = m_focused_lifetime;
    ++ m_modal_depth;
    set_content(std::make_unique<ModalLayer>(std::move(m_previous_content), std::move(window)));
}
void Application::pop_overlay()
{
    if(m_modal_depth == 0)
    {
        return;
    }
    auto* const layer{static_cast<ModalLayer*>(m_content.get())};
    set_content(layer->release_background());
    -- m_modal_depth;
    if(m_previous_focus != nullptr && !m_previous_focus_lifetime.expired() && m_content->contains(m_previous_focus))
    {
        set_focus(m_previous_focus);
    }
    m_previous_focus = nullptr;
    m_previous_focus_lifetime = {};
}
void Application::set_theme(Theme theme)
{
    m_theme = std::move(theme);
    m_needs_render = true;
}
void Application::reset_focus()
{
    if(m_focused != nullptr && m_content != nullptr && m_content->contains(m_focused))
    {
        m_focused->set_focused(false);
    }
    m_focused = nullptr;
    focus_next();
    m_needs_render = true;
}
void Application::focus(Widget& widget)
{
    set_focus(&widget);
    m_needs_render = true;
}
bool Application::focus_toward(int dx, int dy)
{
    auto* const root{dialog_shown() != nullptr ? dialog_shown() : m_content.get()};
    if(root == nullptr || m_focused == nullptr)
    {
        return false;
    }
    std::vector<Widget*> candidates;
    root->collect_focusable(candidates);
    const auto from{m_focused->bounds()};
    Widget* best{nullptr};
    auto best_score{0};
    for(auto* const candidate : candidates)
    {
        const auto to{candidate->bounds()};
        if(candidate == m_focused || to.width <= 0 || to.height <= 0)
        {
            continue;
        }
        // How far past the side it is on, and how far apart along that side: none for one beside it.
        const auto across{dx > 0   ? to.x - (from.x + from.width)
                          : dx < 0 ? from.x - (to.x + to.width)
                          : dy > 0 ? to.y - (from.y + from.height)
                                   : from.y - (to.y + to.height)};
        if(across < 0)
        {
            continue;
        }
        const auto along{dx != 0 ? std::max({0, to.y - (from.y + from.height), from.y - (to.y + to.height)})
                                 : std::max({0, to.x - (from.x + from.width), from.x - (to.x + to.width)})};
        // One beside it comes before one nearer but off to a side.
        const auto score{across + 3 * along};
        if(best == nullptr || score < best_score)
        {
            best = candidate;
            best_score = score;
        }
    }
    if(best == nullptr)
    {
        return false;
    }
    focus(*best);
    return true;
}
const Theme& Application::theme() const
{
    return m_theme;
}
Clipboard& Application::clipboard()
{
    return m_clipboard;
}
void Application::set_title(std::string_view title)
{
    m_backend->set_title(title);
}
CommandRegistry& Application::commands()
{
    return m_commands;
}
const CommandRegistry& Application::commands() const
{
    return m_commands;
}
Widget* Application::focused() const
{
    if(m_focused == nullptr || m_focused_lifetime.expired() || m_content == nullptr || !m_content->contains(m_focused))
    {
        return nullptr;
    }
    return m_focused;
}
void Application::set_event_observer(std::function<void(const Event&)> observer)
{
    m_event_observer = std::move(observer);
}
void Application::post(std::function<void()> callback)
{
    // Woken under the lock, so set_backend cannot swap the backend in between.
    std::lock_guard<std::mutex> lock{m_post_mutex};
    m_posted.push_back(std::move(callback));
    m_backend->wake();
}
void Application::set_backend(std::unique_ptr<Backend> backend)
{
    std::unique_ptr<Backend> previous;
    {
        std::lock_guard<std::mutex> lock{m_post_mutex};
        previous = std::exchange(m_backend, std::move(backend));
    }
    // The clipboard offers copied text to the backend, so it goes on to the new one before the old is gone.
    m_clipboard.set_backend(m_backend.get());
    // A new backend starts with its own arrow.
    m_pointer = Pointer::arrow;
    offer_zoom();
    m_needs_render = true;
}

void Application::dispatch_event(const Event& event)
{
    if(is_wanted(event))
    {
        deliver_event(event);
    }
}

void Application::deliver_drop(const DropEvent& drop)
{
    Widget* taker{};
    if(drop.phase != DropEvent::Phase::left && m_content != nullptr)
    {
        for(auto* widget{m_content->hit_test(drop.x, drop.y)}; widget != nullptr; widget = widget->parent())
        {
            if(widget->handle_event(drop))
            {
                taker = widget;
                break;
            }
        }
    }
    // The one they were over before hears that they went, after the one they came to heard them come.
    if(auto* const before{m_drop_target.get()}; before != nullptr && before != taker)
    {
        before->handle_event(DropEvent{DropEvent::Phase::left, drop.x, drop.y, {}, drop.pixel});
    }
    m_drop_target = drop.phase == DropEvent::Phase::moving ? taker : nullptr;
}

bool Application::is_wanted(const Event& event)
{
    const auto* const mouse{std::get_if<MouseEvent>(&event)};
    if(mouse == nullptr || m_content == nullptr)
    {
        return true;
    }
    const auto moves{mouse->hover || mouse->dragging || mouse->right_dragging || mouse->middle_dragging};
    if(!moves && !mouse->middle)
    {
        return true;
    }
    const auto* const target{m_mouse_capture != nullptr && !m_mouse_capture_lifetime.expired()
                                 ? m_mouse_capture
                                 : m_content->hit_test(mouse->x, mouse->y)};
    const auto raw{target != nullptr && target->takes_raw_pointer()};
    if(mouse->middle)
    {
        return raw;
    }
    // A terminal reports a move only from one cell to another; a drag goes by its place in the cell too, as it may be
    // selecting text set in a proportional font.
    if(!raw && m_last_motion && m_last_motion->cell.x == mouse->x && m_last_motion->cell.y == mouse->y &&
       (!mouse->dragging || m_last_motion->offset == mouse->cell_offset))
    {
        return false;
    }
    m_last_motion = Motion{{mouse->x, mouse->y}, mouse->cell_offset};
    return true;
}

void Application::deliver_event(const Event& event)
{
    if(m_event_observer)
    {
        m_event_observer(event);
    }
    if(const auto* const mouse{std::get_if<MouseEvent>(&event)})
    {
        // Ctrl and the wheel make everything bigger or smaller, wherever the pointer is, where the backend can.
        if(m_wheel_zooms && mouse->control && mouse->wheel_delta != 0 && zoom(mouse->wheel_delta > 0 ? 1 : -1))
        {
            return;
        }
        follow_pointer(*mouse);
    }
    else if(std::holds_alternative<KeyEvent>(event) || std::holds_alternative<TextEvent>(event))
    {
        hide_tooltip();
    }
    if(const auto* const shutdown{std::get_if<ShutdownEvent>(&event)})
    {
        if(!m_shutting_down)
        {
            m_shutting_down = true;
            m_shutdown.fire(shutdown->reason);
            quit();
        }
        return;
    }
    if(const auto* const modifiers{std::get_if<ModifiersEvent>(&event)})
    {
        m_modifiers_changed.fire(*modifiers);
        return;
    }
    if(std::holds_alternative<CloseRequestEvent>(event))
    {
        if(m_close_requested)
        {
            m_close_requested();
        }
        else
        {
            quit();
        }
        return;
    }
    if(const auto* const drop{std::get_if<DropEvent>(&event)})
    {
        deliver_drop(*drop);
        return;
    }
    ensure_focus_is_valid();
    if(handle_key_mode(event))
    {
        return;
    }
    if(dispatch_to_popup(event))
    {
        return;
    }
    if(auto* const dialog{dialog_shown()}; dialog != nullptr && activate_mnemonic(*dialog, event, false))
    {
        return;
    }
    if(const auto* const focus{std::get_if<FocusEvent>(&event)})
    {
        m_terminal_focus_changed.fire(focus->focused);
        return;
    }
    if(const auto* const original_mouse{std::get_if<MouseEvent>(&event)})
    {
        // No drag outlives the button: letting go clears what it showed, whoever takes the release.
        struct DragEnd
        {
            bool releasing;
            ~DragEnd()
            {
                if(releasing)
                {
                    set_drag_image(std::nullopt);
                }
            }
        } const drag_end{original_mouse->released};
        if(m_content == nullptr)
        {
            return;
        }
        // Presses on the same cell in quick succession count up to a triple click, then start over.
        auto counted{*original_mouse};
        if(counted.pressed)
        {
            constexpr std::chrono::milliseconds multi_click_interval{500};
            const auto now{std::chrono::steady_clock::now()};
            const auto repeated{counted.x == m_last_press.x && counted.y == m_last_press.y &&
                                counted.right == m_last_press.right &&
                                now - m_last_press.time <= multi_click_interval && m_last_press.clicks < 3};
            counted.clicks = repeated ? m_last_press.clicks + 1 : 1;
            m_last_press = {counted.x, counted.y, counted.right, counted.clicks, now};
        }
        const Event counted_event{counted};
        const auto* const mouse{&counted};
        if(m_discard_mouse_until_release)
        {
            if(!mouse->pressed)
            {
                m_discard_mouse_until_release = !mouse->released;
                return;
            }
            m_discard_mouse_until_release = false;
        }
        if(mouse->pressed)
        {
            m_content->dismiss_overlays_at(mouse->x, mouse->y);
        }
        auto* target{m_mouse_capture != nullptr && !m_mouse_capture_lifetime.expired()
                         ? m_mouse_capture
                         : m_content->hit_test(mouse->x, mouse->y)};
        if(target != nullptr && !m_content->contains(target))
        {
            target = nullptr;
        }
        m_pointer_position = Point{mouse->x, mouse->y};
        show_pointer(target);
        // A press focuses what it falls on, or the first thing round it that takes the focus, as a list does for a
        // click on one of its rows.
        if(mouse->pressed)
        {
            for(auto* widget{target}; widget != nullptr; widget = widget->parent())
            {
                if(widget->is_focusable())
                {
                    set_focus(widget);
                    break;
                }
            }
        }
        if(mouse->pressed)
        {
            m_mouse_capture = target;
            m_mouse_capture_lifetime = target == nullptr ? std::weak_ptr<void>{} : target->m_lifetime;
        }
        for(auto* widget{target}; widget != nullptr; widget = widget->parent())
        {
            if(widget->handle_event(counted_event))
            {
                if(mouse->released)
                {
                    m_mouse_capture = nullptr;
                    m_mouse_capture_lifetime.reset();
                }
                return;
            }
        }
        if(mouse->released)
        {
            m_mouse_capture = nullptr;
            m_mouse_capture_lifetime.reset();
        }
        return;
    }
    if(const auto* const key{std::get_if<KeyEvent>(&event)})
    {
        // Tab and Shift+Tab go round the focus unless the focused widget takes them; with Ctrl or Alt they are
        // shortcuts like any other key, such as Ctrl+Tab for the next tab.
        const auto plain{!key->control && !key->alt};
        if(key->key == Key::tab && plain)
        {
            if(!dispatch_to_focused(event))
            {
                focus_next();
            }
            return;
        }
        if(key->key == Key::shift_tab && plain)
        {
            if(!dispatch_to_focused(event))
            {
                focus_previous();
            }
            return;
        }
    }
    if(m_content != nullptr)
    {
        if(auto* const target{m_content->keyboard_target()})
        {
            auto is_overlay_claim{false};
            for(auto* w{target}; w != nullptr; w = w->parent())
            {
                if(w->is_overlay())
                {
                    is_overlay_claim = true;
                    break;
                }
            }
            if(is_overlay_claim)
            {
                for(auto* widget{target}; widget != nullptr; widget = widget->parent())
                {
                    if(widget->handle_event(event))
                    {
                        return;
                    }
                }
            }
        }
    }
    // A reserved command's keys are its own even over a widget that takes every key, as a terminal does.
    if(m_modal_depth == 0 && m_commands.dispatch_reserved(event))
    {
        return;
    }
    // Keys a focused widget claims as its own go to it first, as a file panel's F5.
    if((std::holds_alternative<KeyEvent>(event) || std::holds_alternative<TextEvent>(event)) && m_focused != nullptr &&
       m_focused->claims_key(event) && dispatch_to_focused(event))
    {
        return;
    }
    // Shortcuts the application bound come before a widget's default handling of the same keys, as in
    // most toolkits; widgets then need not know which keys the application reserves.
    if(m_modal_depth == 0 && (m_content == nullptr || m_content->keyboard_target() == nullptr) &&
       m_commands.dispatch(event))
    {
        return;
    }
    if(m_focused != nullptr && m_focused->m_typing_delegate != nullptr &&
       !m_focused->m_typing_delegate_lifetime.expired() && is_typing(event))
    {
        set_focus(m_focused->m_typing_delegate);
    }
    if(dispatch_to_focused(event))
    {
        return;
    }
    if(edit_focused_field(event))
    {
        return;
    }
    // A letter alone, that what has the focus did not take, as Alt and the letter: Turbo Vision's way in a dialog.
    if(auto* const dialog{dialog_shown()}; dialog != nullptr && activate_mnemonic(*dialog, event, true))
    {
        return;
    }
    // In a dialog an arrow nobody used steps to the next or previous field, as in classic form dialogs;
    // elsewhere it stays with the widget that has the focus.
    if(const auto* const key{std::get_if<KeyEvent>(&event)}; key != nullptr && m_modal_depth > 0)
    {
        if(key->key == Key::down || key->key == Key::up)
        {
            focus_by_offset(key->key == Key::down ? 1 : -1, false);
            return;
        }
    }
    if(m_content != nullptr)
    {
        if(auto* const target{m_content->keyboard_target()})
        {
            if(target != m_focused)
            {
                for(auto* widget{target}; widget != nullptr; widget = widget->parent())
                {
                    if(widget->handle_event(event))
                    {
                        return;
                    }
                }
            }
        }
        // Outside a dialog Alt and a letter are the menus' first; one nothing took goes to the window's letters, as a
        // script's window has them.
        if(dialog_shown() == nullptr)
        {
            static_cast<void>(activate_mnemonic(*m_content, event, false));
        }
    }
}

Timer::Timer(std::shared_ptr<State> state) : m_state{std::move(state)}
{
}
Timer::~Timer()
{
    stop();
}
Timer& Timer::operator=(Timer&& other) noexcept
{
    if(this != &other)
    {
        stop();
        m_state = std::move(other.m_state);
    }
    return *this;
}
void Timer::stop()
{
    m_state.reset();
}
bool Timer::is_active() const
{
    return m_state != nullptr;
}

Timer Application::start_timer(std::chrono::milliseconds interval, std::function<void()> callback)
{
    auto state{std::make_shared<Timer::State>(
        Timer::State{interval, std::chrono::steady_clock::now() + interval, std::move(callback)})};
    m_timers.push_back(state);
    return Timer{std::move(state)};
}
bool Application::process_timers()
{
    const auto now{std::chrono::steady_clock::now()};
    std::vector<std::weak_ptr<Timer::State>> due;
    m_timers.erase(std::remove_if(m_timers.begin(), m_timers.end(),
                                  [](const std::weak_ptr<Timer::State>& timer) { return timer.expired(); }),
                   m_timers.end());
    for(const auto& timer : m_timers)
    {
        if(const auto state{timer.lock()}; state != nullptr && state->due <= now)
        {
            state->due = now + state->interval;
            due.push_back(timer);
        }
    }
    // A callback may stop timers, its own included: one stopped by an earlier callback does not run.
    for(const auto& timer : due)
    {
        if(const auto state{timer.lock()})
        {
            state->callback();
        }
    }
    return !due.empty();
}
std::optional<std::chrono::steady_clock::time_point> Application::next_timer_due()
{
    std::optional<std::chrono::steady_clock::time_point> next;
    for(const auto& timer : m_timers)
    {
        if(const auto state{timer.lock()}; state != nullptr && (!next || state->due < *next))
        {
            next = state->due;
        }
    }
    return next;
}

void Application::run_frame()
{
    m_closed_popups.clear();
    const auto ran_timers{process_timers()};
    const auto ran_posted{process_posted()};
    auto handled_events{false};
    constexpr std::size_t maximum_events_per_frame{1024};
    for(std::size_t event_count{}; event_count < maximum_events_per_frame; ++ event_count)
    {
        const auto event{m_backend->poll_event()};
        if(!event.has_value())
        {
            break;
        }
        if(!is_wanted(*event))
        {
            continue;
        }
        deliver_event(*event);
        handled_events = true;
    }
    if(ran_timers || ran_posted || handled_events)
    {
        m_needs_render = true;
    }
    if(m_needs_render)
    {
        render();
    }
    // Everything is event driven: with no input handled and no work posted, sleep until the next event or
    // the next timer.
    // Work posted while this frame ran is picked up by the next one instead.
    if(!ran_posted && !handled_events && !has_posted())
    {
        m_backend->wait_for_input(next_timer_due());
    }
}
bool Application::has_posted()
{
    std::lock_guard<std::mutex> lock{m_post_mutex};
    return !m_posted.empty();
}
int Application::exec()
{
    m_is_running = !m_shutting_down;
    while(m_is_running)
    {
        run_frame();
    }
    return 0;
}
bool Application::reports_modifiers() const
{
    return m_backend->reports_modifiers();
}
void Application::on_close_requested(std::function<void()> callback)
{
    m_close_requested = std::move(callback);
}

void Application::on_zoomed(std::function<void()> callback)
{
    m_zoomed = std::move(callback);
}

void Application::set_wheel_zooms(bool zooms)
{
    m_wheel_zooms = zooms;
}
bool Application::is_shutting_down() const
{
    return m_shutting_down;
}
void Application::quit()
{
    m_is_running = false;
}
void Application::render()
{
    if(m_content == nullptr)
    {
        return;
    }
    const auto size{m_backend->size()};
    m_content->arrange({0, 0, size.width, size.height});
    m_backend->render(
        [&](Painter& painter)
        {
            m_content->paint(painter, m_theme);
            if(m_popup != nullptr)
            {
                m_popup->arrange({0, 0, size.width, size.height});
                m_popup->paint(painter, m_theme);
            }
            paint_tooltip(painter, size);
            paint_key_mode_hint(painter, size);
            if(const auto& image{drag_image()})
            {
                painter.draw_drag_image(image->position, image->cell_offset, image->text, m_theme);
            }
        });
    m_needs_render = false;
}
void Application::show_pointer(const Widget* target)
{
    // The pointer as what is under it, or what it is dragging, asks.
    const auto pointer{target == nullptr || !m_pointer_position ? Pointer::arrow
                                                                : target->pointer_at(*m_pointer_position)};
    if(pointer != m_pointer)
    {
        m_pointer = pointer;
        m_backend->set_pointer(pointer);
    }
}
void Application::update_pointer()
{
    if(m_content == nullptr || !m_pointer_position)
    {
        return;
    }
    auto* target{m_mouse_capture != nullptr && !m_mouse_capture_lifetime.expired()
                     ? m_mouse_capture
                     : m_content->hit_test(m_pointer_position->x, m_pointer_position->y)};
    show_pointer(target != nullptr && m_content->contains(target) ? target : nullptr);
}
void Application::follow_pointer(const MouseEvent& mouse)
{
    hide_tooltip();
    if(!mouse.hover)
    {
        return;
    }
    // Shown once the pointer has rested a moment where it is, as tooltips are everywhere.
    constexpr std::chrono::milliseconds rest{600};
    const Point at{mouse.x, mouse.y};
    const auto pointer_columns{m_backend->pointer_columns(mouse.cell_offset)};
    m_tooltip_timer =
        start_timer(rest,
                    [this, at, pointer_columns]
                    {
                        m_tooltip_timer.stop();
                        if(m_content == nullptr || m_modal_depth != 0)
                        {
                            return;
                        }
                        for(auto* widget{m_content->hit_test(at.x, at.y)}; widget != nullptr; widget = widget->parent())
                        {
                            if(auto text{widget->tooltip_at(at)})
                            {
                                m_tooltip = Tooltip{std::move(*text), at, pointer_columns};
                                m_needs_render = true;
                                return;
                            }
                        }
                    });
}
void Application::hide_tooltip()
{
    m_tooltip_timer.stop();
    if(m_tooltip)
    {
        m_tooltip.reset();
        m_needs_render = true;
    }
}
// Under the pointer, or over it when there is no room below, kept on the screen; a text wider than the screen
// takes more lines, broken where it has to be, so all of it shows.
void Application::paint_tooltip(Painter& painter, Size size) const
{
    if(!m_tooltip || size.width < 3 || size.height <= 0)
    {
        return;
    }
    // As wide as its text shows, which in a proportional font is less than its characters, with a space each side.
    const auto lines_in{[this](int room)
                        {
                            auto lines{wrap_text(m_tooltip->text, std::max(1, room - 2))};
                            auto width{0};
                            for(const auto& line : lines)
                            {
                                width = std::max(width, shown_width(" " + line + " "));
                            }
                            return std::pair{std::move(lines), width};
                        }};
    // Beside the pointer, never under it: starting after the columns the pointer covers, which in pixels depends on
    // the system's pointer, or, with no room there, ending in the column before it; too wide for either side, broken
    // to fit the wider.
    const auto at{m_tooltip->at};
    const auto after{size.width - at.x - m_tooltip->pointer_columns};
    const auto before{at.x};
    auto [lines, width] = lines_in(size.width);
    if(width > after && width > before)
    {
        std::tie(lines, width) = lines_in(std::max(after, before));
    }
    const auto x{width <= after ? at.x + m_tooltip->pointer_columns : std::max(0, at.x - width)};
    if(lines.size() > static_cast<std::size_t>(size.height))
    {
        lines.resize(static_cast<std::size_t>(size.height));
    }
    const auto height{static_cast<int>(lines.size())};
    const auto y{at.y + 1 + height <= size.height ? at.y + 1 : std::max(0, at.y - height)};
    painter.draw_tooltip({x, y, width, height}, lines, m_theme);
}
// During a key mode the bottom line says what the keys do; a hint too wide for it takes the lines above too,
// broken between its parts, as " · " separates them.
void Application::paint_key_mode_hint(Painter& painter, Size size) const
{
    if(!m_key_mode || size.height <= 0)
    {
        return;
    }
    const std::string_view separator{" · "};
    std::vector<std::string> lines{std::string{}};
    std::string_view rest{m_key_mode->hint};
    while(!rest.empty())
    {
        const auto end{rest.find(separator)};
        const auto part{rest.substr(0, end)};
        rest = end == std::string_view::npos ? std::string_view{} : rest.substr(end + separator.size());
        auto joined{lines.back().empty() ? std::string{part}
                                         : lines.back() + std::string{separator} + std::string{part}};
        if(!lines.back().empty() && text_width(joined) > size.width - 2)
        {
            lines.emplace_back(part);
        }
        else
        {
            lines.back() = std::move(joined);
        }
    }
    const TextStyle style{m_theme.status_foreground, m_theme.status_background};
    const auto top{std::max(0, size.height - static_cast<int>(lines.size()))};
    for(std::size_t index{}; index < lines.size() && top + static_cast<int>(index) < size.height; ++ index)
    {
        const auto y{top + static_cast<int>(index)};
        painter.draw_bar({0, y, size.width, 1}, BarKind::status, m_theme);
        painter.draw_label({1, y}, lines[index], style);
    }
}
namespace
{

// Ctrl and a, c, x or v in a field: select, copy, cut or paste, through the clipboard; false for other letters.
template <typename Field> bool edit_field(Field& field, char32_t letter, Clipboard& clipboard)
{
    switch(letter)
    {
    case U'a':
        field.select_all();
        return true;
    case U'c':
    case U'x':
    {
        std::string copied;
        if(letter == U'c')
        {
            field.copy_selection(copied);
        }
        else
        {
            field.cut_selection(copied);
        }
        if(!copied.empty())
        {
            static_cast<void>(clipboard.set_text(copied));
        }
        return true;
    }
    case U'v':
        field.paste(clipboard.text());
        return true;
    default:
        return false;
    }
}

} // namespace

// Ctrl+A, Ctrl+C, Ctrl+X and Ctrl+V in a text field or text nobody else took, as in a dialog, where the
// application's shortcuts wait, or in a program that binds no edit commands, as a script's read-only log in oui_run:
// they select, copy, cut and paste through the clipboard, as everywhere. Read-only text copies but changes not.
bool Application::edit_focused_field(const Event& event)
{
    const auto* const text{std::get_if<TextEvent>(&event)};
    if(text == nullptr || !text->control || text->alt || text->shift)
    {
        return false;
    }
    if(auto* const field{dynamic_cast<LineEdit*>(m_focused)})
    {
        return edit_field(*field, text->codepoint, m_clipboard);
    }
    if(auto* const edit{dynamic_cast<TextEdit*>(m_focused)})
    {
        return edit_field(*edit, text->codepoint, m_clipboard);
    }
    return false;
}
// A capture only exists between a press and its release, so losing it means a click is still in progress.
// Its drag and release would otherwise land on whatever now lies under the pointer, such as the editor
// below a dialog that a double-click just closed.
void Application::drop_mouse_capture()
{
    if(m_mouse_capture != nullptr)
    {
        m_discard_mouse_until_release = true;
    }
    m_mouse_capture = nullptr;
    m_mouse_capture_lifetime.reset();
}
void Application::ensure_focus_is_valid()
{
    if(m_mouse_capture != nullptr &&
       (m_mouse_capture_lifetime.expired() || m_content == nullptr || !m_content->contains(m_mouse_capture)))
    {
        drop_mouse_capture();
    }
    if(m_focused == nullptr ||
       (!m_focused_lifetime.expired() && m_content != nullptr && m_content->contains(m_focused)))
    {
        return;
    }
    m_focused = nullptr;
    m_focused_lifetime.reset();
    focus_next();
}
bool Application::dispatch_to_focused(const Event& event)
{
    // A widget disabled while it had the focus hears nothing more.
    if(m_focused != nullptr && !m_focused->is_enabled())
    {
        return false;
    }
    for(auto* widget{m_focused}; widget != nullptr; widget = widget->parent())
    {
        if(widget->key_handler_keeps(event) || widget->handle_event(event))
        {
            return true;
        }
    }
    if(m_focused == nullptr && m_content != nullptr)
    {
        return m_content->handle_event(event);
    }
    return false;
}
bool Application::process_posted()
{
    std::vector<std::function<void()>> posted;
    {
        std::lock_guard<std::mutex> lock{m_post_mutex};
        posted.swap(m_posted);
    }
    for(const auto& callback : posted)
    {
        callback();
    }
    return !posted.empty();
}
void Application::focus_next()
{
    focus_by_offset(1);
}
void Application::focus_previous()
{
    focus_by_offset(-1);
}
void Application::set_focus(Widget* const widget)
{
    if(m_focused != nullptr && !m_focused_lifetime.expired() && m_content != nullptr && m_content->contains(m_focused))
    {
        m_focused->set_focused(false);
    }
    m_focused = widget;
    m_focused_lifetime = widget == nullptr ? std::weak_ptr<void>{} : widget->m_lifetime;
    if(m_focused != nullptr)
    {
        m_focused->set_focused(true);
    }
}
void Application::focus_by_offset(int offset, bool wrap)
{
    if(m_content == nullptr)
    {
        return;
    }
    std::vector<Widget*> focusable;
    m_content->collect_focusable(focusable);
    if(focusable.empty())
    {
        return;
    }
    const auto current{std::find(focusable.begin(), focusable.end(), m_focused)};
    if(!wrap && current != focusable.end())
    {
        const auto target{std::distance(focusable.begin(), current) + offset};
        if(target >= 0 && target < static_cast<std::ptrdiff_t>(focusable.size()))
        {
            set_focus(focusable.at(static_cast<std::size_t>(target)));
        }
        return;
    }
    const auto index{current == focusable.end() ? 0
                                                : (static_cast<int>(std::distance(focusable.begin(), current)) +
                                                   offset + static_cast<int>(focusable.size())) %
                                                      static_cast<int>(focusable.size())};
    set_focus(focusable.at(static_cast<std::size_t>(index)));
}

} // namespace olagarro::oui
