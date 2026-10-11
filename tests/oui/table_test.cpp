#include <oui/application.h>
#include <oui/backend.h>
#include <oui/button.h>
#include <oui/cell_buffer.h>
#include <oui/label.h>
#include <oui/list.h>
#include <oui/raster_backend.h>
#include <oui/table.h>
#include <oui/type_ahead.h>
#include <oui/window.h>

#include <gtest/gtest.h>

#include <chrono>

#include <optional>
#include <vector>

using namespace olagarro;

using oui::table;

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

class TableTest : public ::testing::Test
{
  protected:
    void SetUp() override { m_app = std::make_unique<oui::Application>(std::make_unique<FixedBackend>()); }

    void TearDown() override { m_app.reset(); }

    std::unique_ptr<oui::Application> m_app;
};

TEST_F(TableTest, CreateTableWithHeaders)
{
    auto table_widget{table({"Name", "Value"})};
    EXPECT_TRUE(table_widget->headers_visible());
    EXPECT_EQ(table_widget->column_count(), 2);
}

TEST_F(TableTest, AddLineCreatesLabels)
{
    auto table_widget{table({"A", "B"})};
    table_widget->add_line({"val1", "val2"});

    EXPECT_EQ(table_widget->row_count(), 1);
    auto* widget = table_widget->cell_widget(0, 0);
    auto* label = dynamic_cast<const oui::Label*>(widget);
    ASSERT_NE(label, nullptr);
    EXPECT_STREQ(label->text().data(), "val1");
}

TEST_F(TableTest, SetCellWidgetReplacesLabel)
{
    auto table_widget{table({"A", "B"})};
    table_widget->add_line({"text", "data"});

    auto btn{oui::button("Click")};
    table_widget->set_cell_widget(0, 0, std::move(btn));

    auto* widget = table_widget->cell_widget(0, 0);
    auto* button = dynamic_cast<const oui::Button*>(widget);
    ASSERT_NE(button, nullptr);
}

TEST_F(TableTest, ColumnWidthsSetCorrectly)
{
    auto table_widget{table({"Name", "Value"})};
    table_widget->set_column_width(0, 20);
    table_widget->set_column_width(1, 10);

    EXPECT_EQ(table_widget->column_width(0), 20);
    EXPECT_EQ(table_widget->column_width(1), 10);
    EXPECT_EQ(table_widget->column_width(5), 4);
}

TEST_F(TableTest, TableMetadataWorks)
{
    auto table_widget{table({"Name", "Value"})};
    table_widget->add_line({"item1", "100"});

    table_widget->set_metadata(0, 1, "num_value", 42);

    auto* val = table_widget->metadata(0, 1, "num_value");
    ASSERT_NE(val, nullptr);
    ASSERT_TRUE(std::holds_alternative<int>(*val));
    EXPECT_EQ(std::get<int>(*val), 42);
}

TEST_F(TableTest, ClearRemovesAllChildren)
{
    auto table_widget{table({"A", "B"})};
    table_widget->add_line({"x", "y"});
    table_widget->add_line({"z", "w"});

    EXPECT_EQ(table_widget->row_count(), 2);
    table_widget->clear();
    EXPECT_EQ(table_widget->row_count(), 0);
}

TEST_F(TableTest, ClearAlsoRemovesMetadata)
{
    auto table_widget{table({"A", "B"})};
    table_widget->add_line({"x", "y"});
    table_widget->set_metadata(0, 1, "value", 42);

    table_widget->clear();

    EXPECT_EQ(table_widget->metadata(0, 1, "value"), nullptr);
}

TEST_F(TableTest, HorizontalScrollEnabledWhenContentExceeds)
{
    auto table_widget{table({"Name", "Value"})};
    table_widget->set_column_width(0, 20);
    table_widget->set_column_width(1, 20);
    table_widget->add_line({"short", "val"});

    table_widget->arrange({0, 0, 15, 10});
    EXPECT_TRUE(table_widget->horizontal_scroll_enabled());
}

TEST_F(TableTest, HorizontalScrollDisabledWhenContentFits)
{
    auto table_widget{table({"A", "B"})};
    table_widget->add_line({"x", "y"});

    table_widget->arrange({0, 0, 30, 10});
    EXPECT_FALSE(table_widget->horizontal_scroll_enabled());
}

