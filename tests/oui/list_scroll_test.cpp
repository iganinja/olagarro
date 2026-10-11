#include <oui/application.h>
#include <oui/backend.h>
#include <oui/button.h>
#include <oui/cell_buffer.h>
#include <oui/label.h>
#include <oui/list.h>
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

class ListScrollTest : public ::testing::Test
{
  protected:
    void SetUp() override { m_app = std::make_unique<oui::Application>(std::make_unique<FixedBackend>()); }

    void TearDown() override { m_app.reset(); }

    std::unique_ptr<oui::Application> m_app;
};

TEST_F(ListScrollTest, ScrollOffsetStartsAtZero)
{
    auto list{oui::list()};
    EXPECT_EQ(list->scroll_offset(), 0);
}

TEST_F(ListScrollTest, PageUpDownChangesScrollOffset)
{
    auto list{oui::list()};
    for(int i{0}; i < 20; ++ i)
    {
        list->add(oui::label("Item " + std::to_string(i)));
    }

    list->arrange({0, 0, 30, 10});
    list->handle_event(oui::Event{oui::KeyEvent{oui::Key::page_down}});
    EXPECT_GT(list->scroll_offset(), 0);

    list->handle_event(oui::Event{oui::KeyEvent{oui::Key::page_up}});
    EXPECT_LT(list->scroll_offset(), 20);
}

TEST_F(ListScrollTest, HomeEndControlsScroll)
{
    auto list{oui::list()};
    for(int i{0}; i < 20; ++ i)
    {
        list->add(oui::label("Item " + std::to_string(i)));
    }

    list->arrange({0, 0, 30, 10});
    list->handle_event(oui::Event{oui::KeyEvent{oui::Key::end}});
    EXPECT_GT(list->scroll_offset(), 0);

    list->handle_event(oui::Event{oui::KeyEvent{oui::Key::home}});
    EXPECT_EQ(list->scroll_offset(), 0);
}

TEST_F(ListScrollTest, HorizontalScrollDisabledByDefault)
{
    auto list{oui::list()};
    EXPECT_FALSE(list->horizontal_scroll_enabled());
    EXPECT_EQ(list->scroll_offset_x(), 0);
}

TEST_F(ListScrollTest, AListCutsLongRowsToItsWidthInsteadOfScrollingSideways)
{
    auto list{oui::list()};
    list->add_item("This is a very long label that exceeds the viewport width significantly");
    list->arrange({0, 0, 30, 10});
    EXPECT_FALSE(list->handle_event(oui::Event{oui::KeyEvent{oui::Key::right}}));
    EXPECT_EQ(list->scroll_offset_x(), 0);
}

TEST_F(ListScrollTest, ATableWiderThanItselfScrollsSidewaysWithLeftAndRight)
{
    auto table{oui::table()};
    table->set_column_width(0, 80);
    for(int row{}; row < 20; ++ row)
    {
        table->add_line({"Row " + std::to_string(row)});
    }
    table->arrange({0, 0, 30, 10});
    EXPECT_TRUE(table->horizontal_scroll_enabled());
    EXPECT_TRUE(table->handle_event(oui::Event{oui::KeyEvent{oui::Key::right}}));
    EXPECT_GT(table->scroll_offset_x(), 0);
    EXPECT_TRUE(table->handle_event(oui::Event{oui::KeyEvent{oui::Key::left}}));
    EXPECT_EQ(table->scroll_offset_x(), 0);
    // Home, End and the page keys still move through the rows.
    EXPECT_TRUE(table->handle_event(oui::Event{oui::KeyEvent{oui::Key::end}}));
    EXPECT_EQ(table->selected_row(), std::optional<std::size_t>{19});
    EXPECT_GT(table->scroll_offset(), 0);
}
