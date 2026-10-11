#include <oui/text_layout.h>
#include <oui/text_metrics.h>

#include <oui/utf8.h>

#include <algorithm>

namespace olagarro::oui
{

int text_width(std::string_view text)
{
    auto width{0};
    for(std::size_t index{}; index < text.size();)
    {
        width += std::max(0, terminal_cell_width(decode_utf8(text, index)));
    }
    return width;
}

namespace
{

// Breaks one line of text with no line breaks in it into lines that fit `room`, as text_metrics()
// measures: at the spaces between words, dropping the spaces a break falls on, and inside a word only when
// it does not fit a line by itself.
void wrap_paragraph(std::string_view text, int room, std::vector<std::string>& lines)
{
    const auto& metrics{text_metrics()};
    const auto fits{[&metrics, room](std::string_view candidate) { return metrics.width(candidate) <= room; }};
    std::string line;
    for(std::size_t index{}; index < text.size();)
    {
        // The next word with the blanks before it.
        const auto word_start{std::min(text.find_first_not_of(' ', index), text.size())};
        const auto word_end{std::min(text.find(' ', word_start), text.size())};
        const auto piece{text.substr(index, word_end - index)};
        const auto word{text.substr(word_start, word_end - word_start)};
        index = word_end;
        if(fits(line + std::string{piece}))
        {
            line += std::string{piece};
            continue;
        }
        if(!line.empty())
        {
            lines.push_back(std::move(line));
            line.clear();
        }
        // A word wider than a line goes in pieces, a character at a time.
        for(std::size_t at{}; at < word.size();)
        {
            const auto start{at};
            static_cast<void>(decode_utf8(word, at));
            const auto character{word.substr(start, at - start)};
            if(!line.empty() && !fits(line + std::string{character}))
            {
                lines.push_back(std::move(line));
                line.clear();
            }
            line += std::string{character};
        }
    }
    lines.push_back(std::move(line));
}

} // namespace

int shown_width(std::string_view text)
{
    if(text.empty())
    {
        return 0;
    }
    const auto& metrics{text_metrics()};
    const auto cell{std::max(1, metrics.cell_width())};
    return (metrics.width(text) + 2 * metrics.field_padding() + cell - 1) / cell;
}

std::vector<std::string> wrap_text(std::string_view text, int width)
{
    const auto& metrics{text_metrics()};
    const auto room{std::max(1, width) * std::max(1, metrics.cell_width())};
    std::vector<std::string> lines;
    for(std::size_t start{};;)
    {
        const auto end{std::min(text.find('\n', start), text.size())};
        wrap_paragraph(text.substr(start, end - start), room, lines);
        if(end == text.size())
        {
            break;
        }
        start = end + 1;
    }
    return lines;
}

std::string fit_text(std::string_view text, int width)
{
    // Measured as the text shows: in a proportional font more of it fits than it has characters; the padding is
    // what interface text keeps from a cell's edges.
    const auto& metrics{text_metrics()};
    const auto room{width * std::max(1, metrics.cell_width()) - 2 * metrics.field_padding()};
    if(metrics.width(text) <= room)
    {
        return std::string{text};
    }
    if(width <= 0)
    {
        return {};
    }
    const auto mark{metrics.width("~")};
    std::string result;
    for(std::size_t index{}; index < text.size();)
    {
        const auto start{index};
        static_cast<void>(decode_utf8(text, index));
        std::string longer{result};
        longer.append(text.substr(start, index - start));
        if(metrics.width(longer) + mark > room)
        {
            break;
        }
        result = std::move(longer);
    }
    return result + "~";
}

} // namespace olagarro::oui
