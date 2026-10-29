#pragma once

#include <string_view>

namespace olagarro::oui
{

class Widget;

// What hears the spans OUI opens around its own work (a frame, its layout, each widget's paint, the graphical
// painter's work and the present), so a program measuring itself sees OUI's stages among its own. Spans nest on the
// thread that opens them: each end() closes the span its thread began last.
class TraceSink
{
  public:
    virtual ~TraceSink() = default;

    // Called on the thread doing the work, from any thread at once: an implementation is safe to call so.
    virtual void begin(std::string_view name) = 0;
    virtual void end() = 0;
};

// Sets what hears OUI's spans, or none with nullptr, the default, when a span costs one relaxed atomic read. The sink
// is not owned; it lives until every span begun with it has ended, as one set for the program's whole run does.
void set_trace_sink(TraceSink* sink);
[[nodiscard]] TraceSink* trace_sink();

// A span from its construction to its destruction, told to the sink set when it began, if any.
class TraceScope
{
  public:
    explicit TraceScope(std::string_view name);
    // A widget's paint, named after its id, or its type when it has none.
    explicit TraceScope(const Widget& widget);
    ~TraceScope();

    TraceScope(const TraceScope&) = delete;
    TraceScope& operator=(const TraceScope&) = delete;
    TraceScope(TraceScope&&) = delete;
    TraceScope& operator=(TraceScope&&) = delete;

  private:
    TraceSink* m_sink;
};

} // namespace olagarro::oui
