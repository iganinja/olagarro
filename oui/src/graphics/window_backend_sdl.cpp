#include <oui/graphics/pixel_backend.h>
#include <oui/graphics/window_backend.h>
#include <oui/utf8.h>

#include "system_pointer.h"
#include "wheel.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <deque>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace olagarro::oui
{

namespace
{

[[nodiscard]] std::optional<Key> named_key(SDL_Keycode key, bool shift)
{
    switch(key)
    {
    case SDLK_TAB:
        return shift ? Key::shift_tab : Key::tab;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        return Key::enter;
    case SDLK_SPACE:
        return Key::space;
    case SDLK_BACKSPACE:
        return Key::backspace;
    case SDLK_DELETE:
        return Key::delete_;
    case SDLK_ESCAPE:
        return Key::escape;
    case SDLK_LEFT:
        return Key::left;
    case SDLK_RIGHT:
        return Key::right;
    case SDLK_UP:
        return Key::up;
    case SDLK_DOWN:
        return Key::down;
    case SDLK_HOME:
        return Key::home;
    case SDLK_END:
        return Key::end;
    case SDLK_PAGEUP:
        return Key::page_up;
    case SDLK_PAGEDOWN:
        return Key::page_down;
    case SDLK_INSERT:
        return Key::insert_;
    case SDLK_F1:
        return Key::f1;
    case SDLK_F2:
        return Key::f2;
    case SDLK_F3:
        return Key::f3;
    case SDLK_F4:
        return Key::f4;
    case SDLK_F5:
        return Key::f5;
    case SDLK_F6:
        return Key::f6;
    case SDLK_F7:
        return Key::f7;
    case SDLK_F8:
        return Key::f8;
    case SDLK_F9:
        return Key::f9;
    case SDLK_F10:
        return Key::f10;
    case SDLK_F11:
        return Key::f11;
    case SDLK_F12:
        return Key::f12;
    default:
        return std::nullopt;
    }
}

[[nodiscard]] Color mixed(Color from, Color to, int alpha)
{
    const auto channel{[alpha](std::uint8_t a, std::uint8_t b)
                       { return static_cast<std::uint8_t>((a * (255 - alpha) + b * alpha + 127) / 255); }};
    return {channel(from.red, to.red), channel(from.green, to.green), channel(from.blue, to.blue)};
}

// The window has no frame from the desktop: OUI draws its own, with a title bar and its buttons, and tells the
// desktop which parts move and resize the window, so it does that its own way.
[[nodiscard]] bool is_modifier(SDL_Keycode key)
{
    return key == SDLK_LCTRL || key == SDLK_RCTRL || key == SDLK_LSHIFT || key == SDLK_RSHIFT || key == SDLK_LALT ||
           key == SDLK_RALT;
}

// The modifiers as a key event left them; AltGr writes characters, so only the left Alt counts as Alt.
[[nodiscard]] ModifiersEvent modifiers_of(SDL_Keymod modifiers)
{
    return {(modifiers & SDL_KMOD_CTRL) != 0, (modifiers & SDL_KMOD_LALT) != 0, (modifiers & SDL_KMOD_SHIFT) != 0};
}

// SDL's video goes, and SDL itself once nothing else of it is open, as sound may still be playing.
void quit_video()
{
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    if(SDL_WasInit(0) == 0)
    {
        SDL_Quit();
    }
}

class WindowBackend final : public DesktopWindow
{
  public:
    explicit WindowBackend(const WindowOptions& options) : DesktopWindow{options.style}, m_title{options.title}
    {
        if(!SDL_Init(SDL_INIT_VIDEO))
        {
            throw std::runtime_error{std::string{"Cannot open a window: "} + SDL_GetError()};
        }
        // Hidden until it is where it belongs, so it does not jump there.
        const auto& geometry{options.geometry};
        if(!SDL_CreateWindowAndRenderer(options.title.c_str(), whole(geometry.size.width), whole(geometry.size.height),
                                        SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN, &m_window,
                                        &m_renderer))
        {
            const std::string error{SDL_GetError()};
            quit_video();
            throw std::runtime_error{"Cannot open a window: " + error};
        }
        SDL_SetRenderVSync(m_renderer, 1);
        if(options.icon && options.icon->size > 0)
        {
            // A copy, as SDL takes pixels it may write to; it copies them again for the window.
            auto pixels{options.icon->pixels};
            const auto size{options.icon->size};
            if(auto* const surface{
                   SDL_CreateSurfaceFrom(size, size, SDL_PIXELFORMAT_ARGB8888, pixels.data(), size * 4)})
            {
                SDL_SetWindowIcon(m_window, surface);
                SDL_DestroySurface(surface);
            }
        }
        if(geometry.position)
        {
            const SDL_Rect wanted{whole(geometry.position->x), whole(geometry.position->y), whole(geometry.size.width),
                                  whole(geometry.size.height)};
            if(SDL_GetDisplayForRect(&wanted) != 0)
            {
                SDL_SetWindowPosition(m_window, wanted.x, wanted.y);
            }
        }
        m_normal = {geometry.position, geometry.size, false};
        if(geometry.maximized)
        {
            SDL_MaximizeWindow(m_window);
        }
        SDL_ShowWindow(m_window);
        SDL_SetWindowHitTest(m_window, &WindowBackend::hit_test, this);
        m_wake_event = SDL_RegisterEvents(1);
        SDL_StartTextInput(m_window);
        int width{};
        int height{};
        SDL_GetWindowSizeInPixels(m_window, &width, &height);
        lay_out(width, height);
    }

    ~WindowBackend() override
    {
        for(auto* const cursor : m_cursors)
        {
            if(cursor != nullptr)
            {
                SDL_DestroyCursor(cursor);
            }
        }
        if(m_texture != nullptr)
        {
            SDL_DestroyTexture(m_texture);
        }
        SDL_DestroyRenderer(m_renderer);
        SDL_DestroyWindow(m_window);
        quit_video();
    }

    WindowBackend(const WindowBackend&) = delete;
    WindowBackend& operator=(const WindowBackend&) = delete;

    std::optional<Event> poll_event() override
    {
        SDL_Event event;
        while(m_pending.empty() && SDL_PollEvent(&event))
        {
            take(event);
        }
        if(m_pending.empty())
        {
            return std::nullopt;
        }
        auto next{m_pending.front()};
        m_pending.pop_front();
        return next;
    }

    void wait_for_input(std::optional<std::chrono::steady_clock::time_point> deadline) override
    {
        if(!m_pending.empty())
        {
            return;
        }
        auto timeout{-1};
        if(deadline)
        {
            const auto left{
                std::chrono::duration_cast<std::chrono::milliseconds>(*deadline - std::chrono::steady_clock::now())};
            timeout = static_cast<int>(std::max<long long>(0, left.count()));
        }
        SDL_Event event;
        if(SDL_WaitEventTimeout(&event, timeout))
        {
            take(event);
        }
    }

    void wake() override
    {
        SDL_Event event{};
        event.type = m_wake_event;
        SDL_PushEvent(&event);
    }

    bool reports_modifiers() const override { return true; }
    bool is_key_held(Key key) const override
    {
        // The SDL key that names it, and whether the key in its place on the keyboard is down.
        static constexpr SDL_Keycode keys[]{SDLK_TAB,    SDLK_RETURN, SDLK_SPACE,  SDLK_BACKSPACE, SDLK_DELETE,
                                            SDLK_ESCAPE, SDLK_LEFT,   SDLK_RIGHT,  SDLK_UP,        SDLK_DOWN,
                                            SDLK_HOME,   SDLK_END,    SDLK_PAGEUP, SDLK_PAGEDOWN,  SDLK_INSERT};
        int count{};
        const auto* const state{SDL_GetKeyboardState(&count)};
        for(const auto each : keys)
        {
            if(named_key(each, false) == key)
            {
                const auto scancode{SDL_GetScancodeFromKey(each, nullptr)};
                return scancode > 0 && scancode < count && state[scancode];
            }
        }
        return false;
    }

    WindowGeometry geometry() const override
    {
        auto result{m_normal};
        result.maximized = maximized();
        if(!result.maximized)
        {
            int x{};
            int y{};
            SDL_GetWindowPosition(m_window, &x, &y);
            int width{};
            int height{};
            SDL_GetWindowSize(m_window, &width, &height);
            result.size = Size{static_cast<double>(width), static_cast<double>(height)};
            result.position = Point{static_cast<double>(x), static_cast<double>(y)};
        }
        return result;
    }

    void fit_content(Size cells) override
    {
        const auto cell{cell_size()};
        auto width{whole(cells.width * cell.width) + 2 * frame};
        auto height{whole(cells.height * cell.height) + 2 * frame + m_title_height};
        SDL_Rect room{};
        if(SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(m_window), &room))
        {
            width = std::min(width, room.w);
            height = std::min(height, room.h);
        }
        SDL_SetWindowSize(m_window, width, height);
        SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }

    void set_clipboard_text(std::string_view text) override { SDL_SetClipboardText(std::string{text}.c_str()); }

    [[nodiscard]] int pointer_size() const override { return m_pointer_size; }

    void set_pointer(Pointer pointer) override
    {
        m_pointer = pointer;
        show_cursor(pointer);
    }

    void set_title(std::string_view title) override
    {
        m_title = title;
        SDL_SetWindowTitle(m_window, m_title.c_str());
        redecorate();
    }

  protected:
    void fonts_changed() override
    {
        lay_out(canvas().width(), canvas().height());
        m_pending.push_back(ResizeEvent{size().width, size().height});
    }

    void decorate(Canvas& canvas, const GraphicalStyle& style, Font& font) override
    {
        const PixelRect whole{0, 0, canvas.width(), canvas.height()};
        // The edge of the window, raised as every OUI window's.
        canvas.fill({0, 0, whole.width, frame}, style.light);
        canvas.fill({0, 0, frame, whole.height}, style.light);
        canvas.fill({0, whole.height - frame, whole.width, frame}, style.shadow);
        canvas.fill({whole.width - frame, 0, frame, whole.height}, style.shadow);
        // The title bar: grey while another window has the focus.
        const auto bar{title_bar()};
        const auto color{m_focused ? style.title : mixed(style.face, style.shadow, 140)};
        canvas.fill(bar, color);
        canvas.fill({bar.x, bar.y + bar.height - 1, bar.width, 1}, mixed(color, {0, 0, 0}, 90));
        const auto top{bar.y + (bar.height - font.line_height()) / 2};
        const auto text_clip{intersect(whole, {bar.x, bar.y, button(Control::minimize).x - bar.x - 4, bar.height})};
        canvas.set_clip(text_clip);
        font.draw(canvas, bar.x + 11, top + 1, m_title, mixed(color, {0, 0, 0}, 90));
        font.draw(canvas, bar.x + 10, top, m_title, style.title_text);
        canvas.set_clip(whole);
        for(const auto control : {Control::minimize, Control::maximize, Control::close})
        {
            draw_button(canvas, control, color, style.title_text);
        }
    }

    void show(const Canvas& canvas) override
    {
        if(m_texture == nullptr || m_texture_width != canvas.width() || m_texture_height != canvas.height())
        {
            if(m_texture != nullptr)
            {
                SDL_DestroyTexture(m_texture);
            }
            m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING,
                                          canvas.width(), canvas.height());
            SDL_SetTextureScaleMode(m_texture, SDL_SCALEMODE_NEAREST);
            m_texture_width = canvas.width();
            m_texture_height = canvas.height();
        }
        SDL_UpdateTexture(m_texture, nullptr, canvas.pixels().data(), canvas.width() * 4);
        SDL_RenderClear(m_renderer);
        SDL_RenderTexture(m_renderer, m_texture, nullptr, nullptr);
        SDL_RenderPresent(m_renderer);
    }

  private:
    enum class Control
    {
        none,
        minimize,
        maximize,
        close
    };

    static constexpr int frame{1};
    // How far in from its edge the window can be grabbed to resize it.
    static constexpr int grip{6};

    void lay_out(int width, int height)
    {
        // The title bar fits its text, whatever the size of the fonts.
        m_title_height = interface_font().line_height() + 10;
        resize(width, height);
        set_content_area({frame, frame + m_title_height, width - 2 * frame, height - 2 * frame - m_title_height});
    }

    [[nodiscard]] PixelRect title_bar() const { return {frame, frame, canvas().width() - 2 * frame, m_title_height}; }

    [[nodiscard]] PixelRect button(Control control) const
    {
        const auto size{m_title_height - 8};
        const auto bar{title_bar()};
        const auto place{control == Control::close ? 0 : control == Control::maximize ? 1 : 2};
        return {bar.x + bar.width - 4 - size - place * (size + 4), bar.y + 4, size, size};
    }

    [[nodiscard]] static bool inside(const PixelRect& rect, int x, int y)
    {
        return x >= rect.x && y >= rect.y && x < rect.x + rect.width && y < rect.y + rect.height;
    }

    [[nodiscard]] Control control_at(int x, int y) const
    {
        for(const auto control : {Control::minimize, Control::maximize, Control::close})
        {
            if(inside(button(control), x, y))
            {
                return control;
            }
        }
        return Control::none;
    }

    [[nodiscard]] bool maximized() const { return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MAXIMIZED) != 0; }

    void draw_button(Canvas& canvas, Control control, Color bar, Color ink) const
    {
        const auto rect{button(control)};
        const auto pressed{m_pressed == control && m_hovered == control};
        auto face{bar};
        if(m_hovered == control)
        {
            face = mixed(bar, {255, 255, 255}, 40);
        }
        const auto light{mixed(face, {255, 255, 255}, 70)};
        const auto dark{mixed(face, {0, 0, 0}, 90)};
        canvas.fill(rect, face);
        canvas.fill({rect.x, rect.y, rect.width, 1}, pressed ? dark : light);
        canvas.fill({rect.x, rect.y, 1, rect.height}, pressed ? dark : light);
        canvas.fill({rect.x, rect.y + rect.height - 1, rect.width, 1}, pressed ? light : dark);
        canvas.fill({rect.x + rect.width - 1, rect.y, 1, rect.height}, pressed ? light : dark);
        const auto shift{pressed ? 1.0f : 0.0f};
        const auto cx{static_cast<float>(rect.x) + static_cast<float>(rect.width) / 2 + shift};
        const auto cy{static_cast<float>(rect.y) + static_cast<float>(rect.height) / 2 + shift};
        const auto half{4.5f};
        switch(control)
        {
        case Control::minimize:
            canvas.draw_line({cx - half, cy + half - 1}, {cx + half, cy + half - 1}, 2.0f, ink);
            break;
        case Control::maximize:
            if(maximized())
            {
                // Restore: a window behind another.
                outline(canvas, {cx - half + 2.5f, cy - half}, {cx + half, cy + half - 2.5f}, ink);
                canvas.fill({static_cast<int>(cx - half), static_cast<int>(cy - half + 2.5f),
                             static_cast<int>(2 * half - 2.5f), static_cast<int>(2 * half - 2.5f)},
                            face);
                outline(canvas, {cx - half, cy - half + 2.5f}, {cx + half - 2.5f, cy + half}, ink);
            }
            else
            {
                outline(canvas, {cx - half, cy - half}, {cx + half, cy + half}, ink);
            }
            break;
        case Control::close:
            canvas.draw_line({cx - half, cy - half}, {cx + half, cy + half}, 1.8f, ink);
            canvas.draw_line({cx + half, cy - half}, {cx - half, cy + half}, 1.8f, ink);
            break;
        case Control::none:
            break;
        }
    }

    static void outline(Canvas& canvas, PixelPoint from, PixelPoint to, Color ink)
    {
        // A window's outline: a thicker top, as its title bar.
        canvas.draw_line({from.x, from.y + 1}, {to.x, from.y + 1}, 2.0f, ink);
        canvas.draw_line({from.x, to.y}, {to.x, to.y}, 1.2f, ink);
        canvas.draw_line({from.x, from.y}, {from.x, to.y}, 1.2f, ink);
        canvas.draw_line({to.x, from.y}, {to.x, to.y}, 1.2f, ink);
    }

    // Window coordinates differ from pixels on a dense display.
    [[nodiscard]] float pixel_scale() const
    {
        int window_width{};
        int pixel_width{};
        int height{};
        SDL_GetWindowSize(m_window, &window_width, &height);
        SDL_GetWindowSizeInPixels(m_window, &pixel_width, &height);
        return window_width > 0 ? static_cast<float>(pixel_width) / static_cast<float>(window_width) : 1.0f;
    }

    static SDL_HitTestResult hit_test(SDL_Window*, const SDL_Point* point, void* data)
    {
        auto& self{*static_cast<WindowBackend*>(data)};
        const auto scale{self.pixel_scale()};
        const auto x{static_cast<int>(static_cast<float>(point->x) * scale)};
        const auto y{static_cast<int>(static_cast<float>(point->y) * scale)};
        const auto width{self.canvas().width()};
        const auto height{self.canvas().height()};
        if(!self.maximized())
        {
            const auto left{x < grip};
            const auto right{x >= width - grip};
            const auto top{y < grip};
            const auto bottom{y >= height - grip};
            if(top && left)
            {
                return SDL_HITTEST_RESIZE_TOPLEFT;
            }
            if(top && right)
            {
                return SDL_HITTEST_RESIZE_TOPRIGHT;
            }
            if(bottom && left)
            {
                return SDL_HITTEST_RESIZE_BOTTOMLEFT;
            }
            if(bottom && right)
            {
                return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
            }
            if(top)
            {
                return SDL_HITTEST_RESIZE_TOP;
            }
            if(bottom)
            {
                return SDL_HITTEST_RESIZE_BOTTOM;
            }
            if(left)
            {
                return SDL_HITTEST_RESIZE_LEFT;
            }
            if(right)
            {
                return SDL_HITTEST_RESIZE_RIGHT;
            }
        }
        if(inside(self.title_bar(), x, y) && self.control_at(x, y) == Control::none)
        {
            return SDL_HITTEST_DRAGGABLE;
        }
        return SDL_HITTEST_NORMAL;
    }

    // A pixel in the content area as a cell; none outside it.
    [[nodiscard]] std::optional<Point> cell_at(float window_x, float window_y) const
    {
        const auto scale{pixel_scale()};
        const auto x{static_cast<int>(window_x * scale)};
        const auto y{static_cast<int>(window_y * scale)};
        const auto& content{content_area()};
        if(!inside(content, x, y))
        {
            return std::nullopt;
        }
        return Point{
            std::floor((x - content.x) / cell_size().width),
            static_cast<double>(GraphicalPainter::row_at(y - content.y, content.height, whole(cell_size().height)))};
    }

    // A pixel in the window, in the content area's pixels: negative or past its size outside it.
    [[nodiscard]] Point content_pixel(float window_x, float window_y) const
    {
        const auto scale{pixel_scale()};
        const auto& content{content_area()};
        return {static_cast<double>(static_cast<int>(std::floor(window_x * scale)) - content.x),
                static_cast<double>(static_cast<int>(std::floor(window_y * scale)) - content.y)};
    }

    // The cell a pixel would be in, as if the cells went on past the content area: where a drag that left it is.
    [[nodiscard]] Point cell_beyond(Point pixel) const
    {
        const auto& content{content_area()};
        const auto floor_divide{[](int value, int by) { return value >= 0 ? value / by : -((-value + by - 1) / by); }};
        const auto x{whole(pixel.x)};
        const auto y{whole(pixel.y)};
        const auto cell_width{whole(cell_size().width)};
        const auto cell_height{whole(cell_size().height)};
        const auto row{y >= 0 && y < content.height ? GraphicalPainter::row_at(y, content.height, cell_height)
                                                    : floor_divide(y, cell_height)};
        return {static_cast<double>(floor_divide(x, cell_width)), static_cast<double>(row)};
    }

    // How far into its cell a pixel lies, from the cell's left.
    [[nodiscard]] int offset_in_cell(float window_x) const
    {
        const auto x{static_cast<int>(window_x * pixel_scale()) - content_area().x};
        return std::max(0, x) % whole(cell_size().width);
    }

    [[nodiscard]] static SDL_SystemCursor system_cursor(Pointer pointer)
    {
        switch(pointer)
        {
        case Pointer::resize_columns:
            return SDL_SYSTEM_CURSOR_EW_RESIZE;
        case Pointer::resize_rows:
            return SDL_SYSTEM_CURSOR_NS_RESIZE;
        case Pointer::resize_down_right:
            return SDL_SYSTEM_CURSOR_NWSE_RESIZE;
        case Pointer::resize_up_right:
            return SDL_SYSTEM_CURSOR_NESW_RESIZE;
        case Pointer::crosshair:
            return SDL_SYSTEM_CURSOR_CROSSHAIR;
        case Pointer::hand:
        case Pointer::grabbing:
            // SDL has a pointing hand, not an open and a closed one.
            return SDL_SYSTEM_CURSOR_POINTER;
        case Pointer::move:
            return SDL_SYSTEM_CURSOR_MOVE;
        case Pointer::text:
            return SDL_SYSTEM_CURSOR_TEXT;
        case Pointer::arrow:
        case Pointer::none:
            break;
        }
        return SDL_SYSTEM_CURSOR_DEFAULT;
    }

    // The system's own pointer for `pointer`, made the first time it is wanted.
    void show_cursor(Pointer pointer)
    {
        if(pointer == m_shown)
        {
            return;
        }
        m_shown = pointer;
        auto& cursor{m_cursors[static_cast<std::size_t>(pointer)]};
        if(cursor == nullptr)
        {
            cursor = SDL_CreateSystemCursor(system_cursor(pointer));
        }
        if(pointer == Pointer::none)
        {
            SDL_HideCursor();
            return;
        }
        SDL_ShowCursor();
        SDL_SetCursor(cursor);
    }

    // The title bar's buttons answer the mouse themselves; true when the event was theirs.
    bool take_control_mouse(const SDL_Event& event)
    {
        const auto scale{pixel_scale()};
        float window_x{};
        float window_y{};
        if(event.type == SDL_EVENT_MOUSE_MOTION)
        {
            window_x = event.motion.x;
            window_y = event.motion.y;
        }
        else
        {
            window_x = event.button.x;
            window_y = event.button.y;
        }
        const auto over{control_at(static_cast<int>(window_x * scale), static_cast<int>(window_y * scale))};
        if(over != m_hovered)
        {
            m_hovered = over;
            redecorate();
        }
        if(event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT && over != Control::none)
        {
            m_pressed = over;
            redecorate();
            return true;
        }
        if(event.type == SDL_EVENT_MOUSE_BUTTON_UP && m_pressed != Control::none)
        {
            const auto pressed{m_pressed};
            m_pressed = Control::none;
            redecorate();
            if(pressed == over)
            {
                act(pressed);
            }
            return true;
        }
        return m_pressed != Control::none || over != Control::none;
    }

    void act(Control control)
    {
        switch(control)
        {
        case Control::minimize:
            SDL_MinimizeWindow(m_window);
            break;
        case Control::maximize:
            if(maximized())
            {
                SDL_RestoreWindow(m_window);
            }
            else
            {
                SDL_MaximizeWindow(m_window);
            }
            break;
        case Control::close:
            m_pending.push_back(CloseRequestEvent{});
            break;
        case Control::none:
            break;
        }
    }

    // Turns an SDL event into what OUI expects, queued.
    void take(const SDL_Event& event)
    {
        // A key's own modifiers, as they were when it went down: by the time a burst of keys is read, Ctrl may be up.
        const auto key_event{event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP};
        const auto modifiers{key_event ? event.key.mod : SDL_GetModState()};
        const auto shift{(modifiers & SDL_KMOD_SHIFT) != 0};
        const auto control{(modifiers & SDL_KMOD_CTRL) != 0};
        // AltGr writes characters, so only the left Alt counts as Alt.
        const auto alt{(modifiers & SDL_KMOD_LALT) != 0};
        switch(event.type)
        {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            m_pending.push_back(CloseRequestEvent{});
            break;
        case SDL_EVENT_WINDOW_MOVED:
        case SDL_EVENT_WINDOW_RESIZED:
            // What it goes back to from maximized.
            if(!maximized())
            {
                m_normal = geometry();
            }
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            lay_out(event.window.data1, event.window.data2);
            m_pending.push_back(ResizeEvent{size().width, size().height});
            break;
        case SDL_EVENT_WINDOW_EXPOSED:
        case SDL_EVENT_WINDOW_MAXIMIZED:
        case SDL_EVENT_WINDOW_RESTORED:
            m_pending.push_back(ResizeEvent{size().width, size().height});
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            m_focused = event.type == SDL_EVENT_WINDOW_FOCUS_GAINED;
            redecorate();
            m_pending.push_back(FocusEvent{m_focused});
            break;
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            if(m_hovered != Control::none)
            {
                m_hovered = Control::none;
                redecorate();
            }
            break;
        case SDL_EVENT_TEXT_INPUT:
            for(std::size_t index{}; event.text.text[index] != '\0';)
            {
                const auto codepoint{decode_utf8(event.text.text, index)};
                // Space comes as a key, as it does from a terminal.
                if(codepoint != U' ')
                {
                    m_pending.push_back(TextEvent{codepoint, false, false, codepoint >= U'A' && codepoint <= U'Z'});
                }
            }
            break;
        case SDL_EVENT_KEY_UP:
            if(is_modifier(event.key.key))
            {
                m_pending.push_back(modifiers_of(event.key.mod));
            }
            break;
        case SDL_EVENT_KEY_DOWN:
        {
            if(is_modifier(event.key.key))
            {
                m_pending.push_back(modifiers_of(event.key.mod));
                break;
            }
            if(const auto key{named_key(event.key.key, shift)})
            {
                m_pending.push_back(KeyEvent{*key, control, alt, shift && *key != Key::shift_tab});
                break;
            }
            // With Ctrl or Alt a key writes nothing, so it comes here as a shortcut: the character the key makes
            // with Shift as it is, lower case for a letter, as the terminal sends it.
            if(!control && !alt)
            {
                break;
            }
            auto character{
                static_cast<char32_t>(SDL_GetKeyFromScancode(event.key.scancode, modifiers & SDL_KMOD_SHIFT, false))};
            // The keypad's signs, as Ctrl and plus or minus zoom.
            if(event.key.key == SDLK_KP_PLUS || event.key.key == SDLK_KP_MINUS)
            {
                character = event.key.key == SDLK_KP_PLUS ? U'+' : U'-';
            }
            if(character == 0 || (character & SDLK_SCANCODE_MASK) != 0)
            {
                break;
            }
            if(character >= U'A' && character <= U'Z')
            {
                character = character - U'A' + U'a';
            }
            m_pending.push_back(TextEvent{character, control, alt, shift});
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        {
            if(take_control_mouse(event))
            {
                break;
            }
            const auto cell{cell_at(event.button.x, event.button.y)};
            const auto pressed{event.type == SDL_EVENT_MOUSE_BUTTON_DOWN};
            // A press outside the content, on the edge, gives nothing; its release still ends a drag.
            if(!cell && pressed)
            {
                break;
            }
            const auto pixel{content_pixel(event.button.x, event.button.y)};
            const auto at{cell.value_or(cell_beyond(pixel))};
            MouseEvent mouse{at.x, at.y, pressed};
            mouse.cell_offset = offset_in_cell(event.button.x);
            mouse.pixel = pixel;
            mouse.released = !pressed;
            mouse.right = event.button.button == SDL_BUTTON_RIGHT;
            mouse.middle = event.button.button == SDL_BUTTON_MIDDLE;
            mouse.shift = shift;
            mouse.control = control;
            mouse.alt = alt;
            if(event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT ||
               event.button.button == SDL_BUTTON_MIDDLE)
            {
                m_pending.push_back(mouse);
            }
            break;
        }
        case SDL_EVENT_MOUSE_MOTION:
        {
            if(take_control_mouse(event))
            {
                show_cursor(Pointer::arrow);
                break;
            }
            const auto dragging{(event.motion.state & SDL_BUTTON_LMASK) != 0};
            const auto right_dragging{(event.motion.state & SDL_BUTTON_RMASK) != 0};
            const auto middle_dragging{(event.motion.state & SDL_BUTTON_MMASK) != 0};
            const auto pixel{content_pixel(event.motion.x, event.motion.y)};
            auto cell{cell_at(event.motion.x, event.motion.y)};
            if(!cell)
            {
                // The frame and the title bar are the window's, where the arrow is; a drag that left the content
                // goes on, from where it would be.
                show_cursor(Pointer::arrow);
                if(!dragging && !right_dragging && !middle_dragging)
                {
                    break;
                }
                cell = cell_beyond(pixel);
            }
            else
            {
                show_cursor(m_pointer);
            }
            // Every pixel it moves: the application tells who hears more than a move from cell to cell.
            if(m_last_pixel && m_last_pixel->x == pixel.x && m_last_pixel->y == pixel.y)
            {
                break;
            }
            m_last_pixel = pixel;
            MouseEvent mouse{cell->x, cell->y, false};
            mouse.cell_offset = offset_in_cell(event.motion.x);
            mouse.pixel = pixel;
            mouse.right_dragging = right_dragging;
            mouse.middle_dragging = middle_dragging;
            // Held while dragging too, as Shift keeping a shape square while it is drawn.
            mouse.shift = shift;
            mouse.control = control;
            mouse.alt = alt;
            if(dragging)
            {
                mouse.dragging = true;
            }
            else
            {
                mouse.hover = true;
            }
            m_pending.push_back(mouse);
            break;
        }
        case SDL_EVENT_DROP_BEGIN:
            m_dropped_paths.clear();
            break;
        case SDL_EVENT_DROP_POSITION:
            if(const auto cell{cell_at(event.drop.x, event.drop.y)})
            {
                m_pending.push_back(DropEvent{
                    DropEvent::Phase::moving, cell->x, cell->y, {}, content_pixel(event.drop.x, event.drop.y)});
                m_drop_over_content = true;
            }
            else if(m_drop_over_content)
            {
                // Over the frame or the title bar, where nothing hears them.
                m_pending.push_back(DropEvent{DropEvent::Phase::left});
                m_drop_over_content = false;
            }
            break;
        case SDL_EVENT_DROP_FILE:
            if(event.drop.data != nullptr)
            {
                m_dropped_paths.emplace_back(event.drop.data);
            }
            m_drop_at = {event.drop.x, event.drop.y};
            break;
        case SDL_EVENT_DROP_COMPLETE:
        {
            // SDL ends the set this way both when files are let go and when they are carried out of the window.
            const auto where{m_dropped_paths.empty() ? std::pair{event.drop.x, event.drop.y} : m_drop_at};
            const auto cell{cell_at(where.first, where.second)};
            if(!m_dropped_paths.empty() && cell)
            {
                m_pending.push_back(DropEvent{DropEvent::Phase::dropped, cell->x, cell->y, std::move(m_dropped_paths),
                                              content_pixel(where.first, where.second)});
            }
            else if(m_drop_over_content || !m_dropped_paths.empty())
            {
                m_pending.push_back(DropEvent{DropEvent::Phase::left});
            }
            m_dropped_paths.clear();
            m_drop_over_content = false;
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL:
        {
            float x{};
            float y{};
            SDL_GetMouseState(&x, &y);
            const auto cell{cell_at(x, y)};
            if(!cell)
            {
                break;
            }
            // Whole notches, as a touchpad turns it a little at a time, with the keys held.
            m_wheel += event.wheel.y;
            const auto notches{static_cast<int>(m_wheel)};
            m_wheel -= static_cast<float>(notches);
            const auto modifiers{SDL_GetModState()};
            for(auto mouse : wheel_events(*cell, notches, (modifiers & SDL_KMOD_CTRL) != 0,
                                          (modifiers & SDL_KMOD_ALT) != 0, (modifiers & SDL_KMOD_SHIFT) != 0))
            {
                mouse.pixel = content_pixel(x, y);
                m_pending.push_back(mouse);
            }
            break;
        }
        default:
            break;
        }
    }

    std::string m_title;
    int m_title_height{30};
    SDL_Window* m_window{};
    SDL_Renderer* m_renderer{};
    SDL_Texture* m_texture{};
    int m_texture_width{};
    int m_texture_height{};
    Uint32 m_wake_event{};
    std::deque<Event> m_pending;
    std::optional<Point> m_last_pixel;
    // Files carried from the system: the paths of the set being dropped, where the last one was let go, and whether
    // the application was last told they are over what it shows.
    std::vector<std::string> m_dropped_paths;
    std::pair<float, float> m_drop_at{};
    bool m_drop_over_content{false};
    float m_wheel{};
    bool m_focused{true};
    WindowGeometry m_normal;
    Control m_hovered{Control::none};
    Control m_pressed{Control::none};
    // The pointer the application wants over what it shows, the one showing, and the system's, by Pointer.
    Pointer m_pointer{Pointer::arrow};
    Pointer m_shown{Pointer::arrow};
    std::array<SDL_Cursor*, static_cast<std::size_t>(Pointer::none) + 1> m_cursors{};
    // The system's pointer's size in pixels, read once: the desktop sets it before programs start.
    int m_pointer_size{system_pointer_size()};
};

} // namespace

std::unique_ptr<DesktopWindow> window_backend(WindowOptions options)
{
    return std::make_unique<WindowBackend>(options);
}

} // namespace olagarro::oui
