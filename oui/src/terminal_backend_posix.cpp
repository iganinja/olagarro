#include <oui/crash_reporter.h>
#include <oui/terminal_backend.h>
#include <oui/utf8.h>

#include "terminal_backend_helpers.h"
#include "terminal_input_decoder.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <csignal>
#include <cerrno>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#ifdef __linux__
#include <linux/kd.h>
#endif

namespace olagarro::oui
{

using namespace terminal_helpers;

namespace
{

volatile sig_atomic_t resize_pending{0};
// The signal that asked the program to stop, SIGHUP or SIGTERM, until the backend reports it.
volatile sig_atomic_t shutdown_signal{0};
// Write end of the backend's wake pipe, so the resize signal can end a wait in progress.
volatile sig_atomic_t wake_write_fd{-1};

void write_wake_byte(int descriptor)
{
    const char byte{'w'};
    // A full pipe already guarantees a wake-up, so a failed write needs no handling.
    if(write(descriptor, &byte, 1) < 0)
    {
        return;
    }
}

// What a crash needs to put the terminal back: the mode it had and whether a backend is using it. Signal
// handlers can only reach globals.
termios original_mode_for_crash{};
volatile sig_atomic_t terminal_in_use{0};

void restore_terminal_after_crash()
{
    if(terminal_in_use == 0)
    {
        return;
    }
    constexpr char reset[]{"\x1b[?1003l\x1b[?1006l\x1b[>4m\x1b[?1004l\x1b[0m\x1b[?7h\x1b[?1049l\x1b[?25h"};
    if(write(STDOUT_FILENO, reset, sizeof(reset) - 1) < 0)
    {
        return;
    }
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_mode_for_crash);
}

// The characters the Linux console's font can show, which the kernel tells any program writing to it.
// Nothing on any other terminal: those draw what they lack from other fonts.
std::optional<std::unordered_set<char32_t>> console_font_glyphs()
{
#ifdef __linux__
    unimapdesc map{};
    // Asked with no room first, the kernel answers with how many entries it has.
    if(ioctl(STDOUT_FILENO, GIO_UNIMAP, &map) == 0 || errno != ENOMEM || map.entry_ct == 0)
    {
        return std::nullopt;
    }
    std::vector<unipair> entries(map.entry_ct);
    map.entries = entries.data();
    if(ioctl(STDOUT_FILENO, GIO_UNIMAP, &map) != 0)
    {
        return std::nullopt;
    }
    std::unordered_set<char32_t> glyphs;
    for(std::size_t index{}; index < map.entry_ct && index < entries.size(); ++ index)
    {
        glyphs.insert(entries[index].unicode);
    }
    return glyphs;
#else
    return std::nullopt;
#endif
}

void on_shutdown_signal(int signal)
{
    shutdown_signal = signal;
    if(wake_write_fd >= 0)
    {
        write_wake_byte(wake_write_fd);
    }
}

void on_terminal_resize(int)
{
    resize_pending = 1;
    if(wake_write_fd >= 0)
    {
        write_wake_byte(wake_write_fd);
    }
}

} // namespace

