#include <oui/application.h>
#include <oui/backend.h>
#include <oui/cell_buffer.h>
#include <oui/label.h>
#include <oui/table.h>
#include <oui/raster_backend.h>
#include <oui/window.h>

#include <gtest/gtest.h>

#include <optional>
#include <vector>

using namespace olagarro;

namespace
{

class FixedBackend final : public oui::Backend
{
  public:
    [[nodiscard]] oui::Size size() const override { return {30, 10}; }
    std::optional<oui::Event> poll_event() override { return std::nullopt; }
    void present(const oui::CellBuffer&) override {}
};

} // namespace

class ListHeaderTest : public ::testing::Test
{
  protected:
    void SetUp() override { m_app = std::make_unique<oui::Application>(std::make_unique<FixedBackend>()); }

    void TearDown() override { m_app.reset(); }

    std::unique_ptr<oui::Application> m_app;
};

TEST_F(ListHeaderTest, HeadersShowOnceSetAndCanBeHidden)
{
    auto list{oui::table()};
    EXPECT_FALSE(list->headers_visible());
    list->set_headers({"Col1", "Col2"});
    EXPECT_TRUE(list->headers_visible());
    list->set_headers_visible(false);
    EXPECT_FALSE(list->headers_visible());
}

TEST_F(ListHeaderTest, HeaderIndicatorShowsForActiveSort)
{
    auto list{oui::table()};
    list->set_headers({"Name", "Value"});
    list->set_headers_visible(true);
    list->set_column_sort(0, oui::SortOrder::ascending);

    auto order{list->column_sort_order(0)};
    EXPECT_EQ(order, oui::SortOrder::ascending);
}

TEST_F(ListHeaderTest, HeaderClickEnabledFlag)
{
    auto list{oui::table()};
    EXPECT_FALSE(list->headers_clickable());
    list->set_headers_clickable(true);
    EXPECT_TRUE(list->headers_clickable());
}