TEST_F(TableTest, SelectsCompleteRowsWithKeyboardNavigation)
{
    auto table_widget{table({"Name", "Value"})};
    table_widget->add_line({"first", "1"});
    table_widget->add_line({"second", "2"});

    table_widget->arrange({0, 0, 30, 10});
    table_widget->handle_event(oui::Event{oui::KeyEvent{oui::Key::down}});

    EXPECT_EQ(table_widget->selected_row(), std::optional<std::size_t>{1});
}

TEST_F(TableTest, SortsCompleteRowsAndPreservesMetadata)
{
    auto table_widget{table({"Name", "Value"})};
    table_widget->add_line({"Cherry", "3"});
    table_widget->add_line({"Apple", "1"});
    table_widget->set_metadata(1, 1, "rank", 1);
    table_widget->set_column_sort(0, oui::SortOrder::ascending);

    table_widget->apply_sort(0);

    const auto* name = dynamic_cast<const oui::Label*>(table_widget->cell_widget(0, 0));
    const auto* value = dynamic_cast<const oui::Label*>(table_widget->cell_widget(0, 1));
    ASSERT_NE(name, nullptr);
    ASSERT_NE(value, nullptr);
    EXPECT_STREQ(name->text().data(), "Apple");
    EXPECT_STREQ(value->text().data(), "1");
    EXPECT_EQ(table_widget->metadata_or<int>(0, 1, "rank", 0), 1);
}

TEST_F(TableTest, HeaderClickSortsUsingTheClickedColumn)
{
    auto table_widget{table({"Name", "Value"})};
    table_widget->set_column_width(0, 10);
    table_widget->set_column_width(1, 5);
    table_widget->set_headers_clickable(true);
    table_widget->add_line({"Cherry", "3"});
    table_widget->add_line({"Apple", "1"});
    table_widget->arrange({0, 0, 30, 10});
    std::optional<std::pair<std::size_t, oui::SortOrder>> announced;
    const auto connection{table_widget->on_column_click([&announced](std::size_t column, oui::SortOrder order)
                                                        { announced = {column, order}; })};

    table_widget->handle_event(oui::Event{oui::MouseEvent{1, 0, true}});
    // Held down, nothing is sorted yet; let go on the same header, it is.
    EXPECT_FALSE(announced);
    oui::MouseEvent release{1, 0, false};
    release.released = true;
    table_widget->handle_event(oui::Event{release});

    const auto* name = dynamic_cast<const oui::Label*>(table_widget->cell_widget(0, 0));
    ASSERT_NE(name, nullptr);
    EXPECT_EQ(table_widget->column_sort_order(0), oui::SortOrder::ascending);
    ASSERT_TRUE(announced);
    EXPECT_EQ(announced->first, 0u);
    EXPECT_EQ(announced->second, oui::SortOrder::ascending);
    EXPECT_STREQ(name->text().data(), "Apple");
}

TEST_F(TableTest, DraggingAHeaderEdgeResizesTheColumn)
{
    auto table_widget{table({"Name", "Value"})};
    table_widget->set_column_width(0, 10);
    table_widget->set_column_width(1, 5);
    table_widget->set_headers_clickable(true);
    table_widget->set_columns_resizable(true);
    table_widget->add_line({"Cherry", "3"});
    table_widget->arrange({0, 0, 30, 10});
    std::optional<std::pair<std::size_t, int>> resized;
    bool sorted{false};
    const auto resize_connection{
        table_widget->on_column_resized([&resized](std::size_t column, int width) { resized = {column, width}; })};
    const auto sort_connection{
        table_widget->on_column_click([&sorted](std::size_t, oui::SortOrder) { sorted = true; })};
    const auto drag{[&table_widget](int from, int to)
                    {
                        table_widget->handle_event(oui::MouseEvent{from, 0, true});
                        oui::MouseEvent moved{to, 0, false};
                        moved.dragging = true;
                        table_widget->handle_event(moved);
                        oui::MouseEvent release{to, 0, false};
                        release.released = true;
                        table_widget->handle_event(release);
                    }};

    // The last cell of the first column is its edge.
    drag(9, 14);
    EXPECT_EQ(table_widget->column_width(0), 15);
    ASSERT_TRUE(resized);
    EXPECT_EQ(*resized, (std::pair<std::size_t, int>{0, 15}));
    EXPECT_FALSE(sorted) << "a drag is not a click";

    // The stretch column's edge moves the column after it instead.
    table_widget->set_stretch_column(0);
    table_widget->arrange({0, 0, 30, 10});
    drag(24, 20);
    EXPECT_EQ(table_widget->column_width(1), 9);

    // Let go where it was pressed, a header is a click that sorts.
    drag(26, 26);
    EXPECT_TRUE(sorted);
}

