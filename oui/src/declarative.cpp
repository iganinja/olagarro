#include <oui/declarative.h>

#include <algorithm>
#include <cctype>

namespace olagarro::oui
{

namespace
{

[[nodiscard]] bool is_type_start(char character)
{
    return std::isupper(static_cast<unsigned char>(character)) != 0;
}

[[nodiscard]] bool is_name_character(char character, bool first)
{
    const auto byte{static_cast<unsigned char>(character)};
    return std::islower(byte) != 0 || character == '_' || (!first && (std::isdigit(byte) != 0 || character == '-'));
}

// Reads what a line declares, after its indentation: the widget's type, its text and its properties.
class LineReader
{
  public:
    LineReader(std::string_view text, std::size_t line, std::size_t column)
        : m_text{text}, m_line{line}, m_column{column}
    {
    }

    // The node, or the first thing wrong with the line.
    [[nodiscard]] std::optional<DeclarativeNode> read(std::vector<ParseDiagnostic>& diagnostics)
    {
        DeclarativeNode node;
        node.line = m_line;
        node.column = m_column;
        if(m_text.empty() || !is_type_start(m_text.front()))
        {
            return fail(diagnostics, 0, "Expected a widget type, as Label or Button.");
        }
        while(m_index < m_text.size() && std::isalnum(static_cast<unsigned char>(m_text[m_index])) != 0)
        {
            ++ m_index;
        }
        node.type = std::string{m_text.substr(0, m_index)};
        auto first{true};
        while(true)
        {
            const auto had_space{skip_spaces()};
            if(m_index >= m_text.size())
            {
                return node;
            }
            if(!had_space)
            {
                return fail(diagnostics, m_index, "Expected a space here.");
            }
            const auto start{m_index};
            if(m_text[m_index] == '"')
            {
                if(!first)
                {
                    return fail(diagnostics, start, "The text goes right after the type.");
                }
                auto text{quoted(diagnostics)};
                if(!text)
                {
                    return std::nullopt;
                }
                node.text = std::move(*text);
            }
            else
            {
                if(!is_name_character(m_text[m_index], true))
                {
                    return fail(diagnostics, start, "Expected a property, as name=value or a name alone.");
                }
                while(m_index < m_text.size() && is_name_character(m_text[m_index], false))
                {
                    ++ m_index;
                }
                std::string name{m_text.substr(start, m_index - start)};
                std::string value;
                if(m_index < m_text.size() && m_text[m_index] == '=')
                {
                    ++ m_index;
                    if(m_index < m_text.size() && m_text[m_index] == '"')
                    {
                        auto text{quoted(diagnostics)};
                        if(!text)
                        {
                            return std::nullopt;
                        }
                        value = std::move(*text);
                    }
                    else
                    {
                        const auto value_start{m_index};
                        while(m_index < m_text.size() && m_text[m_index] != ' ')
                        {
                            ++ m_index;
                        }
                        value = std::string{m_text.substr(value_start, m_index - value_start)};
                    }
                }
                if(node.has(name))
                {
                    return fail(diagnostics, start, "The property " + name + " is given twice.");
                }
                node.properties.emplace_back(std::move(name), std::move(value));
                node.property_columns.push_back(m_column + start);
            }
            first = false;
        }
    }

  private:
    // Whether there were any.
    bool skip_spaces()
    {
        const auto start{m_index};
        while(m_index < m_text.size() && m_text[m_index] == ' ')
        {
            ++ m_index;
        }
        return m_index > start;
    }

    // A text in double quotes at the reader's place, with \" and \\ in it.
    [[nodiscard]] std::optional<std::string> quoted(std::vector<ParseDiagnostic>& diagnostics)
    {
        const auto start{m_index};
        ++ m_index;
        std::string text;
        while(m_index < m_text.size())
        {
            const auto character{m_text[m_index++]};
            if(character == '"')
            {
                return text;
            }
            if(character == '\\' && m_index < m_text.size())
            {
                const auto escaped{m_text[m_index++]};
                text += escaped == 'n' ? '\n' : escaped;
                continue;
            }
            text += character;
        }
        fail(diagnostics, start, "A quote is not closed.");
        return std::nullopt;
    }

    std::nullopt_t fail(std::vector<ParseDiagnostic>& diagnostics, std::size_t at, std::string message) const
    {
        diagnostics.push_back({m_line, m_column + at, std::move(message)});
        return std::nullopt;
    }

    std::string_view m_text;
    std::size_t m_line;
    std::size_t m_column;
    std::size_t m_index{};
};

} // namespace

std::optional<std::string_view> DeclarativeNode::property(std::string_view name) const
{
    const auto found{std::find_if(properties.begin(), properties.end(),
                                  [name](const auto& property) { return property.first == name; })};
    return found == properties.end() ? std::nullopt : std::optional<std::string_view>{found->second};
}

bool DeclarativeNode::has(std::string_view name) const
{
    return property(name).has_value();
}

bool ParseResult::succeeded() const
{
    return root.has_value() && diagnostics.empty();
}

ParseResult parse_oui(std::string_view source)
{
    ParseResult result;
    // The nodes open, each with its indentation, from the root down.
    std::vector<std::pair<std::size_t, DeclarativeNode*>> stack;
    std::size_t offset{};
    std::size_t line_number{1};
    while(offset < source.size())
    {
        const auto line_end{source.find('\n', offset)};
        auto line{source.substr(offset, (line_end == std::string_view::npos ? source.size() : line_end) - offset)};
        if(!line.empty() && line.back() == '\r')
        {
            line.remove_suffix(1);
        }
        offset = line_end == std::string_view::npos ? source.size() : line_end + 1;
        const auto number{line_number++};
        std::size_t indent{};
        while(indent < line.size() && line[indent] == ' ')
        {
            ++ indent;
        }
        while(!line.empty() && line.back() == ' ')
        {
            line.remove_suffix(1);
        }
        if(indent >= line.size() || line[indent] == '#')
        {
            continue;
        }
        if(line[indent] == '\t')
        {
            result.diagnostics.push_back({number, indent + 1, "Tabs are not allowed for indentation."});
            continue;
        }
        auto node{LineReader{line.substr(indent), number, indent + 1}.read(result.diagnostics)};
        if(!node)
        {
            continue;
        }
        while(!stack.empty() && indent <= stack.back().first)
        {
            stack.pop_back();
        }
        if(stack.empty())
        {
            if(result.root)
            {
                result.diagnostics.push_back({number, indent + 1, "A document can only have one root widget."});
                continue;
            }
            result.root = std::move(*node);
            stack.emplace_back(indent, &*result.root);
        }
        else
        {
            auto& children{stack.back().second->children};
            children.push_back(std::move(*node));
            stack.emplace_back(indent, &children.back());
        }
    }
    if(!result.root && result.diagnostics.empty())
    {
        result.diagnostics.push_back({1, 1, "Expected a root widget."});
    }
    return result;
}

} // namespace olagarro::oui
