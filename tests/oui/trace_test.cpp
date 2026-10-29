#include <oui/application.h>
#include <oui/button.h>
#include <oui/graphics/pixel_backend.h>
#include <oui/layout.h>
#include <oui/trace.h>
#include <oui/window.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using namespace olagarro;
using namespace olagarro::oui;

// Each span as the path of names from the outermost one open on its thread, "oui.frame/oui.render", as it begins.
class RecordingSink : public TraceSink
{
  public:
    void begin(std::string_view name) override
    {
        auto& open{open_paths()};
        open.push_back(open.empty() ? std::string{name} : open.back() + "/" + std::string{name});
        const std::lock_guard lock{m_mutex};
        m_paths.push_back(open.back());
    }
    void end() override
    {
        open_paths().pop_back();
        const std::lock_guard lock{m_mutex};
        ++ m_ended;
    }

    [[nodiscard]] bool has(std::string_view path) const
    {
        const std::lock_guard lock{m_mutex};
        return std::find(m_paths.begin(), m_paths.end(), path) != m_paths.end();
    }
    // Every path, one a line, for a failure to show.
    [[nodiscard]] std::string listed() const
    {
        const std::lock_guard lock{m_mutex};
        std::string text;
        for(const auto& path : m_paths)
        {
            text += path + "\n";
        }
        return text;
    }
    [[nodiscard]] std::size_t begun() const
    {
        const std::lock_guard lock{m_mutex};
        return m_paths.size();
    }
    [[nodiscard]] std::size_t ended() const
    {
        const std::lock_guard lock{m_mutex};
        return m_ended;
    }

  private:
    static std::vector<std::string>& open_paths()
    {
        thread_local std::vector<std::string> open;
        return open;
    }

    mutable std::mutex m_mutex;
    std::vector<std::string> m_paths;
    std::size_t m_ended{0};
};

TEST(OuiTraceTests, AScopeWithoutASinkTellsNothing)
{
    ASSERT_EQ(trace_sink(), nullptr);
    RecordingSink sink;
    {
        const TraceScope span{"before"};
    }
    set_trace_sink(&sink);
    {
        const TraceScope span{"while"};
    }
    set_trace_sink(nullptr);
    {
        const TraceScope span{"after"};
    }
    EXPECT_EQ(sink.begun(), 1u);
    EXPECT_EQ(sink.ended(), 1u);
    EXPECT_TRUE(sink.has("while"));
}

TEST(OuiTraceTests, AFrameReportsItsStagesNested)
{
    auto backend{image_backend({480, 240})};
    Application application{std::move(backend)};
    auto window{oui::window("Traced")};
    auto content{column()};
    button("Press", *content, [] {});
    window->add(std::move(content));
    window->set_id("main");
    application.set_window(std::move(window));
    RecordingSink sink;
    set_trace_sink(&sink);
    application.run_frame();
    set_trace_sink(nullptr);
    EXPECT_EQ(sink.begun(), sink.ended());
    EXPECT_TRUE(sink.has("oui.frame"));
    EXPECT_TRUE(sink.has("oui.frame/oui.events"));
    EXPECT_TRUE(sink.has("oui.frame/oui.render/oui.layout"));
    EXPECT_TRUE(sink.has("oui.frame/oui.render/oui.paint"));
    // The window by its id, what it holds by its type.
    EXPECT_TRUE(sink.has("oui.frame/oui.render/oui.paint/main"));
    EXPECT_TRUE(sink.has("oui.frame/oui.render/oui.paint/main/Column/Button")) << sink.listed();
    EXPECT_TRUE(sink.has("oui.frame/oui.render/oui.present"));
}

} // namespace
