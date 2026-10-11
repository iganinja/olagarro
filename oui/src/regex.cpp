#include <oui/regex.h>

#include <oui/utf8.h>

#include <libregexp.h>
#include <quickjs.h>

#include <stdexcept>

namespace olagarro::oui
{

namespace
{

// The engine's regular expressions allocate through a context: a bare one for each thread that compiles or
// searches, as a runtime serves one thread at a time. It lives as long as the thread.
class RegexRuntime
{
  public:
    RegexRuntime() : m_runtime{JS_NewRuntime()}, m_context{JS_NewContextRaw(m_runtime)}
    {
        // Patterns run deep on whatever thread asks, so the runtime does not measure the stack.
        JS_SetMaxStackSize(m_runtime, 0);
    }
    ~RegexRuntime()
    {
        JS_FreeContext(m_context);
        JS_FreeRuntime(m_runtime);
    }
    RegexRuntime(const RegexRuntime&) = delete;
    RegexRuntime& operator=(const RegexRuntime&) = delete;

    [[nodiscard]] JSContext* context() const { return m_context; }

  private:
    JSRuntime* m_runtime;
    JSContext* m_context;
};

[[nodiscard]] JSContext* this_threads_context()
{
    thread_local RegexRuntime runtime;
    return runtime.context();
}

[[nodiscard]] int flags_from(std::string_view flags)
{
    int result{};
    for(const auto flag : flags)
    {
        switch(flag)
        {
        case 'i':
            result |= LRE_FLAG_IGNORECASE;
            break;
        case 'm':
            result |= LRE_FLAG_MULTILINE;
            break;
        case 's':
            result |= LRE_FLAG_DOTALL;
            break;
        case 'u':
            result |= LRE_FLAG_UNICODE;
            break;
        case 'v':
            result |= LRE_FLAG_UNICODE_SETS;
            break;
        case 'y':
            result |= LRE_FLAG_STICKY;
            break;
        case 'g':
        case 'd':
            break;
        default:
            throw std::invalid_argument{"unknown regular expression flag '" + std::string(1, flag) + "'"};
        }
    }
    return result;
}

} // namespace

Utf16Text::Utf16Text(std::string_view text)
{
    units.reserve(text.size());
    byte_offsets.reserve(text.size() + 1);
    std::size_t index{};
    while(index < text.size())
    {
        const auto start{index};
        const auto codepoint{decode_utf8(text, index)};
        if(codepoint >= 0x10000)
        {
            units.push_back(static_cast<char16_t>(0xD800 + ((codepoint - 0x10000) >> 10)));
            units.push_back(static_cast<char16_t>(0xDC00 + ((codepoint - 0x10000) & 0x3FF)));
            byte_offsets.push_back(start);
            byte_offsets.push_back(start);
        }
        else
        {
            units.push_back(static_cast<char16_t>(codepoint));
            byte_offsets.push_back(start);
        }
    }
    byte_offsets.push_back(text.size());
}

Regex::Regex(std::string_view pattern, std::string_view flags)
{
    const std::string source{pattern};
    char error[128]{};
    int length{};
    auto* const bytecode{lre_compile(&length, error, sizeof(error), source.c_str(), source.size(), flags_from(flags),
                                     this_threads_context())};
    if(bytecode == nullptr)
    {
        throw std::invalid_argument{"/" + source + "/: " + error};
    }
    m_bytecode.assign(bytecode, bytecode + length);
    lre_realloc(this_threads_context(), bytecode, 0);
}

std::optional<std::pair<std::size_t, std::size_t>> Regex::search(const std::u16string& text, std::size_t from) const
{
    if(from > text.size())
    {
        return std::nullopt;
    }
    std::vector<std::uint8_t*> captures(static_cast<std::size_t>(lre_get_alloc_count(m_bytecode.data())));
    const auto* const units{reinterpret_cast<const std::uint8_t*>(text.data())};
    const auto found{lre_exec(captures.data(), m_bytecode.data(), units, static_cast<int>(from),
                              static_cast<int>(text.size()), 1, this_threads_context())};
    if(found != 1 || captures[0] == nullptr || captures[1] == nullptr)
    {
        return std::nullopt;
    }
    return std::pair{static_cast<std::size_t>(captures[0] - units) / 2,
                     static_cast<std::size_t>(captures[1] - units) / 2};
}

} // namespace olagarro::oui
