#pragma once

#include <oui/color.h>

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace olagarro::oui
{

// A colour a program running in a terminal asked for: the terminal's own, or one it named.
struct TerminalColor
{
    bool is_default{true};
    Color rgb{};
    // Which of the first eight colours it was named as, which bold makes bright, as xterm draws it; -1 for others.
    int base{-1};

    [[nodiscard]] bool operator==(const TerminalColor& other) const
    {
        return is_default == other.is_default && (is_default || rgb == other.rgb);
    }
};

// One place on a terminal's screen: the character there and how it is drawn.
struct TerminalCell
{
    char32_t glyph{U' '};
    TerminalColor foreground;
    TerminalColor background;
    bool bold{false};
    bool faint{false};
    bool italic{false};
    bool underline{false};
    bool inverse{false};
    // The right half of a wide character, as an ideograph, which the cell before draws.
    bool continuation{false};
};

// What a terminal shows: the text programs write to it, read as an xterm does, its escape sequences moving the
// cursor, erasing, scrolling and colouring; and the lines that scrolled off the top, to look back at. It is
// only the screen: a TerminalView joins it to a program and to the keyboard.
class TerminalScreen
{
  public:
    TerminalScreen(int columns, int rows);

    // Reads what the program wrote; a sequence or a character cut between two writes goes on in the next.
    void feed(std::string_view bytes);
    // A new size, as when the window changes: lines keep their start, the cursor stays on the screen.
    void resize(int columns, int rows);

    [[nodiscard]] int columns() const;
    [[nodiscard]] int rows() const;
    // A cell of the screen as shown `scrolled_back` lines up into what scrolled off; 0 is the screen itself.
    [[nodiscard]] const TerminalCell& cell(int column, int row, int scrolled_back = 0) const;
    // How many lines scrolled off the top and can be looked back at; none while a program uses the whole screen
    // for itself, as an editor does.
    [[nodiscard]] int history_size() const;
    [[nodiscard]] int cursor_column() const;
    [[nodiscard]] int cursor_row() const;
    [[nodiscard]] bool cursor_visible() const;
    // Whether the cursor keys send the program's own codes (ESC O) rather than the usual ones (ESC [).
    [[nodiscard]] bool application_cursor_keys() const;
    // Whether text pasted goes between the marks that tell a program it was pasted, not typed.
    [[nodiscard]] bool bracketed_paste() const;
    // What the program called its window, as a shell names the folder it is in.
    [[nodiscard]] const std::string& title() const;
    // What the terminal answers the program, as where its cursor is when asked; taken once.
    [[nodiscard]] std::string take_reply();
    // Changes with every change to what shows, to tell whether to draw again.
    [[nodiscard]] std::uint64_t revision() const;

  private:
    using Line = std::vector<TerminalCell>;

    enum class State
    {
        ground,
        escape,
        escape_intermediate,
        csi,
        osc,
        osc_escape,
        string,
        string_escape
    };

    void take(char32_t character);
    void control(char32_t character);
    void escape(char32_t final, char intermediate);
    void csi(char32_t final);
    void sgr();
    void private_mode(int mode, bool set);
    void print(char32_t character);
    void line_feed();
    void reverse_line_feed();
    void scroll_up(int count);
    void scroll_down(int count);
    void erase_display(int how);
    void erase_line(int how);
    void insert_lines(int count);
    void delete_lines(int count);
    void insert_characters(int count);
    void delete_characters(int count);
    void erase_characters(int count);
    void move_to(int column, int row);
    void use_alternate(bool alternate);
    void reset();
    void save_cursor();
    void restore_cursor();
    [[nodiscard]] TerminalCell blank() const;
    [[nodiscard]] int parameter(std::size_t index, int fallback) const;
    [[nodiscard]] Line blank_line() const;

    int m_columns;
    int m_rows;
    std::vector<Line> m_lines;
    std::vector<Line> m_other_lines;
    std::deque<Line> m_history;
    bool m_alternate{false};
    int m_cursor_column{0};
    int m_cursor_row{0};
    // At the last column after writing there: the next character goes to the next line first.
    bool m_wrap_pending{false};
    int m_top{0};
    int m_bottom{0};
    TerminalCell m_pen;
    struct Saved
    {
        int column{0};
        int row{0};
        TerminalCell pen;
    } m_saved, m_other_saved;
    bool m_cursor_visible{true};
    bool m_application_cursor_keys{false};
    bool m_bracketed_paste{false};
    bool m_autowrap{true};
    std::string m_title;
    std::string m_reply;
    std::uint64_t m_revision{0};

    State m_state{State::ground};
    std::string m_parameters;
    char m_intermediate{0};
    bool m_private{false};
    std::string m_string;
    // The bytes of a UTF-8 character still to come, and what came of it so far.
    int m_utf8_left{0};
    char32_t m_utf8_value{0};
};

} // namespace olagarro::oui
