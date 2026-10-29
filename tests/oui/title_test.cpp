#include <oui/application.h>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace olagarro;

namespace
{

// A backend that remembers each title it was given.
class TitleBackend final : public oui::Backend
{
  public:
    explicit TitleBackend(std::shared_ptr<std::vector<std::string>> titles) : m_titles{std::move(titles)} {}
    [[nodiscard]] oui::Size size() const override { return {80, 24}; }
    std::optional<oui::Event> poll_event() override { return std::nullopt; }
    void present(const oui::CellBuffer&) override {}
    void set_title(std::string_view title) override { m_titles->emplace_back(title); }

    std::shared_ptr<std::vector<std::string>> m_titles;
};

} // namespace

// A window's backend may redraw and show the whole window for a new title; a program that sets its title after each
// change, as one being dragged, made a maximized window wait on the screen for each mouse move with nothing new.
TEST(OuiTitleTests, TheSameTitleAgainReachesNoBackend)
{
    const auto titles{std::make_shared<std::vector<std::string>>()};
    oui::Application application{std::make_unique<TitleBackend>(titles)};
    application.set_title("project");
    application.set_title("project");
    application.set_title("project *");
    application.set_title("project *");
    application.set_title("project");
    EXPECT_EQ(*titles, (std::vector<std::string>{"project", "project *", "project"}));
}
