#include <oui/trace.h>

#include <oui/widget.h>

#include <atomic>
#include <cstddef>
#include <optional>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

namespace olagarro::oui
{

namespace
{

std::atomic<TraceSink*> g_sink{nullptr};

// The last of the names in a type's name as the Itanium C++ ABI mangles it, "N8olagarro3oui6ButtonE" or "6Button",
// read here as OUI keeps the runtime's demangler out (application.cpp); nothing for what it does not read so, as a
// template.
std::optional<std::string> last_mangled_name(std::string_view mangled)
{
    if(mangled.size() >= 2 && mangled.front() == 'N' && mangled.back() == 'E')
    {
        mangled = mangled.substr(1, mangled.size() - 2);
    }
    std::optional<std::string> last;
    while(!mangled.empty())
    {
        std::size_t length{0};
        std::size_t digits{0};
        while(digits < mangled.size() && mangled[digits] >= '0' && mangled[digits] <= '9')
        {
            length = length * 10 + static_cast<std::size_t>(mangled[digits] - '0');
            ++ digits;
        }
        if(digits == 0 || length == 0 || digits + length > mangled.size())
        {
            return std::nullopt;
        }
        last = std::string{mangled.substr(digits, length)};
        mangled.remove_prefix(digits + length);
    }
    return last;
}

// A type's name without its namespaces: "Button", not "olagarro::oui::Button"; as the compiler gives it when that
// cannot be read.
std::string readable_type_name(const std::type_info& type)
{
    std::string_view name{type.name()};
    if(const auto last{last_mangled_name(name)})
    {
        return *last;
    }
    // MSVC names it as written, "class olagarro::oui::Button".
    for(const std::string_view kind : {"class ", "struct "})
    {
        if(name.substr(0, kind.size()) == kind)
        {
            name.remove_prefix(kind.size());
        }
    }
    if(name.find('<') == std::string_view::npos)
    {
        if(const auto colons{name.rfind("::")}; colons != std::string_view::npos)
        {
            name.remove_prefix(colons + 2);
        }
    }
    return std::string{name};
}

// Each type's name made once a thread, as a widget's paint is traced every frame.
std::string_view type_name(const Widget& widget)
{
    thread_local std::unordered_map<std::type_index, std::string> names;
    const std::type_index type{typeid(widget)};
    auto found{names.find(type)};
    if(found == names.end())
    {
        found = names.emplace(type, readable_type_name(typeid(widget))).first;
    }
    return found->second;
}

} // namespace

void set_trace_sink(TraceSink* sink)
{
    g_sink.store(sink, std::memory_order_release);
}

TraceSink* trace_sink()
{
    return g_sink.load(std::memory_order_acquire);
}

TraceScope::TraceScope(std::string_view name) : m_sink{g_sink.load(std::memory_order_acquire)}
{
    if(m_sink != nullptr)
    {
        m_sink->begin(name);
    }
}

TraceScope::TraceScope(const Widget& widget) : m_sink{g_sink.load(std::memory_order_acquire)}
{
    if(m_sink != nullptr)
    {
        m_sink->begin(widget.id().empty() ? type_name(widget) : widget.id());
    }
}

TraceScope::~TraceScope()
{
    if(m_sink != nullptr)
    {
        m_sink->end();
    }
}

} // namespace olagarro::oui