TEST_F(TableTest, MouseClickSelectsTheContainingRow)
{
    auto table_widget{table({"Name", "Value"})};
    table_widget->add_line({"first", "1"});
    table_widget->add_line({"second", "2"});
    table_widget->arrange({0, 0, 30, 10});

    const auto second_row{table_widget->cell_widget(1, 1)->bounds()};
    table_widget->handle_event(oui::Event{oui::MouseEvent{second_row.x, second_row.y, true}});

    EXPECT_EQ(table_widget->selected_row(), std::optional<std::size_t>{1});
}

TEST(TypeAheadTest, FindsByPrefixStepsThroughARepeatedLetterAndStartsOverAfterAPause)
{
    const std::vector<std::string> names{"apple", "Banana", "berry", "cherry"};
    const auto text_of{[&names](std::size_t row) { return names.at(row); }};
    oui::TypeAhead typing;
    // "be" skips "Banana", which only matches the "b"; case does not matter.
    EXPECT_EQ(typing.find(U'b', names.size(), 0, text_of), std::optional<std::size_t>{1});
    EXPECT_EQ(typing.find(U'e', names.size(), 1, text_of), std::optional<std::size_t>{2});
    EXPECT_EQ(typing.find(U'x', names.size(), 2, text_of), std::nullopt);
    oui::TypeAhead stepping;
    EXPECT_EQ(stepping.find(U'b', names.size(), 0, text_of), std::optional<std::size_t>{1});
    EXPECT_EQ(stepping.find(U'b', names.size(), 1, text_of), std::optional<std::size_t>{2});
    EXPECT_EQ(stepping.find(U'b', names.size(), 2, text_of), std::optional<std::size_t>{1});
    // With no pause allowed, every character starts a new search.
    oui::TypeAhead impatient{std::chrono::milliseconds{-1}};
    EXPECT_EQ(impatient.find(U'c', names.size(), 0, text_of), std::optional<std::size_t>{3});
    EXPECT_EQ(impatient.find(U'a', names.size(), 3, text_of), std::optional<std::size_t>{0});
}

TEST(OuiTableRowTests, TypingSelectsTheRowWhoseTextStartsWithIt)
{
    oui::Table table;
    table.set_headers({"Name", "Size"});
    for(const auto* const name : {"alpha", "beta", "gamma"})
    {
        table.add_line({name, "1"});
    }
    table.set_type_ahead([&table](std::size_t row)
                         { return std::string{static_cast<oui::Label*>(table.cell_widget(row, 0))->text()}; });
    EXPECT_TRUE(table.handle_event(oui::TextEvent{U'g'}));
    EXPECT_EQ(table.selected_row(), std::optional<std::size_t>{2});
}

TEST(OuiTableRowTests, ScrollsByRowsPagesAndActivatesWithEnter)
{
    oui::Table table;
    table.set_headers({"Name", "Size", "Type"});
    for(int row{}; row < 30; ++ row)
    {
        table.add_line({"row " + std::to_string(row), "1", "t"});
    }
    std::optional<std::size_t> activated;
    table.on_row_activated([&activated](std::size_t row) { activated = row; });
    // A header and five rows.
    table.arrange({0, 0, 40, 6});
    table.select_row(20);
    table.arrange({0, 0, 40, 6});
    // The selected row is on screen, below the header, however many columns precede it.
    const auto& cell{*table.cell_widget(20, 0)};
    EXPECT_GE(cell.bounds().y, 1);
    EXPECT_LE(cell.bounds().y, 5);
    EXPECT_TRUE(table.handle_event(oui::KeyEvent{oui::Key::page_down}));
    EXPECT_EQ(table.selected_row(), std::optional<std::size_t>{25});
    EXPECT_TRUE(table.handle_event(oui::KeyEvent{oui::Key::page_down}));
    EXPECT_EQ(table.selected_row(), std::optional<std::size_t>{29});
    EXPECT_TRUE(table.handle_event(oui::KeyEvent{oui::Key::enter}));
    EXPECT_EQ(activated, std::optional<std::size_t>{29});
    // At the last row Down stays in the table, as Turbo Vision's lists do.
    EXPECT_TRUE(table.handle_event(oui::KeyEvent{oui::Key::down}));
    EXPECT_EQ(table.selected_row(), std::optional<std::size_t>{29});
}
