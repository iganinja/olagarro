#include <oui/terminal_backend.h>

#include "terminal_backend_helpers.h"
#include "unicode_composition.h"

#include <algorithm>
#include <chrono>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace olagarro::oui
{

using namespace terminal_helpers;

static std::atomic<bool> s_ctrl_c_pending{false};
// Signalled to end a wait for input: by wake(), and by Ctrl+C, whose handler runs on its own thread.
static HANDLE s_wake_event{nullptr};

// Set by the handler when the console window closes or the user logs off.
static std::atomic<bool> s_shutdown_pending{false};
// Signalled once the application has finished, so the handler can let Windows end the process. Windows
// waits about five seconds for the handler to return; returning sooner ends the process at once, before
// the application kept anything. Never closed: the handler may still be waiting on it when the backend goes.
static HANDLE s_shutdown_done{nullptr};

static BOOL WINAPI ctrl_handler(DWORD dwCtrlType)
{
    if(dwCtrlType == CTRL_C_EVENT)
    {
        s_ctrl_c_pending = true;
        if(s_wake_event != nullptr)
        {
            SetEvent(s_wake_event);
        }
        return TRUE;
    }
    if(dwCtrlType == CTRL_CLOSE_EVENT || dwCtrlType == CTRL_LOGOFF_EVENT || dwCtrlType == CTRL_SHUTDOWN_EVENT)
    {
        s_shutdown_pending = true;
        if(s_wake_event != nullptr)
        {
            SetEvent(s_wake_event);
        }
        if(s_shutdown_done != nullptr)
        {
            WaitForSingleObject(s_shutdown_done, 4500);
        }
        return TRUE;
    }
    return FALSE;
}

class AnsiTerminalBackend::Impl
{
  public:
    explicit Impl(TerminalOptions options) : m_options{options}
    {
        m_input = GetStdHandle(STD_INPUT_HANDLE);
        m_output = GetStdHandle(STD_OUTPUT_HANDLE);
        s_wake_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if(s_shutdown_done == nullptr)
        {
            s_shutdown_done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        }
        else
        {
            ResetEvent(s_shutdown_done);
        }
        m_output_code_page = GetConsoleOutputCP();
        if(GetConsoleMode(m_input, &m_input_mode) == 0 || GetConsoleMode(m_output, &m_output_mode) == 0)
        {
            throw std::runtime_error{"OUI requires an interactive Windows console."};
        }
        const DWORD raw_input{
            (m_input_mode | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS) &
            ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_QUICK_EDIT_MODE)};
        if(SetConsoleMode(m_input, raw_input) == 0)
        {
            throw std::runtime_error{"Unable to configure Windows console input."};
        }
        SetConsoleCtrlHandler(ctrl_handler, TRUE);
        m_vt_output = !m_options.force_legacy &&
                      SetConsoleMode(m_output, m_output_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
        if(m_vt_output)
        {
            SetConsoleOutputCP(CP_UTF8);
        }
        if(m_vt_output)
        {
            if(m_options.use_alternate_screen)
            {
                std::fputs("\x1b[?1049h", stdout);
            }
            std::fputs("\x1b[?7l\x1b[?25l", stdout);
            std::fflush(stdout);
        }
        if(!m_vt_output)
        {
            capture_screen();
        }
    }

    void capture_screen()
    {
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if(GetConsoleScreenBufferInfo(m_output, &info) == 0)
        {
            return;
        }
        const auto width{static_cast<SHORT>(info.srWindow.Right - info.srWindow.Left + 1)};
        const auto height{static_cast<SHORT>(info.srWindow.Bottom - info.srWindow.Top + 1)};
        if(width <= 0 || height <= 0)
        {
            return;
        }
        m_saved_cursor = info.dwCursorPosition;
        m_saved_screen.resize(static_cast<std::size_t>(width) * height, {{L' '}, info.wAttributes});
        SMALL_RECT region{info.srWindow.Left, info.srWindow.Top, info.srWindow.Right, info.srWindow.Bottom};
        const COORD buffer_size{width, height};
        const COORD buffer_origin{};
        if(ReadConsoleOutput(m_output, m_saved_screen.data(), buffer_size, buffer_origin, &region) == 0)
        {
            m_saved_screen.clear();
            return;
        }
        m_saved_region = region;
    }

    void restore_screen()
    {
        if(m_saved_screen.empty())
        {
            return;
        }
        const auto width{static_cast<SHORT>(m_saved_region.Right - m_saved_region.Left + 1)};
        const auto height{static_cast<SHORT>(m_saved_region.Bottom - m_saved_region.Top + 1)};
        if(width <= 0 || height <= 0)
        {
            return;
        }
        const COORD buffer_size{width, height};
        const COORD buffer_origin{};
        SMALL_RECT target{m_saved_region};
        WriteConsoleOutput(m_output, m_saved_screen.data(), buffer_size, buffer_origin, &target);
        SetConsoleCursorPosition(m_output, m_saved_cursor);
    }

    ~Impl()
    {
        SetConsoleCtrlHandler(ctrl_handler, FALSE);
        if(s_shutdown_done != nullptr)
        {
            SetEvent(s_shutdown_done);
        }
        if(s_wake_event != nullptr)
        {
            CloseHandle(s_wake_event);
            s_wake_event = nullptr;
        }
        if(m_vt_output)
        {
            std::fputs("\x1b[0m\x1b[?7h", stdout);
            if(m_options.use_alternate_screen)
            {
                std::fputs("\x1b[?1049l", stdout);
            }
            std::fputs("\x1b[?25h", stdout);
            std::fflush(stdout);
        }
        else
        {
            // restore_screen(); // TEMPORARILY DISABLED
        }
        SetConsoleMode(m_input, m_input_mode);
        SetConsoleMode(m_output, m_output_mode);
        if(m_output_code_page != 0)
        {
            SetConsoleOutputCP(m_output_code_page);
        }
    }

    [[nodiscard]] Size size() const
    {
        CONSOLE_SCREEN_BUFFER_INFO info{};
        GetConsoleScreenBufferInfo(m_output, &info);
        return {static_cast<double>(info.srWindow.Right - info.srWindow.Left + 1),
                static_cast<double>(info.srWindow.Bottom - info.srWindow.Top + 1)};
    }

    void set_diagnose(bool enable) { m_diagnose = enable; }

    // The console input handle is signalled while records are waiting, so this sleeps until the next key,
    // mouse or resize record, a wake-up, or `deadline`.
    void wait_for_input(std::optional<std::chrono::steady_clock::time_point> deadline)
    {
        if(!m_pending_events.empty() || s_ctrl_c_pending || s_shutdown_pending)
        {
            return;
        }
        auto timeout{INFINITE};
        if(deadline)
        {
            // Rounded up, so the wait never ends just short of the deadline and spins.
            const auto remaining{
                std::chrono::ceil<std::chrono::milliseconds>(*deadline - std::chrono::steady_clock::now())};
            timeout = static_cast<DWORD>(std::clamp<std::chrono::milliseconds::rep>(
                remaining.count(), 0, static_cast<std::chrono::milliseconds::rep>(INFINITE - 1)));
        }
        const HANDLE handles[]{m_input, s_wake_event};
        WaitForMultipleObjects(s_wake_event != nullptr ? 2 : 1, handles, FALSE, timeout);
    }

    // Windows Terminal takes OSC 52 like other terminals; the classic console has no way to.
    void set_clipboard_text(std::string_view text)
    {
        if(m_vt_output)
        {
            const auto sequence{osc52_clipboard(text)};
            std::fwrite(sequence.data(), 1, sequence.size(), stdout);
            std::fflush(stdout);
        }
    }

    void wake()
    {
        if(s_wake_event != nullptr)
        {
            SetEvent(s_wake_event);
        }
    }

    std::optional<Event> poll_event()
    {
        if(!m_pending_events.empty())
        {
            auto event{std::move(m_pending_events.front())};
            m_pending_events.erase(m_pending_events.begin());
            return event;
        }
        if(s_shutdown_pending.exchange(false))
        {
            return ShutdownEvent{ShutdownEvent::Reason::terminal_closed};
        }
        if(s_ctrl_c_pending.exchange(false))
        {
            return TextEvent{0x03, true, false, false};
        }
        DWORD count{};
        GetNumberOfConsoleInputEvents(m_input, &count);
        if(count == 0)
        {
            return std::nullopt;
        }
        INPUT_RECORD record{};
        DWORD read{};
        ReadConsoleInputW(m_input, &record, 1, &read);
        return handle_record(record);
    }

    enum class DeadKeyState : unsigned char
    {
        none,
        pending
    };

    // Spacing accents, as keyboard layouts report dead keys, and the combining marks they compose with.
    struct DeadKeyAccent
    {
        char32_t spacing;
        char32_t combining;
    };
    static constexpr DeadKeyAccent dead_key_accents[]{
        {U'`', U'\u0300'},      {U'\u00B4', U'\u0301'}, {U'\'', U'\u0301'},     {U'^', U'\u0302'},
        {U'~', U'\u0303'},      {U'\u00AF', U'\u0304'}, {U'\u02D8', U'\u0306'}, {U'\u02D9', U'\u0307'},
        {U'\u00A8', U'\u0308'}, {U'"', U'\u0308'},      {U'\u02DA', U'\u030A'}, {U'\u02DD', U'\u030B'},
        {U'\u02C7', U'\u030C'}, {U'\u00B8', U'\u0327'}, {U'\u02DB', U'\u0328'}};

    [[nodiscard]] static char32_t normalize_dead_key(char32_t diacritic)
    {
        for(const auto& accent : dead_key_accents)
        {
            if(accent.spacing == diacritic)
            {
                return accent.combining;
            }
        }
        return diacritic;
    }

    // The character a dead key types on its own; the first spacing form listed wins.
    [[nodiscard]] static char32_t dead_key_literal(char32_t normalized)
    {
        for(const auto& accent : dead_key_accents)
        {
            if(accent.combining == normalized)
            {
                return accent.spacing;
            }
        }
        return normalized;
    }

    void enter_dead_key(char32_t diacritic, UINT, UINT)
    {
        m_dead_key_state = DeadKeyState::pending;
        m_dead_key_diacritic = normalize_dead_key(diacritic);
    }

    void cancel_dead_key()
    {
        m_dead_key_state = DeadKeyState::none;
        m_dead_key_diacritic = 0;
    }

    [[nodiscard]] bool has_dead_key() const { return m_dead_key_state == DeadKeyState::pending; }

    [[nodiscard]] std::vector<Event> resolve_dead_key(const INPUT_RECORD& record, bool shift)
    {
        if(m_dead_key_state != DeadKeyState::pending)
        {
            return {};
        }
        const auto normalized{m_dead_key_diacritic};
        const auto literal{dead_key_literal(normalized)};
        m_dead_key_state = DeadKeyState::none;
        m_dead_key_diacritic = 0;
        std::vector<Event> events;
        const auto vk{record.Event.KeyEvent.wVirtualKeyCode};
        if(record.Event.KeyEvent.uChar.UnicodeChar != 0)
        {
            const auto base{static_cast<char32_t>(record.Event.KeyEvent.uChar.UnicodeChar)};
            if(const auto composed{compose(base, normalized)})
            {
                events.push_back(TextEvent{*composed, false, false, shift});
            }
            else
            {
                events.push_back(TextEvent{literal, false, false, shift});
                events.push_back(TextEvent{base, false, false, shift});
            }
            return events;
        }
        if(vk == VK_BACK)
        {
            return {};
        }
        if(vk == VK_SPACE)
        {
            events.push_back(TextEvent{literal, false, false, shift});
            events.push_back(KeyEvent{Key::space, false, false, shift});
            return events;
        }
        events.push_back(TextEvent{literal, false, false, shift});
        return events;
    }

    [[nodiscard]] static std::optional<Event> mouse_record_event(const MOUSE_EVENT_RECORD& record)
    {
        const auto& mouse{record};
        const double x{static_cast<double>(mouse.dwMousePosition.X)};
        const double y{static_cast<double>(mouse.dwMousePosition.Y)};
        if(mouse.dwEventFlags == MOUSE_WHEELED)
        {
            const auto delta{static_cast<short>(HIWORD(mouse.dwButtonState)) > 0 ? 1 : -1};
            return MouseEvent{x, y, false, delta};
        }
        if(mouse.dwEventFlags == DOUBLE_CLICK)
        {
            if((mouse.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) != 0)
            {
                return MouseEvent{x, y, true};
            }
            if((mouse.dwButtonState & RIGHTMOST_BUTTON_PRESSED) != 0)
            {
                return MouseEvent{x, y, true, 0, false, false, false, true};
            }
            return std::nullopt;
        }
        if(mouse.dwEventFlags == 0)
        {
            if((mouse.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) != 0)
            {
                return MouseEvent{x, y, true};
            }
            if((mouse.dwButtonState & RIGHTMOST_BUTTON_PRESSED) != 0)
            {
                return MouseEvent{x, y, true, 0, false, false, false, true};
            }
            return MouseEvent{x, y, false, 0, false, true};
        }
        if(mouse.dwEventFlags == MOUSE_MOVED)
        {
            if((mouse.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) != 0)
            {
                return MouseEvent{x, y, false, 0, true};
            }
            return MouseEvent{x, y, false, 0, false, false, true};
        }
        return std::nullopt;
    }

    std::optional<Event> handle_record(const INPUT_RECORD& record)
    {
        if(record.EventType == FOCUS_EVENT)
        {
            return FocusEvent{record.Event.FocusEvent.bSetFocus != 0};
        }
        if(record.EventType == WINDOW_BUFFER_SIZE_EVENT)
        {
            return ResizeEvent{static_cast<double>(record.Event.WindowBufferSizeEvent.dwSize.X),
                               static_cast<double>(record.Event.WindowBufferSizeEvent.dwSize.Y)};
        }
        if(record.EventType == MOUSE_EVENT)
        {
            auto event{mouse_record_event(record.Event.MouseEvent)};
            if(auto* const mouse{event ? std::get_if<MouseEvent>(&*event) : nullptr})
            {
                const auto state{record.Event.MouseEvent.dwControlKeyState};
                mouse->shift = (state & SHIFT_PRESSED) != 0;
                mouse->control = (state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
                mouse->alt = (state & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
            }
            return event;
        }
        if(record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown)
        {
            return std::nullopt;
        }
        const auto key{record.Event.KeyEvent.wVirtualKeyCode};
        const auto state{record.Event.KeyEvent.dwControlKeyState};
        const auto control{(state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0};
        const auto alt{(state & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0};
        const auto shift{(state & SHIFT_PRESSED) != 0};
        const auto alt_gr{(state & (LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED)) == (LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED)};
        if(m_diagnose)
        {
            const auto& ke{record.Event.KeyEvent};
            std::fprintf(stderr,
                         "DIAG vk=0x%02X scan=0x%02X uni=0x%04X state=0x%08lX ctrl=%d alt=%d shift=%d altgr=%d\n", key,
                         ke.wVirtualScanCode, ke.uChar.UnicodeChar, state, control ? 1 : 0, alt ? 1 : 0, shift ? 1 : 0,
                         alt_gr ? 1 : 0);
            std::fflush(stderr);
        }
        const auto is_function_key{key >= VK_F1 && key <= VK_F12};
        if(is_function_key || key == VK_RETURN || key == VK_ESCAPE || key == VK_LEFT || key == VK_RIGHT ||
           key == VK_UP || key == VK_DOWN || key == VK_HOME || key == VK_END || key == VK_PRIOR || key == VK_NEXT ||
           key == VK_INSERT || key == VK_TAB || key == VK_DELETE || key == VK_BACK)
        {
            cancel_dead_key();
        }
        if(is_function_key)
        {
            return KeyEvent{static_cast<Key>(static_cast<int>(Key::f1) + (key - VK_F1)), control, alt, shift};
        }
        if(alt_gr)
        {
            if(record.Event.KeyEvent.uChar.UnicodeChar != 0)
            {
                const auto mapped{static_cast<char32_t>(record.Event.KeyEvent.uChar.UnicodeChar)};
                if(is_diacritic(mapped))
                {
                    enter_dead_key(mapped, key, record.Event.KeyEvent.wVirtualScanCode);
                    return std::nullopt;
                }
                return TextEvent{record.Event.KeyEvent.uChar.UnicodeChar, false, false, shift};
            }
            BYTE alt_gr_kbs[256]{};
            alt_gr_kbs[VK_LCONTROL] = 0x80;
            alt_gr_kbs[VK_RMENU] = 0x80;
            if(shift)
            {
                alt_gr_kbs[VK_SHIFT] = 0x80;
            }
            const auto layout{GetKeyboardLayout(0)};
            wchar_t mapped{};
            const auto result{
                ToUnicodeEx(key, record.Event.KeyEvent.wVirtualScanCode, alt_gr_kbs, &mapped, 1, 0, layout)};
            if(mapped != 0)
            {
                const auto mapped_cp{static_cast<char32_t>(mapped)};
                if(is_diacritic(mapped_cp))
                {
                    enter_dead_key(mapped_cp, key, record.Event.KeyEvent.wVirtualScanCode);
                    return std::nullopt;
                }
                if(result == 1 && mapped >= 0x20)
                {
                    return TextEvent{mapped_cp, false, false, shift};
                }
            }
            return std::nullopt;
        }
        if(has_dead_key() && !control && !alt)
        {
            auto events{resolve_dead_key(record, shift)};
            if(!events.empty())
            {
                m_pending_events = std::move(events);
            }
            if(!m_pending_events.empty())
            {
                return m_pending_events.front();
            }
            return std::nullopt;
        }
        else if(has_dead_key())
        {
            cancel_dead_key();
        }
        if(alt && key >= 'A' && key <= 'Z')
        {
            return TextEvent{static_cast<char32_t>(key - 'A' + 'a'), control, alt, shift};
        }
        if(key == VK_OEM_1 && !control && !alt)
        {
            BYTE keyboard_state[256]{};
            if(shift)
            {
                keyboard_state[VK_SHIFT] = 0x80;
            }
            const auto layout{GetKeyboardLayout(0)};
            const auto corrected_vk{MapVirtualKeyW(record.Event.KeyEvent.wVirtualScanCode, MAPVK_VSC_TO_VK)};
            wchar_t mapped{};
            const auto result{ToUnicodeEx(corrected_vk != 0 ? corrected_vk : key,
                                          record.Event.KeyEvent.wVirtualScanCode, keyboard_state, &mapped, 1, 0,
                                          layout)};
            if(result >= 1 && mapped >= 0x20)
            {
                return TextEvent{static_cast<char32_t>(mapped), control, alt, shift};
            }
            if(result == -1 && mapped >= 0x20)
            {
                const auto mapped_cp{static_cast<char32_t>(mapped)};
                if(is_diacritic(mapped_cp))
                {
                    enter_dead_key(mapped_cp, key, record.Event.KeyEvent.wVirtualScanCode);
                    return std::nullopt;
                }
                return TextEvent{mapped_cp, control, alt, shift};
            }
        }
        if(key == VK_TAB)
        {
            return KeyEvent{shift ? Key::shift_tab : Key::tab, control, alt, shift};
        }
        if(key == VK_RETURN)
        {
            return KeyEvent{Key::enter, control, alt, shift};
        }
        if(key == VK_SPACE)
        {
            return KeyEvent{Key::space, control, alt, shift};
        }
        if(key == VK_BACK)
        {
            return KeyEvent{Key::backspace, control, alt, shift};
        }
        if(key == VK_DELETE)
        {
            return KeyEvent{Key::delete_, control, alt, shift};
        }
        if(key == VK_ESCAPE)
        {
            return KeyEvent{Key::escape, control, alt, shift};
        }
        if(key == VK_LEFT)
        {
            return KeyEvent{Key::left, control, alt, shift};
        }
        if(key == VK_RIGHT)
        {
            return KeyEvent{Key::right, control, alt, shift};
        }
        if(key == VK_UP)
        {
            return KeyEvent{Key::up, control, alt, shift};
        }
        if(key == VK_DOWN)
        {
            return KeyEvent{Key::down, control, alt, shift};
        }
        if(key == VK_HOME)
        {
            return KeyEvent{Key::home, control, alt, shift};
        }
        if(key == VK_END)
        {
            return KeyEvent{Key::end, control, alt, shift};
        }
        if(key == VK_PRIOR)
        {
            return KeyEvent{Key::page_up, control, alt, shift};
        }
        if(key == VK_NEXT)
        {
            return KeyEvent{Key::page_down, control, alt, shift};
        }
        if(key == VK_INSERT)
        {
            return KeyEvent{Key::insert_, control, alt, shift};
        }
        if(record.Event.KeyEvent.uChar.UnicodeChar == 0)
        {
            BYTE keyboard_state[256]{};
            if(control)
            {
                keyboard_state[VK_CONTROL] = 0x80;
            }
            if(alt)
            {
                keyboard_state[VK_MENU] = 0x80;
            }
            if(shift)
            {
                keyboard_state[VK_SHIFT] = 0x80;
            }
            const auto layout{GetKeyboardLayout(0)};
            wchar_t mapped{};
            const auto result{
                ToUnicodeEx(key, record.Event.KeyEvent.wVirtualScanCode, keyboard_state, &mapped, 1, 0, layout)};
            if(result >= 1 && mapped >= 0x20)
            {
                return TextEvent{static_cast<char32_t>(mapped), control, alt, shift};
            }
            if(result == -1 && mapped >= 0x20)
            {
                const auto mapped_cp{static_cast<char32_t>(mapped)};
                if(is_diacritic(mapped_cp))
                {
                    enter_dead_key(mapped_cp, key, record.Event.KeyEvent.wVirtualScanCode);
                    return std::nullopt;
                }
                return TextEvent{mapped_cp, control, alt, shift};
            }
            if(result <= 0)
            {
                const auto corrected_vk{MapVirtualKeyW(record.Event.KeyEvent.wVirtualScanCode, MAPVK_VSC_TO_VK)};
                if(corrected_vk != 0 && corrected_vk != key)
                {
                    wchar_t mapped2{};
                    const auto result2{ToUnicodeEx(corrected_vk, record.Event.KeyEvent.wVirtualScanCode, keyboard_state,
                                                   &mapped2, 1, 0, layout)};
                    if(result2 >= 1 && mapped2 >= 0x20)
                    {
                        return TextEvent{static_cast<char32_t>(mapped2), control, alt, shift};
                    }
                }
            }
        }
        if(record.Event.KeyEvent.uChar.UnicodeChar != 0)
        {
            return TextEvent{record.Event.KeyEvent.uChar.UnicodeChar, control, alt, shift};
        }
        return std::nullopt;
    }

    void present(const CellBuffer& buffer)
    {
        if(m_vt_output)
        {
            present_full_ansi(buffer);
        }
        else
        {
            present_windows(buffer);
        }
        m_previous = buffer;
    }

    void reset() { m_previous.reset(); }

  private:
    void present_full_ansi(const CellBuffer& buffer)
    {
        std::string output;
        output.reserve(static_cast<std::size_t>(buffer.width() * buffer.height() * 2));
        for(auto y{0}; y < buffer.height(); ++ y)
        {
            for(auto x{0}; x < buffer.width();)
            {
                const auto& cell{buffer.cell_at(x, y)};
                const auto changed{!m_previous || m_previous->width() != buffer.width() ||
                                   m_previous->height() != buffer.height() || m_previous->cell_at(x, y) != cell};
                if(!changed)
                {
                    ++ x;
                    continue;
                }
                output += cursor_to(x, y);
                std::string previous_style;
                do
                {
                    const auto& run_cell{buffer.cell_at(x, y)};
                    const auto current_style{style(run_cell)};
                    if(current_style != previous_style)
                    {
                        output += current_style;
                        previous_style = current_style;
                    }
                    output += run_cell.continuation ? " " : utf8(run_cell.glyph);
                    ++ x;
                    if(x == buffer.width())
                    {
                        break;
                    }
                    const auto& next{buffer.cell_at(x, y)};
                    const auto next_changed{!m_previous || m_previous->width() != buffer.width() ||
                                            m_previous->height() != buffer.height() ||
                                            m_previous->cell_at(x, y) != next};
                    if(!next_changed || style(next) != current_style)
                    {
                        break;
                    }
                } while(true);
            }
        }
        if(!output.empty())
        {
            output += "\x1b[0m";
            std::fwrite(output.data(), 1, output.size(), stdout);
            std::fflush(stdout);
        }
    }

    void present_windows(const CellBuffer& buffer) const
    {
        std::vector<CHAR_INFO> cells;
        cells.reserve(static_cast<std::size_t>(buffer.width() * buffer.height()));
        for(auto y{0}; y < buffer.height(); ++ y)
        {
            for(auto x{0}; x < buffer.width(); ++ x)
            {
                const auto& cell{buffer.cell_at(x, y)};
                auto color{static_cast<WORD>(windows_console_attribute(cell.foreground, false) |
                                             windows_console_attribute(cell.background, true))};
                if(cell.attributes & text_bold)
                {
                    color |= FOREGROUND_INTENSITY;
                }
                cells.push_back(
                    {{cell.continuation ? L' ' : (cell.glyph <= 0xffff ? static_cast<wchar_t>(cell.glyph) : L'\ufffd')},
                     static_cast<WORD>(with_visible_foreground(static_cast<std::uint16_t>(color)))});
            }
        }
        CONSOLE_SCREEN_BUFFER_INFO info{};
        GetConsoleScreenBufferInfo(m_output, &info);
        COORD buffer_size{static_cast<SHORT>(buffer.width()), static_cast<SHORT>(buffer.height())};
        COORD buffer_origin{};
        SMALL_RECT target{info.srWindow.Left, info.srWindow.Top,
                          static_cast<SHORT>(info.srWindow.Left + buffer.width() - 1),
                          static_cast<SHORT>(info.srWindow.Top + buffer.height() - 1)};
        WriteConsoleOutputW(m_output, cells.data(), buffer_size, buffer_origin, &target);
    }

    std::optional<CellBuffer> m_previous;
    TerminalOptions m_options;
    bool m_vt_output{true};
    bool m_diagnose{};
    HANDLE m_input{INVALID_HANDLE_VALUE};
    HANDLE m_output{INVALID_HANDLE_VALUE};
    DWORD m_input_mode{};
    DWORD m_output_mode{};
    UINT m_output_code_page{};
    DeadKeyState m_dead_key_state{DeadKeyState::none};
    char32_t m_dead_key_diacritic{};
    std::vector<Event> m_pending_events;
    std::vector<CHAR_INFO> m_saved_screen;
    SMALL_RECT m_saved_region{0, 0, 0, 0};
    COORD m_saved_cursor{};
};

AnsiTerminalBackend::AnsiTerminalBackend(TerminalOptions options) : m_impl{std::make_unique<Impl>(options)}
{
}
AnsiTerminalBackend::~AnsiTerminalBackend() = default;
void AnsiTerminalBackend::wait_for_input(std::optional<std::chrono::steady_clock::time_point> deadline)
{
    m_impl->wait_for_input(deadline);
}
void AnsiTerminalBackend::wake()
{
    m_impl->wake();
}
void AnsiTerminalBackend::set_clipboard_text(std::string_view text)
{
    m_impl->set_clipboard_text(text);
}
void AnsiTerminalBackend::set_diagnose(bool enable)
{
    m_impl->set_diagnose(enable);
}
Size AnsiTerminalBackend::size() const
{
    return m_impl->size();
}
std::optional<Event> AnsiTerminalBackend::poll_event()
{
    return m_impl->poll_event();
}
void AnsiTerminalBackend::present(const CellBuffer& buffer)
{
    m_impl->present(buffer);
}
void AnsiTerminalBackend::reset()
{
    m_impl->reset();
}

std::unique_ptr<AnsiTerminalBackend> terminal_backend(TerminalOptions options)
{
    return std::make_unique<AnsiTerminalBackend>(options);
}

} // namespace olagarro::oui