class AnsiTerminalBackend::Impl
{
  public:
    explicit Impl(TerminalOptions options) : m_options{options}
    {
        if(pipe(m_wake_pipe) != 0)
        {
            throw std::runtime_error{"OUI could not create its wake-up pipe."};
        }
        for(const auto descriptor : m_wake_pipe)
        {
            fcntl(descriptor, F_SETFL, fcntl(descriptor, F_GETFL) | O_NONBLOCK);
            fcntl(descriptor, F_SETFD, FD_CLOEXEC);
        }
        wake_write_fd = m_wake_pipe[1];
        m_previous_resize_handler = std::signal(SIGWINCH, on_terminal_resize);
        // Closing the terminal sends SIGHUP and a system shutdown SIGTERM; either would end the program on
        // the spot, before the application could keep anything.
        m_previous_hangup_handler = std::signal(SIGHUP, on_shutdown_signal);
        m_previous_terminate_handler = std::signal(SIGTERM, on_shutdown_signal);
        if(tcgetattr(STDIN_FILENO, &m_original_mode) != 0)
        {
            throw std::runtime_error{"OUI requires an interactive terminal."};
        }
        original_mode_for_crash = m_original_mode;
        terminal_in_use = 1;
        add_crash_cleanup(restore_terminal_after_crash);
        termios raw_mode{m_original_mode};
        // Every key reaches the application: no echo, no line editing, and no signals or flow
        // control stealing Ctrl+C, Ctrl+Z, Ctrl+S, or Ctrl+Q.
        raw_mode.c_iflag &= static_cast<tcflag_t>(~(BRKINT | ICRNL | INPCK | ISTRIP | IXON));
        raw_mode.c_lflag &= static_cast<tcflag_t>(~(ECHO | ICANON | IEXTEN | ISIG));
        raw_mode.c_cflag |= static_cast<tcflag_t>(CS8);
        raw_mode.c_cc[VMIN] = 0;
        raw_mode.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_mode);
        const char* const color_term{std::getenv("COLORTERM")};
        const char* const terminal{std::getenv("TERM")};
        m_font_glyphs = console_font_glyphs();
        // OUI_LINUX_CONSOLE=1 draws as on the Linux console in any terminal, to see what a program looks like
        // there without switching to one: 8 backgrounds, and a font without the half blocks. An empty font
        // is enough for that, since OUI only stands in for the characters it knows a console font lacks.
        if(const auto* const emulate{std::getenv("OUI_LINUX_CONSOLE")};
           !m_font_glyphs && emulate != nullptr && std::string_view{emulate} == "1")
        {
            m_font_glyphs.emplace();
        }
        m_colour_mode = terminal_helpers::colour_mode(color_term, terminal);
        // Any-motion SGR mouse reporting, modifyOtherKeys so Ctrl+Shift+letter is distinguishable, and focus
        // reports.
        std::fputs("\x1b[?1003h\x1b[?1006h\x1b[>4;1m\x1b[?1004h", stdout);
        if(m_options.use_alternate_screen)
        {
            std::fputs("\x1b[?1049h", stdout);
        }
        std::fputs("\x1b[?7l\x1b[?25l", stdout);
        std::fflush(stdout);
    }

    ~Impl()
    {
        std::fputs("\x1b[?1003l\x1b[?1006l\x1b[>4m\x1b[?1004l", stdout);
        std::fputs("\x1b[0m\x1b[?7h", stdout);
        if(m_options.use_alternate_screen)
        {
            std::fputs("\x1b[?1049l", stdout);
        }
        std::fputs("\x1b[?25h", stdout);
        std::fflush(stdout);
        std::signal(SIGWINCH, m_previous_resize_handler);
        std::signal(SIGHUP, m_previous_hangup_handler);
        std::signal(SIGTERM, m_previous_terminate_handler);
        wake_write_fd = -1;
        terminal_in_use = 0;
        close(m_wake_pipe[0]);
        close(m_wake_pipe[1]);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_original_mode);
    }

    // Sleeps until a key, a mouse report, a resize, a wake-up, or `deadline`. A lone Escape adds its own
    // deadline, to tell the Escape key from the start of a sequence.
    void wait_for_input(std::optional<std::chrono::steady_clock::time_point> deadline)
    {
        if(resize_pending != 0)
        {
            return;
        }
        if(m_decoder.has_pending())
        {
            const auto escape_deadline{m_last_input + escape_timeout};
            deadline = deadline ? std::min(*deadline, escape_deadline) : escape_deadline;
        }
        auto timeout{-1};
        if(deadline)
        {
            // Rounded up, so the wait never ends just short of the deadline and spins.
            const auto remaining{
                std::chrono::ceil<std::chrono::milliseconds>(*deadline - std::chrono::steady_clock::now())};
            timeout = static_cast<int>(
                std::clamp<std::chrono::milliseconds::rep>(remaining.count(), 0, std::numeric_limits<int>::max()));
        }
        pollfd descriptors[]{{STDIN_FILENO, POLLIN, 0}, {m_wake_pipe[0], POLLIN, 0}};
        if(poll(descriptors, 2, timeout) > 0 && (descriptors[1].revents & POLLIN) != 0)
        {
            char drained[64];
            while(read(m_wake_pipe[0], drained, sizeof(drained)) > 0)
            {
            }
        }
    }

    void wake() { write_wake_byte(m_wake_pipe[1]); }

    // OSC 52 asks the terminal to set the clipboard, which reaches the local desktop even over SSH.
    void set_clipboard_text(std::string_view text)
    {
        const auto sequence{osc52_clipboard(text)};
        std::fwrite(sequence.data(), 1, sequence.size(), stdout);
        std::fflush(stdout);
    }

    [[nodiscard]] Size size() const
    {
        winsize dimensions{};
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &dimensions);
        return {static_cast<double>(dimensions.ws_col), static_cast<double>(dimensions.ws_row)};
    }

    std::optional<Event> poll_event()
    {
        if(!m_shutdown_reported && (shutdown_signal != 0 || m_terminal_closed))
        {
            m_shutdown_reported = true;
            return ShutdownEvent{shutdown_signal == SIGTERM ? ShutdownEvent::Reason::terminated
                                                            : ShutdownEvent::Reason::terminal_closed};
        }
        if(resize_pending != 0)
        {
            resize_pending = 0;
            const auto dimensions{size()};
            return ResizeEvent{dimensions.width, dimensions.height};
        }
        if(auto event{m_decoder.next()})
        {
            return event;
        }
        read_input();
        if(auto event{m_decoder.next()})
        {
            return event;
        }
        if(m_decoder.has_pending() && std::chrono::steady_clock::now() - m_last_input >= escape_timeout)
        {
            m_decoder.flush();
            return m_decoder.next();
        }
        return std::nullopt;
    }

    void set_diagnose(bool enable) { m_diagnose = enable; }

    [[nodiscard]] std::string style_for(const Cell& cell) const
    {
        if(m_font_glyphs)
        {
            // The Linux console takes 24-bit colours but shows them with its 16, the same way.
            return console_style(cell);
        }
        switch(m_colour_mode)
        {
        case terminal_helpers::ColourMode::true_colour:
            return style(cell);
        case terminal_helpers::ColourMode::palette_256:
            return palette_style(cell);
        default:
            return reduced_style(cell);
        }
    }

    void present(const CellBuffer& buffer)
    {
        std::string output;
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
                const auto current_style{style_for(cell)};
                output += current_style;
                do
                {
                    const auto& run_cell{buffer.cell_at(x, y)};
                    if(!run_cell.continuation)
                    {
                        output += utf8(m_font_glyphs ? glyph_for_font(run_cell.glyph, *m_font_glyphs) : run_cell.glyph);
                    }
                    ++ x;
                    if(x == buffer.width())
                    {
                        break;
                    }
                    const auto& next{buffer.cell_at(x, y)};
                    const auto next_changed{!m_previous || m_previous->width() != buffer.width() ||
                                            m_previous->height() != buffer.height() ||
                                            m_previous->cell_at(x, y) != next};
                    if(!next_changed || style_for(next) != current_style)
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
        m_previous = buffer;
    }

    void reset() { m_previous.reset(); }

  private:
    // How long a lone ESC waits for the rest of a sequence before it counts as the Escape key.
    static constexpr std::chrono::milliseconds escape_timeout{30};

    void read_input()
    {
        pollfd descriptor{STDIN_FILENO, POLLIN, 0};
        if(poll(&descriptor, 1, 0) <= 0)
        {
            return;
        }
        char input[4096];
        const auto count{read(STDIN_FILENO, input, sizeof(input))};
        // A terminal that reports input and then has none, or fails with EIO, has gone away, even when no
        // SIGHUP arrives.
        if(count == 0 || (count < 0 && errno == EIO))
        {
            m_terminal_closed = true;
            return;
        }
        if(count < 0)
        {
            return;
        }
        const std::string_view bytes{input, static_cast<std::size_t>(count)};
        if(m_diagnose)
        {
            for(const auto byte : bytes)
            {
                std::fprintf(stderr, "%02x ", static_cast<unsigned char>(byte));
            }
            std::fputc('\n', stderr);
        }
        m_decoder.feed(bytes);
        m_last_input = std::chrono::steady_clock::now();
    }

    int m_wake_pipe[2]{-1, -1};
    TerminalInputDecoder m_decoder;
    std::chrono::steady_clock::time_point m_last_input;
    bool m_diagnose{false};
    std::optional<CellBuffer> m_previous;
    TerminalOptions m_options;
    terminal_helpers::ColourMode m_colour_mode{terminal_helpers::ColourMode::true_colour};
    // Set on the Linux console, or when emulating it: its font lacks some characters OUI draws, and it has
    // only 8 backgrounds.
    std::optional<std::unordered_set<char32_t>> m_font_glyphs;
    termios m_original_mode{};
    void (*m_previous_resize_handler)(int){SIG_DFL};
    void (*m_previous_hangup_handler)(int){SIG_DFL};
    void (*m_previous_terminate_handler)(int){SIG_DFL};
    bool m_terminal_closed{false};
    bool m_shutdown_reported{false};
};

AnsiTerminalBackend::AnsiTerminalBackend(TerminalOptions options) : m_impl{std::make_unique<Impl>(options)}
{
}
AnsiTerminalBackend::~AnsiTerminalBackend() = default;
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

std::unique_ptr<AnsiTerminalBackend> terminal_backend(TerminalOptions options)
{
    return std::make_unique<AnsiTerminalBackend>(options);
}

} // namespace olagarro::oui
