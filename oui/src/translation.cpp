#include <oui/translation.h>

#include <oui/utf8.h>

#include <cstdint>
#include <stdexcept>
#include <utility>

namespace olagarro::oui
{

// Defined in the source CMake generates from oui/translations.
std::vector<std::string_view> embedded_oui_catalogs();

namespace
{

// Reads the part of JSON a catalog uses: objects whose values are strings or further objects.
class CatalogReader
{
  public:
    explicit CatalogReader(std::string_view json) : m_json{json} {}

    std::optional<TranslationCatalog> read(std::string& error)
    {
        TranslationCatalog catalog;
        bool has_language{false};
        const auto ok{read_object(
            [this, &catalog, &has_language](const std::string& key)
            {
                if(key == "language")
                {
                    has_language = true;
                    return read_string(catalog.language);
                }
                if(key == "name")
                {
                    return read_string(catalog.name);
                }
                if(key == "contexts")
                {
                    return read_texts(catalog.contexts);
                }
                return fail("unknown key \"" + key + "\"");
            })};
        if(ok)
        {
            skip_space();
            if(m_position != m_json.size())
            {
                static_cast<void>(fail("text after the catalog"));
            }
            else if(!has_language || catalog.language.empty())
            {
                static_cast<void>(fail("the catalog does not say its \"language\""));
            }
        }
        if(!m_error.empty())
        {
            error = m_error;
            return std::nullopt;
        }
        return catalog;
    }

  private:
    bool read_texts(TranslationTexts& texts)
    {
        return read_object(
            [this, &texts](const std::string& context)
            {
                auto& entries{texts[context]};
                return read_object(
                    [this, &entries](const std::string& text)
                    {
                        std::string translation;
                        if(!read_string(translation))
                        {
                            return false;
                        }
                        entries.insert_or_assign(text, std::move(translation));
                        return true;
                    });
            });
    }

    template <typename ReadValue> bool read_object(const ReadValue& read_value)
    {
        skip_space();
        if(!consume('{'))
        {
            return fail("expected an object");
        }
        skip_space();
        if(consume('}'))
        {
            return true;
        }
        while(true)
        {
            std::string key;
            skip_space();
            if(!read_string(key))
            {
                return false;
            }
            skip_space();
            if(!consume(':'))
            {
                return fail("expected ':'");
            }
            if(!read_value(key))
            {
                return false;
            }
            skip_space();
            if(consume('}'))
            {
                return true;
            }
            if(!consume(','))
            {
                return fail("expected ',' or '}'");
            }
        }
    }

    bool read_string(std::string& result)
    {
        skip_space();
        if(!consume('"'))
        {
            return fail("expected a string");
        }
        while(m_position < m_json.size())
        {
            const auto c{m_json[m_position++]};
            if(c == '"')
            {
                return true;
            }
            if(static_cast<unsigned char>(c) < 0x20)
            {
                return fail("a control character inside a string");
            }
            if(c != '\\')
            {
                result += c;
                continue;
            }
            if(m_position == m_json.size())
            {
                break;
            }
            switch(m_json[m_position++])
            {
            case '"':
                result += '"';
                break;
            case '\\':
                result += '\\';
                break;
            case '/':
                result += '/';
                break;
            case 'b':
                result += '\b';
                break;
            case 'f':
                result += '\f';
                break;
            case 'n':
                result += '\n';
                break;
            case 'r':
                result += '\r';
                break;
            case 't':
                result += '\t';
                break;
            case 'u':
            {
                char32_t code_point{};
                if(!read_hex4(code_point))
                {
                    return false;
                }
                if(code_point >= 0xD800 && code_point < 0xDC00)
                {
                    char32_t low{};
                    if(!consume('\\') || !consume('u') || !read_hex4(low) || low < 0xDC00 || low >= 0xE000)
                    {
                        return fail("a lone UTF-16 surrogate");
                    }
                    code_point = 0x10000 + ((code_point - 0xD800) << 10) + (low - 0xDC00);
                }
                else if(code_point >= 0xDC00 && code_point < 0xE000)
                {
                    return fail("a lone UTF-16 surrogate");
                }
                result += encode_utf8(code_point);
                break;
            }
            default:
                // Points the error at the backslash.
                m_position -= 2;
                return fail("an unknown escape");
            }
        }
        return fail("a string that never ends");
    }

    bool read_hex4(char32_t& value)
    {
        if(m_json.size() - m_position < 4)
        {
            return fail("a \\u escape needs four hex digits");
        }
        for(int i{}; i < 4; ++ i)
        {
            const auto c{m_json[m_position++]};
            value <<= 4;
            if(c >= '0' && c <= '9')
            {
                value |= static_cast<char32_t>(c - '0');
            }
            else if(c >= 'a' && c <= 'f')
            {
                value |= static_cast<char32_t>(c - 'a' + 10);
            }
            else if(c >= 'A' && c <= 'F')
            {
                value |= static_cast<char32_t>(c - 'A' + 10);
            }
            else
            {
                return fail("a \\u escape needs four hex digits");
            }
        }
        return true;
    }

    bool consume(char c)
    {
        if(m_position < m_json.size() && m_json[m_position] == c)
        {
            ++ m_position;
            return true;
        }
        return false;
    }

    void skip_space()
    {
        while(m_position < m_json.size() && (m_json[m_position] == ' ' || m_json[m_position] == '\t' ||
                                             m_json[m_position] == '\n' || m_json[m_position] == '\r'))
        {
            ++ m_position;
        }
    }

