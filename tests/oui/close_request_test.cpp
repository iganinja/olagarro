#include <oui/application.h>

#include <gtest/gtest.h>

#include <deque>
#include <memory>
#include <optional>
#include <utility>

using namespace olagarro;

namespace
{

// A backend that hands over the events queued in it, one at a time.
class QueueBackend final : public oui::Backend
{
  public:
    explicit QueueBackend(std::shared_ptr<std::deque<oui::Event>> events) : m_events{std::move(events)} {}
    [[nodiscard]] oui::Size size() const override { return {80, 24}; }
    std::optional<oui::Event> poll_event() override
    {
        if(m_events->empty())
        {
            return std::nullopt;
        }
        auto event{m_events->front()};
        m_events->pop_front();
        return event;
    }
    void present(const oui::CellBuffer&) override {}

    std::shared_ptr<std::deque<oui::Event>> m_events;
};

} // namespace

// Closing a window can come as two requests at once, as SDL's for the window and for the program: a program that asks
// about unsaved work, running frames while it asks, asked twice, one question over the other.
TEST(OuiCloseRequestTests, ARequestWhileOneIsAnsweredIsTheSameRequest)
{
    const auto events{std::make_shared<std::deque<oui::Event>>()};
    oui::Application application{std::make_unique<QueueBackend>(events)};
    auto asked{0};
    application.on_close_requested(
        [&]
        {
            ++ asked;
            // As a question shown in a modal window does, until it is answered.
            application.run_frame();
            application.run_frame();
        });
    events->push_back(oui::CloseRequestEvent{});
    events->push_back(oui::CloseRequestEvent{});
    application.run_frame();
    application.run_frame();
    EXPECT_EQ(asked, 1);

    // Once answered, the next request is asked about again.
    events->push_back(oui::CloseRequestEvent{});
    application.run_frame();
    EXPECT_EQ(asked, 2);
}