    // Records the first error with its line and column, and returns false for the caller to pass up.
    bool fail(const std::string& message)
    {
        if(m_error.empty())
        {
            std::size_t line{1};
            std::size_t column{1};
            for(std::size_t i{}; i < m_position && i < m_json.size(); ++ i)
            {
                if(m_json[i] == '\n')
                {
                    ++ line;
                    column = 1;
                }
                else
                {
                    ++ column;
                }
            }
            m_error = "line " + std::to_string(line) + ", column " + std::to_string(column) + ": " + message;
        }
        return false;
    }

    std::string_view m_json;
    std::size_t m_position{};
    std::string m_error;
};

std::vector<Translation>& installed_translations()
{
    static std::vector<Translation> translations;
    return translations;
}

} // namespace

std::optional<TranslationCatalog> parse_translation_catalog(std::string_view json, std::string& error)
{
    return CatalogReader{json}.read(error);
}

Translation::Translation(TranslationCatalog catalog) : m_catalog{std::move(catalog)}
{
}

const std::string& Translation::language() const
{
    return m_catalog.language;
}

const std::string& Translation::name() const
{
    return m_catalog.name;
}

const std::string* Translation::find(std::string_view context, std::string_view text) const
{
    const auto entries{m_catalog.contexts.find(context)};
    if(entries == m_catalog.contexts.end())
    {
        return nullptr;
    }
    const auto entry{entries->second.find(text)};
    if(entry == entries->second.end() || entry->second.empty())
    {
        return nullptr;
    }
    return &entry->second;
}

const std::string* Translation::source_of(std::string_view context, std::string_view translated) const
{
    const auto entries{m_catalog.contexts.find(context)};
    if(entries == m_catalog.contexts.end())
    {
        return nullptr;
    }
    for(const auto& [source, translation] : entries->second)
    {
        if(!translation.empty() && translation == translated)
        {
            return &source;
        }
    }
    return nullptr;
}

void install_translation(Translation translation)
{
    installed_translations().push_back(std::move(translation));
}

void remove_translations()
{
    installed_translations().clear();
}

std::string installed_language()
{
    const auto& installed{installed_translations()};
    return installed.empty() ? "en" : installed.back().language();
}

std::string translate(std::string_view context, std::string_view text)
{
    const auto& translations{installed_translations()};
    for(auto translation{translations.rbegin()}; translation != translations.rend(); ++ translation)
    {
        if(const auto* const found{translation->find(context, text)})
        {
            return *found;
        }
    }
    return std::string{text};
}

std::string untranslate(std::string_view context, std::string_view text)
{
    const auto& translations{installed_translations()};
    for(auto translation{translations.rbegin()}; translation != translations.rend(); ++ translation)
    {
        if(const auto* const found{translation->source_of(context, text)})
        {
            return *found;
        }
    }
    return std::string{text};
}

std::string_view text_for_language(const std::vector<std::string_view>& texts,
                                   const std::vector<std::string_view>& names, std::string_view language)
{
    if(texts.empty() || names.empty())
    {
        return {};
    }
    const auto english{names.front()};
    const auto dot{english.rfind('.')};
    const auto wanted{std::string{english.substr(0, dot)} + "." + std::string{language} +
                      std::string{dot == std::string_view::npos ? std::string_view{} : english.substr(dot)}};
    for(std::size_t index{1}; index < names.size() && index < texts.size(); ++ index)
    {
        if(names[index] == wanted)
        {
            return texts[index];
        }
    }
    return texts.front();
}

std::string format_text(std::string_view pattern, std::initializer_list<std::string_view> arguments)
{
    std::string result;
    std::size_t position{};
    while(position < pattern.size())
    {
        const auto open{pattern.find('{', position)};
        if(open == std::string_view::npos)
        {
            break;
        }
        const auto close{pattern.find('}', open)};
        if(close == std::string_view::npos)
        {
            break;
        }
        const auto digits{pattern.substr(open + 1, close - open - 1)};
        std::size_t index{};
        bool is_number{!digits.empty() && digits.size() <= 2};
        for(const auto c : digits)
        {
            is_number = is_number && c >= '0' && c <= '9';
            index = index * 10 + static_cast<std::size_t>(c - '0');
        }
        result.append(pattern.substr(position, open - position));
        if(is_number && index < arguments.size())
        {
            result.append(*(arguments.begin() + index));
        }
        else
        {
            result.append(pattern.substr(open, close - open + 1));
        }
        position = close + 1;
    }
    result.append(pattern.substr(position));
    return result;
}

std::vector<Translation> embedded_translations(const std::vector<std::string_view>& catalogs)
{
    std::vector<Translation> translations;
    for(std::size_t i{}; i < catalogs.size(); ++ i)
    {
        std::string error;
        auto catalog{parse_translation_catalog(catalogs[i], error)};
        if(!catalog)
        {
            throw std::runtime_error{"built-in translation catalog " + std::to_string(i) + " is broken: " + error};
        }
        translations.emplace_back(std::move(*catalog));
    }
    return translations;
}

const std::vector<Translation>& oui_translations()
{
    static const std::vector<Translation> translations{embedded_translations(embedded_oui_catalogs())};
    return translations;
}

void install_language(std::string_view language, const std::vector<Translation>& application_translations)
{
    remove_translations();
    for(const auto* const translations : {&oui_translations(), &application_translations})
    {
        for(const auto& translation : *translations)
        {
            if(translation.language() == language)
            {
                install_translation(translation);
            }
        }
    }
}

} // namespace olagarro::oui
