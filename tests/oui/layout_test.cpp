#include <oui/application.h>
#include <oui/button.h>
#include <oui/check_button.h>
#include <oui/turbo_c_theme.h>
#include <oui/cell_buffer.h>
#include <oui/clipboard.h>
#include <oui/command_line.h>
#include <oui/combo_box.h>
#include <oui/command_palette.h>
#include <oui/dynamic_grid.h>
#include <oui/declarative.h>
#include <oui/folder_view.h>
#include <oui/fuzzy.h>
#include <oui/grid.h>
#include <oui/label.h>
#include <oui/layout.h>
#include <oui/line_edit.h>
#include <oui/number_field.h>
#include <oui/list.h>
#include <oui/menu.h>
#include <oui/menu_bar.h>
#include <oui/panel.h>
#include <oui/raster_backend.h>
#include <oui/scroll_view.h>
#include <oui/tab_switcher.h>
#include <oui/dock_layout.h>
#include <oui/progress_bar.h>
#include <oui/scripted_backend.h>
#include <oui/status_bar.h>
#include <oui/tab_view.h>
#include <oui/markdown_view.h>
#include <oui/split_view.h>
#include <oui/text_edit.h>
#include <oui/text_layout.h>
#include <oui/tree_view.h>
#include <oui/utf8.h>
#include <oui/window.h>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <limits>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

using namespace olagarro;

namespace
{

class FixedWidget final : public oui::Widget
{
  public:
    explicit FixedWidget(oui::Size size) : m_size{size} {}

    [[nodiscard]] oui::Size measure(const oui::Constraints& constraints) const override
    {
        return constraints.constrain(m_size);
    }

  private:
    oui::Size m_size;
};

class MemoryBackend final : public oui::Backend
{
  public:
    [[nodiscard]] oui::Size size() const override { return {20, 4}; }

    std::optional<oui::Event> poll_event() override
    {
        if(m_events.empty())
        {
            return std::nullopt;
        }

        const auto event{m_events.front()};
        m_events.erase(m_events.begin());
        return event;
    }

    void present(const oui::CellBuffer& buffer) override { m_last_frame = buffer; }
    [[nodiscard]] int pointer_columns(int cell_offset) const override
    {
        static_cast<void>(cell_offset);
        return m_pointer_columns;
    }

    void push_event(const oui::Event& event) { m_events.push_back(event); }
    // As a window with a big pointer says.
    void set_pointer_columns(int columns) { m_pointer_columns = columns; }

    [[nodiscard]] const std::optional<oui::CellBuffer>& last_frame() const { return m_last_frame; }

  private:
    std::vector<oui::Event> m_events;
    std::optional<oui::CellBuffer> m_last_frame;
    int m_pointer_columns{2};
};

class OverlayTestContainer final : public oui::Container
{
  public:
    [[nodiscard]] oui::Size measure(const oui::Constraints& constraints) const override
    {
        return constraints.constrain({1, 1});
    }

    void arrange(const oui::Rect& bounds) override
    {
        Widget::arrange(bounds);
        for(std::size_t index{}; index < child_count(); ++ index)
        {
            child_at(index).arrange(bounds);
        }
    }

    using Container::add;
};

} // namespace

TEST(OuiDeclarativeTests, ParsesAnIndentedWidgetTree)
{
    const auto result{oui::parse_oui("Window \"Demo\"\n  Column\n    Label \"Hello\"\n")};
    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.root.has_value());
    EXPECT_EQ(result.root->type, "Window");
    ASSERT_EQ(result.root->children.size(), 1u);
    EXPECT_EQ(result.root->children.front().children.front().text, "Hello");
}

TEST(OuiDeclarativeTests, ReportsInvalidWidgetText)
{
    const auto result{oui::parse_oui("Label \"missing its closing quote\n")};
    ASSERT_FALSE(result.succeeded());
    EXPECT_EQ(result.diagnostics.front().line, 1u);
}

TEST(OuiLabelTests, WrapsTextAndMeasuresTheRequiredHeight)
{
    oui::Label label{"abcdef"};
    label.set_wrap(true);

    EXPECT_EQ(label.measure({3, 10}).width, 3);
    EXPECT_EQ(label.measure({3, 10}).height, 2);

    label.arrange({0, 0, 3, 2});
    oui::CellBuffer buffer{3, 2};
    oui::CellBufferPainter painter{buffer};
    label.paint(painter, {});
    EXPECT_EQ(buffer.cell_at(0, 1).glyph, U'd');
}

TEST(OuiRasterBackendTests, ConvertsCellBackgroundsIntoPixels)
{
    oui::RasterBackend backend{{2, 1}, 2, 2};
    oui::CellBuffer buffer{2, 1};
    buffer.set_cell(1, 0, {U'x', {1, 2, 3}, {4, 5, 6}});
    backend.present(buffer);

    EXPECT_EQ(backend.pixel_width(), 4);
    EXPECT_EQ(backend.pixel_at(3, 1), (oui::Color{4, 5, 6}));
}

TEST(OuiLayoutTests, RowAdoptsWidgetsAndArrangesThemHorizontally)
{
    auto layout{oui::row()};
    layout->set_gap(oui::Spacing::tight);
    auto first{std::make_unique<FixedWidget>(oui::Size{3, 1})};
    const auto& first_reference{layout->add(std::move(first))};
    const auto& second_reference{layout->add(std::make_unique<FixedWidget>(oui::Size{5, 2}))};

    EXPECT_EQ(layout->measure({20, 20}).width, 9);
    EXPECT_EQ(layout->measure({20, 20}).height, 2);
    EXPECT_EQ(first_reference.parent(), layout.get());

    layout->arrange({10, 20, 20, 4});

    EXPECT_EQ(first_reference.bounds().x, 10);
    EXPECT_EQ(first_reference.bounds().width, 3);
    EXPECT_EQ(second_reference.bounds().x, 14);
    EXPECT_EQ(second_reference.bounds().height, 4);
}

TEST(OuiLayoutTests, ColumnMeasuresAndArrangesWidgetsVertically)
{
    auto layout{oui::column()};
    layout->set_gap(oui::Spacing::normal);
    const auto& first_reference{layout->add(std::make_unique<FixedWidget>(oui::Size{3, 1}))};
    const auto& second_reference{layout->add(std::make_unique<FixedWidget>(oui::Size{5, 2}))};

    EXPECT_EQ(layout->measure({20, 20}).width, 5);
    EXPECT_EQ(layout->measure({20, 20}).height, 5);

    layout->arrange({10, 20, 20, 10});

    EXPECT_EQ(first_reference.bounds().y, 20);
    EXPECT_EQ(first_reference.bounds().height, 1);
    EXPECT_EQ(second_reference.bounds().y, 23);
    EXPECT_EQ(second_reference.bounds().width, 20);
}

TEST(OuiLayoutTests, ParentAwareButtonFactoryOwnsTheCallback)
{
    auto actions{oui::row()};
    auto calls{0};
    auto& cancel{oui::button("Cancel", *actions, [&calls] { ++ calls; })};

    EXPECT_EQ(actions->child_count(), 1u);
    cancel.click();

    EXPECT_EQ(calls, 1);
}

TEST(OuiPainterTests, DrawsTextAndHonorsTheClipStack)
{
    oui::CellBuffer buffer{4, 1};
    oui::CellBufferPainter painter{buffer};
    painter.push_clip({1, 0, 2, 1});
    painter.draw_text({0, 0}, "abcd", {{1, 2, 3}, {4, 5, 6}});
    painter.pop_clip();

    EXPECT_EQ(buffer.cell_at(0, 0).glyph, U' ');
    EXPECT_EQ(buffer.cell_at(1, 0).glyph, U'b');
    EXPECT_EQ(buffer.cell_at(2, 0).glyph, U'c');
    EXPECT_EQ(buffer.cell_at(3, 0).glyph, U' ');
    EXPECT_EQ(buffer.cell_at(1, 0).foreground, (oui::Color{1, 2, 3}));
}

TEST(OuiPainterTests, DecodesUtf8IntoCells)
{
    oui::CellBuffer buffer{2, 1};
    oui::CellBufferPainter painter{buffer};
    painter.draw_text({0, 0}, "\xc3\xb1", {});

    EXPECT_EQ(buffer.cell_at(0, 0).glyph, U'ñ');
}

TEST(OuiPainterTests, AdvancesByTerminalCellWidth)
{
    oui::CellBuffer buffer{4, 1};
    oui::CellBufferPainter painter{buffer};
    painter.draw_text({0, 0}, "\xe4\xb8\xadx", {});

    EXPECT_EQ(buffer.cell_at(0, 0).glyph, U'中');
    EXPECT_TRUE(buffer.cell_at(1, 0).continuation);
    EXPECT_EQ(buffer.cell_at(2, 0).glyph, U'x');
}

TEST(OuiPainterTests, ReplacesInvalidUtf8Sequences)
{
    oui::CellBuffer buffer{2, 1};
    oui::CellBufferPainter painter{buffer};
    painter.draw_text({0, 0}, "\xc3x", {});

    EXPECT_EQ(buffer.cell_at(0, 0).glyph, U'\ufffd');
    EXPECT_EQ(buffer.cell_at(1, 0).glyph, U'x');
}

TEST(OuiWidgetTests, LabelMeasuresItsText)
{
    const auto text{oui::label("OUI")};

    EXPECT_EQ(text->measure({10, 10}).width, 3);
    EXPECT_EQ(text->measure({10, 10}).height, 1);
}

TEST(OuiLayoutTests, GridUsesContentAndFillColumnsForForms)
{
    auto form{oui::grid()};
    form->set_columns({oui::Length::content(), oui::Length::fill()});
    form->set_gap(oui::Spacing::tight);
    const auto& label{form->add(oui::label("Name"))};
    const auto& field{form->add(oui::button("Value"))};

    form->arrange({0, 0, 20, 2});

    EXPECT_EQ(label.bounds().width, 4);
    EXPECT_EQ(field.bounds().x, 5);
    EXPECT_EQ(field.bounds().width, 15);
}

TEST(OuiLayoutTests, GridMeasuresAFillColumnAsWideAsItsWidestChild)
{
    // A column of one fill column inside a row as wide as its content: not nothing.
    auto column{oui::grid()};
    column->set_columns({oui::Length::fill()});
    column->add(std::make_unique<FixedWidget>(oui::Size{6, 1}));
    column->add(std::make_unique<FixedWidget>(oui::Size{9, 1}));
    auto row{oui::grid()};
    row->set_columns({oui::Length::content(), oui::Length::fill()});
    const auto& inner{row->add(std::move(column))};
    row->add(std::make_unique<FixedWidget>(oui::Size{3, 1}));

    row->arrange({0, 0, 40, 2});

    EXPECT_EQ(inner.bounds().width, 9);
}

// A hidden widget took its room, and the gap after it, in rows, columns and grids alike.
TEST(OuiLayoutTests, HiddenWidgetsTakeNoRoomInRowsColumnsAndGrids)
{
    auto column{oui::column()};
    column->set_gap(oui::Spacing::tight);
    column->add(std::make_unique<FixedWidget>(oui::Size{4, 2}));
    auto& hidden{column->add(std::make_unique<FixedWidget>(oui::Size{9, 3}))};
    const auto& last{column->add(std::make_unique<FixedWidget>(oui::Size{4, 2}))};
    hidden.set_visible(false);
    EXPECT_EQ(column->measure({40, 40}).height, 2 + 1 + 2);
    EXPECT_EQ(column->measure({40, 40}).width, 4);
    column->arrange({0, 0, 40, 40});
    EXPECT_EQ(last.bounds().y, 3);

    auto row{oui::row()};
    row->set_gap(oui::Spacing::tight);
    row->add(std::make_unique<FixedWidget>(oui::Size{4, 1}));
    row->add(std::make_unique<FixedWidget>(oui::Size{9, 1})).set_visible(false);
    const auto& after{row->add(std::make_unique<FixedWidget>(oui::Size{4, 1}))};
    EXPECT_EQ(row->measure({40, 40}).width, 4 + 1 + 4);
    row->arrange({0, 0, 40, 1});
    EXPECT_EQ(after.bounds().x, 5);

    // A grid's row whose widgets are all hidden goes, whatever its length, and so does a column.
    auto grid{oui::grid()};
    grid->set_gap(oui::Spacing::tight);
    grid->set_columns({oui::Length::content(), oui::Length::content(), oui::Length::fill()});
    grid->set_rows({oui::Length::content(), oui::Length::flex(), oui::Length::content()});
    std::vector<oui::Widget*> cells;
    for(int index{}; index < 9; ++ index)
    {
        cells.push_back(&grid->add(std::make_unique<FixedWidget>(oui::Size{3, 2})));
    }
    for(const int index : {3, 4, 5, 1, 7})
    {
        cells[static_cast<std::size_t>(index)]->set_visible(false);
    }
    EXPECT_EQ(grid->measure({40, 40}).height, 2 + 1 + 2);
    grid->arrange({0, 0, 40, 20});
    EXPECT_EQ(cells[6]->bounds().y, 3);
    EXPECT_EQ(cells[2]->bounds().x, 4) << "the middle column, hidden in every row, takes no room";
}

// Rows took only flex from their lengths: fill, a number of cells and a share were all as tall as their content.
TEST(OuiLayoutTests, GridRowsTakeTheLengthsColumnsTake)
{
    auto grid{oui::grid()};
    grid->set_columns({oui::Length::fill()});
    grid->set_rows({oui::Length::cells(3), oui::Length::percent(50), oui::Length::fill(), oui::Length::content()});
    std::vector<oui::Widget*> cells;
    for(int index{}; index < 4; ++ index)
    {
        cells.push_back(&grid->add(std::make_unique<FixedWidget>(oui::Size{3, 1})));
    }
    grid->arrange({0, 0, 10, 20});
    EXPECT_EQ(cells[0]->bounds().height, 3);
    EXPECT_EQ(cells[1]->bounds().height, 10);
    EXPECT_EQ(cells[3]->bounds().height, 1);
    EXPECT_EQ(cells[2]->bounds().height, 20 - 3 - 10 - 1);
}

TEST(OuiLayoutTests, GridKeepsFixedRowsVisibleAfterATallFlexRow)
{
    auto layout{oui::grid()};
    layout->set_columns({oui::Length::flex()});
    layout->set_rows({oui::Length::content(), oui::Length::flex(), oui::Length::content()});
    const auto& header{layout->add(std::make_unique<FixedWidget>(oui::Size{1, 1}))};
    const auto& body{layout->add(std::make_unique<FixedWidget>(oui::Size{1, 20}))};
    const auto& footer{layout->add(std::make_unique<FixedWidget>(oui::Size{1, 2}))};

    layout->arrange({0, 0, 20, 10});

    EXPECT_EQ(header.bounds().height, 1);
    EXPECT_EQ(body.bounds().height, 7);
    EXPECT_EQ(footer.bounds().y, 8);
    EXPECT_EQ(footer.bounds().height, 2);
}

TEST(OuiLayoutTests, DynamicGridReflowsChildrenToTheAvailableWidth)
{
    auto grid{oui::dynamic_grid()};
    grid->set_column_width(oui::Length::cells(3));
    grid->set_gap(oui::Spacing::tight);
    const auto& first{grid->add(std::make_unique<FixedWidget>(oui::Size{1, 1}))};
    grid->add(std::make_unique<FixedWidget>(oui::Size{1, 1}));
    grid->add(std::make_unique<FixedWidget>(oui::Size{1, 1}));
    const auto& fourth{grid->add(std::make_unique<FixedWidget>(oui::Size{1, 1}))};

    grid->arrange({0, 0, 7, 4});

    EXPECT_EQ(first.bounds().x, 0);
    EXPECT_EQ(fourth.bounds().x, 4);
    EXPECT_EQ(fourth.bounds().y, 2);
}

TEST(OuiLayoutTests, ScrollViewMovesContentWithinTheViewport)
{
    auto scroll{oui::scroll_view()};
    const auto& content{scroll->add(std::make_unique<FixedWidget>(oui::Size{4, 5}))};
    scroll->arrange({0, 0, 4, 2});
    scroll->handle_event(oui::KeyEvent{oui::Key::down});
    scroll->arrange({0, 0, 4, 2});

    EXPECT_EQ(scroll->scroll_offset(), 1);
    EXPECT_EQ(content.bounds().y, -1);
}

TEST(OuiLayoutTests, PanelAppliesPaddingToItsContent)
{
    auto panel{oui::panel()};
    panel->set_padding({1, 2, 3, 4});
    const auto& content{panel->add(std::make_unique<FixedWidget>(oui::Size{2, 1}))};
    panel->arrange({0, 0, 10, 10});

    EXPECT_EQ(content.bounds().x, 1);
    EXPECT_EQ(content.bounds().y, 2);
    EXPECT_EQ(content.bounds().width, 6);
    EXPECT_EQ(content.bounds().height, 4);
}

TEST(OuiLayoutTests, ListArrangesArbitraryWidgetsVertically)
{
    auto list{oui::list()};
    list->set_gap(oui::Spacing::tight);
    const auto& label{list->add(oui::label("Item"))};
    const auto& action{list->add(oui::button("Open"))};
    list->arrange({0, 0, 10, 4});

    EXPECT_EQ(label.bounds().y, 0);
    EXPECT_EQ(action.bounds().y, 2);
}

TEST(OuiWidgetTests, ListSelectsItemsWithKeyboardNavigation)
{
    auto list{oui::list()};
    list->add(oui::label("First"));
    list->add(oui::label("Second"));
    auto selected{std::size_t{0}};
    list->on_selection_changed([&selected](std::size_t index) { selected = index; });

    list->handle_event(oui::KeyEvent{oui::Key::down});

    EXPECT_EQ(list->selected_row(), std::optional<std::size_t>{1});
    EXPECT_EQ(selected, 1);
}

TEST(OuiWidgetTests, ListHighlightsItsFocusedDefaultSelection)
{
    auto list{oui::list()};
    list->add(oui::label("First"));
    list->arrange({0, 0, 8, 1});
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_reference{backend.get()};
    oui::Application application{std::move(backend)};
    application.set_content(std::move(list));
    application.render();

    ASSERT_TRUE(backend_reference->last_frame().has_value());
    EXPECT_EQ(backend_reference->last_frame()->cell_at(0, 0).background, oui::Theme{}.item_selected_background);
}

TEST(OuiWidgetTests, WindowArrangesContentAndReportsItsResult)
{
    auto window{oui::window("Confirm")};
    const auto& content{window->add(std::make_unique<FixedWidget>(oui::Size{3, 2}))};
    auto result{oui::DialogResult::cancelled};
    window->on_closed([&result](oui::DialogResult value) { result = value; });

    window->arrange({0, 0, 10, 5});
    window->close(oui::DialogResult::accepted);

    EXPECT_EQ(content.bounds().y, 1);
    EXPECT_TRUE(window->is_closed());
    EXPECT_EQ(result, oui::DialogResult::accepted);
}

TEST(OuiWidgetTests, WindowPaintsABorder)
{
    oui::Window window{"Border"};
    window.arrange({0, 0, 8, 4});
    oui::CellBuffer buffer{8, 4};
    oui::CellBufferPainter painter{buffer};
    window.paint(painter, {});

    EXPECT_EQ(buffer.cell_at(0, 0).glyph, U'╔');
    EXPECT_EQ(buffer.cell_at(7, 3).glyph, U'╝');
}

TEST(OuiWidgetTests, WindowCastsAShadowThatLeavesWhatIsUnderItVisible)
{
    oui::Window window{"Shadow"};
    window.arrange({1, 1, 8, 4});
    oui::CellBuffer buffer{12, 8};
    oui::CellBufferPainter painter{buffer};
    const auto theme{oui::Theme{}};
    // What was there before the window, under where its shadow falls.
    painter.draw_text({9, 3}, "abz", {{255, 255, 255}, {0, 0, 170}});
    painter.draw_text({2, 5}, "y c", {{255, 255, 255}, {0, 0, 170}});
    window.paint(painter, theme);

    // Two columns at its right, a row and two columns in under it: the characters stay, in the shadow's colours.
    for(const auto& [x, y, glyph] : {std::tuple{9, 3, U'a'}, std::tuple{10, 3, U'b'}, std::tuple{4, 5, U'c'}})
    {
        EXPECT_EQ(buffer.cell_at(x, y).glyph, glyph);
        EXPECT_EQ(buffer.cell_at(x, y).foreground, theme.shadow_foreground);
        EXPECT_EQ(buffer.cell_at(x, y).background, theme.shadow);
    }
    EXPECT_EQ(buffer.cell_at(11, 3).background, (oui::Color{0, 0, 170})) << "two columns, no more";
    EXPECT_EQ(buffer.cell_at(2, 5).background, (oui::Color{0, 0, 170})) << "the row under starts two in";
}

TEST(OuiWidgetTests, WindowPaintsItsTitleInTheMiddleOfItsTopEdge)
{
    oui::Window window{"Save"};
    window.arrange({0, 0, 16, 6});
    oui::CellBuffer buffer{16, 6};
    oui::CellBufferPainter painter{buffer};
    window.paint(painter, {});

    // " Save " is six cells, centred in sixteen: from the fifth.
    std::u32string title;
    for(int x{5}; x < 11; ++ x)
    {
        title += buffer.cell_at(x, 0).glyph;
    }
    EXPECT_EQ(title, U" Save ");
    EXPECT_EQ(buffer.cell_at(4, 0).glyph, U'═');
    EXPECT_EQ(buffer.cell_at(11, 0).glyph, U'═');
}

TEST(OuiWidgetTests, WindowMovesWhenItsTitleIsDragged)
{
    oui::Window window{"Move"};
    window.arrange({0, 0, 40, 20});
    const auto original_bounds{window.bounds()};

    EXPECT_TRUE(window.handle_event(oui::MouseEvent{original_bounds.x + 2, original_bounds.y, true}));
    EXPECT_TRUE(window.handle_event(oui::MouseEvent{original_bounds.x + 7, original_bounds.y + 3, false, 0, true}));
    EXPECT_TRUE(
        window.handle_event(oui::MouseEvent{original_bounds.x + 7, original_bounds.y + 3, false, 0, false, true}));
    window.arrange({0, 0, 40, 20});

    EXPECT_EQ(window.bounds().x, original_bounds.x + 5);
    EXPECT_EQ(window.bounds().y, original_bounds.y + 3);
}

namespace
{

// Drags from `from` to `to` with the left button: press, one drag step, release.
void drag(oui::Window& window, oui::Point from, oui::Point to)
{
    window.handle_event(oui::MouseEvent{from.x, from.y, true});
    window.handle_event(oui::MouseEvent{to.x, to.y, false, 0, true});
    window.handle_event(oui::MouseEvent{to.x, to.y, false, 0, false, true});
    window.arrange({0, 0, 60, 24});
}

} // namespace

TEST(OuiWidgetTests, ResizableWindowGrowsFromItsCornerAndKeepsItsTopLeft)
{
    oui::Window window{"Resize"};
    window.set_resizable(true);
    window.add(std::make_unique<FixedWidget>(oui::Size{20, 6}));
    window.arrange({0, 0, 60, 24});
    const auto before{window.bounds()};
    const oui::Point corner{before.x + before.width - 1, before.y + before.height - 1};

    drag(window, corner, {corner.x + 10, corner.y + 3});
    EXPECT_EQ(window.bounds().x, before.x);
    EXPECT_EQ(window.bounds().y, before.y);
    EXPECT_EQ(window.bounds().width, before.width + 10);
    EXPECT_EQ(window.bounds().height, before.height + 3);

    // The right edge changes only the width and the bottom edge only the height.
    const auto grown{window.bounds()};
    drag(window, {grown.x + grown.width - 1, grown.y + 2}, {grown.x + grown.width - 5, grown.y + 2});
    EXPECT_EQ(window.bounds().width, grown.width - 4);
    EXPECT_EQ(window.bounds().height, grown.height);
    drag(window, {grown.x + 3, grown.y + grown.height - 1}, {grown.x + 3, grown.y + grown.height + 1});
    EXPECT_EQ(window.bounds().height, grown.height + 2);
    EXPECT_EQ(window.bounds().x, before.x);
    EXPECT_EQ(window.bounds().y, before.y);
}

TEST(OuiWidgetTests, ResizableWindowStaysWithinItsMinimumAndTheScreen)
{
    oui::Window window{"Resize"};
    window.set_resizable(true);
    window.set_minimum_size({18, 7});
    window.add(std::make_unique<FixedWidget>(oui::Size{20, 6}));
    window.arrange({0, 0, 60, 24});
    const auto before{window.bounds()};
    const oui::Point corner{before.x + before.width - 1, before.y + before.height - 1};

    drag(window, corner, {before.x, before.y});
    EXPECT_EQ(window.bounds().width, 18);
    EXPECT_EQ(window.bounds().height, 7);

    const auto small{window.bounds()};
    drag(window, {small.x + small.width - 1, small.y + small.height - 1}, {200, 200});
    EXPECT_EQ(window.bounds().x + window.bounds().width, 60);
    EXPECT_EQ(window.bounds().y + window.bounds().height, 24);
}

TEST(OuiWidgetTests, WindowPlacementHoldsOnlyWhatTheUserChanged)
{
    oui::Window window{"Placement"};
    window.set_resizable(true);
    window.add(std::make_unique<FixedWidget>(oui::Size{20, 6}));
    window.arrange({0, 0, 60, 24});
    EXPECT_FALSE(window.placement().position);
    EXPECT_FALSE(window.placement().size);

    const auto before{window.bounds()};
    drag(window, {before.x + before.width - 1, before.y + before.height - 1},
         {before.x + before.width + 4, before.y + before.height + 1});
    EXPECT_FALSE(window.placement().position);
    ASSERT_TRUE(window.placement().size);
    EXPECT_EQ(window.placement().size->width, before.width + 5);

    drag(window, {before.x + 3, before.y}, {before.x + 1, before.y + 2});
    ASSERT_TRUE(window.placement().position);
    EXPECT_EQ(window.placement().position->x, before.x - 2);
    EXPECT_EQ(window.placement().position->y, before.y + 2);
}

TEST(OuiWidgetTests, WindowRestoresAPlacementWithinTheScreen)
{
    oui::Window window{"Placement"};
    window.set_resizable(true);
    window.add(std::make_unique<FixedWidget>(oui::Size{20, 6}));
    window.restore_placement({oui::Point{5, 3}, oui::Size{30, 10}});
    window.arrange({0, 0, 60, 24});
    EXPECT_EQ(window.bounds().x, 5);
    EXPECT_EQ(window.bounds().y, 3);
    EXPECT_EQ(window.bounds().width, 30);
    EXPECT_EQ(window.bounds().height, 10);

    // Saved on a bigger terminal: it is pulled back inside the smaller one.
    oui::Window moved{"Placement"};
    moved.add(std::make_unique<FixedWidget>(oui::Size{20, 6}));
    moved.restore_placement({oui::Point{90, 40}, std::nullopt});
    moved.arrange({0, 0, 60, 24});
    EXPECT_EQ(moved.bounds().x + moved.bounds().width, 60);
    EXPECT_EQ(moved.bounds().y + moved.bounds().height, 24);
}

TEST(OuiWidgetTests, FixedWindowIgnoresDragsOnItsCorner)
{
    oui::Window window{"Fixed"};
    window.add(std::make_unique<FixedWidget>(oui::Size{20, 6}));
    window.arrange({0, 0, 60, 24});
    const auto before{window.bounds()};
    const oui::Point corner{before.x + before.width - 1, before.y + before.height - 1};

    drag(window, corner, {corner.x + 10, corner.y + 3});
    EXPECT_EQ(window.bounds().width, before.width);
    EXPECT_EQ(window.bounds().height, before.height);
}

TEST(OuiWidgetTests, ResizableWindowMarksItsGripCorner)
{
    oui::Window window{"Resize"};
    window.set_resizable(true);
    window.arrange({0, 0, 30, 10});
    oui::CellBuffer buffer{30, 10};
    oui::CellBufferPainter painter{buffer};
    window.paint(painter, oui::Theme{});
    const auto right{oui::whole(window.bounds().x + window.bounds().width - 1)};
    const auto bottom{oui::whole(window.bounds().y + window.bounds().height - 1)};
    EXPECT_EQ(buffer.cell_at(right, bottom).glyph, U'┘');
    EXPECT_EQ(buffer.cell_at(right - 1, bottom).glyph, U'─');
}

TEST(OuiWidgetTests, WindowCentersItsNaturalSizeWithinTheAvailableArea)
{
    auto window{oui::window("Dialog")};
    window->add(std::make_unique<FixedWidget>(oui::Size{8, 2}));

    window->arrange({0, 0, 40, 20});

    EXPECT_EQ(window->bounds().x, 14);
    EXPECT_EQ(window->bounds().y, 8);
    EXPECT_EQ(window->bounds().width, 12);
    EXPECT_EQ(window->bounds().height, 4);
}

TEST(OuiWidgetTests, BorderlessWindowUsesTheEntireAvailableArea)
{
    auto window{oui::window("Editor")};
    window->set_border(oui::WindowBorder::none);
    const auto& content{window->add(std::make_unique<FixedWidget>(oui::Size{1, 1}))};

    window->arrange({0, 0, 100, 30});

    EXPECT_EQ(content.bounds().x, 0);
    EXPECT_EQ(content.bounds().y, 0);
    EXPECT_EQ(content.bounds().width, 100);
    EXPECT_EQ(content.bounds().height, 30);
}

TEST(OuiWidgetTests, WindowClosesAsCancelledWhenEscapeIsPressed)
{
    auto window{oui::window("Confirm")};
    auto result{oui::DialogResult::accepted};
    window->on_closed([&result](oui::DialogResult value) { result = value; });

    EXPECT_TRUE(window->handle_event(oui::KeyEvent{oui::Key::escape}));
    EXPECT_TRUE(window->is_closed());
    EXPECT_EQ(result, oui::DialogResult::cancelled);
}

TEST(OuiWidgetTests, LineEditEditsTextFromKeyboardEvents)
{
    auto editor{oui::line_edit()};
    editor->set_text("ab");

    editor->handle_event(oui::KeyEvent{oui::Key::left});
    editor->handle_event(oui::TextEvent{U'X'});
    editor->handle_event(oui::KeyEvent{oui::Key::backspace});

    EXPECT_EQ(editor->text(), "ab");
}

TEST(OuiTextLayoutTests, WrapsAtWordsAndBreaksOnlyAWordTooLongForALine)
{
    EXPECT_EQ(oui::wrap_text("finds the first fifty", 10), (std::vector<std::string>{"finds the", "first", "fifty"}));
    EXPECT_EQ(oui::wrap_text("abcdefghijkl and", 5), (std::vector<std::string>{"abcde", "fghij", "kl", "and"}));
    // A line break always breaks, and leading blanks on a line stay, as indentation.
    EXPECT_EQ(oui::wrap_text("one\n  two", 10), (std::vector<std::string>{"one", "  two"}));
    EXPECT_EQ(oui::wrap_text("", 10), (std::vector<std::string>{""}));
}

TEST(OuiWidgetTests, ALineEditWiderThanItsTextScrollsToTheCursor)
{
    oui::Application application{std::make_unique<MemoryBackend>()};
    auto content{oui::column()};
    auto& editor{content->add(oui::line_edit())};
    auto& other{content->add(oui::line_edit())};
    application.set_content(std::move(content));
    editor.arrange({0, 0, 10, 1});
    editor.set_text("abcdefghijklmnopqrstuvwxyz");
    application.focus(editor);
    editor.handle_event(oui::KeyEvent{oui::Key::end});
    const auto first_cell{[&editor]
                          {
                              oui::CellBuffer buffer{10, 1};
                              oui::CellBufferPainter painter{buffer};
                              editor.paint(painter, {});
                              return std::pair{buffer.cell_at(0, 0).glyph, buffer.cell_at(8, 0).glyph};
                          }};
    // The end shows, with a cell left for the cursor after it.
    EXPECT_EQ(first_cell(), std::pair(U'r', U'z'));
    editor.handle_event(oui::KeyEvent{oui::Key::home});
    EXPECT_EQ(first_cell().first, U'a');
    // Without the focus it reads from the start, wherever the cursor was left.
    editor.handle_event(oui::KeyEvent{oui::Key::end});
    application.focus(other);
    EXPECT_EQ(first_cell().first, U'a');
}

TEST(OuiWidgetTests, LineEditAcceptsSpacesAndUtf8Text)
{
    oui::LineEdit editor;
    EXPECT_TRUE(editor.handle_event(oui::KeyEvent{oui::Key::space}));
    EXPECT_TRUE(editor.handle_event(oui::TextEvent{U'ñ'}));
    EXPECT_EQ(editor.text(), " ñ");
}

TEST(OuiWidgetTests, LineEditReportsTextChanges)
{
    oui::LineEdit edit;
    std::string changed;
    edit.on_changed([&changed](std::string_view text) { changed = text; });

    EXPECT_TRUE(edit.handle_event(oui::TextEvent{U'a'}));
    EXPECT_EQ(changed, "a");
}

TEST(OuiWidgetTests, LineEditSupportsHomeEndAndDelete)
{
    oui::LineEdit edit;
    edit.set_text("aéz");
    static_cast<void>(edit.handle_event(oui::KeyEvent{oui::Key::home}));
    static_cast<void>(edit.handle_event(oui::KeyEvent{oui::Key::right}));
    static_cast<void>(edit.handle_event(oui::KeyEvent{oui::Key::delete_}));

    EXPECT_EQ(edit.text(), "az");
    static_cast<void>(edit.handle_event(oui::KeyEvent{oui::Key::end}));
    // Nothing after the cursor: Delete changes nothing, and is still the field's, no shortcut's.
    EXPECT_TRUE(edit.handle_event(oui::KeyEvent{oui::Key::delete_}));
    EXPECT_EQ(edit.text(), "az");
}

TEST(OuiWidgetTests, LineEditJumpsWordsWithControlArrows)
{
    oui::LineEdit edit;
    edit.set_text("hola mundo");
    edit.handle_event(oui::Event{oui::KeyEvent{oui::Key::home}});
    edit.handle_event(oui::Event{oui::KeyEvent{oui::Key::right, true}});
    edit.handle_event(oui::Event{oui::TextEvent{U'X'}});
    EXPECT_EQ(edit.text(), "holaX mundo");

    edit.handle_event(oui::Event{oui::KeyEvent{oui::Key::end}});
    edit.handle_event(oui::Event{oui::KeyEvent{oui::Key::left, true}});
    edit.handle_event(oui::Event{oui::TextEvent{U'Y'}});
    EXPECT_EQ(edit.text(), "holaX Ymundo");
}

TEST(OuiWidgetTests, LineEditSelectsCutsAndPastesText)
{
    oui::LineEdit edit;
    edit.set_text("alpha");
    static_cast<void>(edit.handle_event(oui::KeyEvent{oui::Key::home}));
    static_cast<void>(edit.handle_event(oui::KeyEvent{oui::Key::right, false, false, true}));
    static_cast<void>(edit.handle_event(oui::KeyEvent{oui::Key::right, false, false, true}));
    std::string clipboard;
    edit.cut_selection(clipboard);

    EXPECT_EQ(clipboard, "al");
    EXPECT_EQ(edit.text(), "pha");
    edit.paste("be");
    EXPECT_EQ(edit.text(), "bepha");
}

TEST(OuiWidgetTests, APasswordLineEditShowsADotForEachCharacterAndKeepsWhatIsTyped)
{
    oui::Application application{std::make_unique<MemoryBackend>()};
    auto content{oui::column()};
    auto& edit{content->add(oui::line_edit())};
    application.set_content(std::move(content));
    edit.set_password(true);
    EXPECT_TRUE(edit.is_password());
    edit.arrange({0, 0, 12, 1});
    application.focus(edit);
    for(const auto letter : std::u32string{U"señal"})
    {
        edit.handle_event(oui::TextEvent{letter});
    }
    EXPECT_EQ(edit.text(), "señal");
    const auto shown{[&edit]
                     {
                         oui::CellBuffer buffer{12, 1};
                         oui::CellBufferPainter painter{buffer};
                         edit.paint(painter, {});
                         std::u32string cells;
                         for(int x{}; x < 12; ++ x)
                         {
                             cells += buffer.cell_at(x, 0).glyph;
                         }
                         return cells;
                     }};
    EXPECT_EQ(shown().substr(0, 6), U"●●●●● ");
    // The cursor moves a character, a dot, at a time, and a click lands between two dots.
    edit.handle_event(oui::KeyEvent{oui::Key::left});
    edit.handle_event(oui::TextEvent{U'X'});
    EXPECT_EQ(edit.text(), "señaXl");
    EXPECT_TRUE(edit.handle_event(oui::MouseEvent{2, 0, true}));
    edit.handle_event(oui::TextEvent{U'Y'});
    EXPECT_EQ(edit.text(), "seYñaXl");
    EXPECT_EQ(shown().substr(0, 8), U"●●●●●●● ");
}

TEST(OuiWidgetTests, AProgressBarFillsAsMuchOfItsTrackAsIsDoneAndSaysItsTextAfter)
{
    oui::ProgressBar bar;
    bar.set_value(0.5);
    bar.set_text("4 of 8");
    bar.arrange({0, 0, 20, 1});
    oui::CellBuffer buffer{20, 1};
    oui::CellBufferPainter painter{buffer};
    const oui::Theme theme{};
    bar.paint(painter, theme);
    // Thirteen cells of track, a space, and the text: half of thirteen is six cells and a half.
    EXPECT_EQ(buffer.cell_at(0, 0).background, theme.item_selected_background);
    EXPECT_EQ(buffer.cell_at(5, 0).background, theme.item_selected_background);
    EXPECT_EQ(buffer.cell_at(6, 0).glyph, U'\u258c');
    EXPECT_EQ(buffer.cell_at(6, 0).foreground, theme.item_selected_background);
    EXPECT_EQ(buffer.cell_at(7, 0).background, theme.scrollbar_background);
    EXPECT_EQ(buffer.cell_at(12, 0).background, theme.scrollbar_background);
    std::u32string text;
    for(int x{14}; x < 20; ++ x)
    {
        text += buffer.cell_at(x, 0).glyph;
    }
    EXPECT_EQ(text, U"4 of 8");
}

TEST(OuiWidgetTests, AProgressBarKeepsItsValueBetweenNothingAndAll)
{
    oui::ProgressBar bar;
    EXPECT_EQ(bar.value(), 0.0);
    bar.set_value(1.5);
    EXPECT_EQ(bar.value(), 1.0);
    bar.set_value(-1.0);
    EXPECT_EQ(bar.value(), 0.0);
    EXPECT_FALSE(bar.is_focusable());
    EXPECT_EQ(bar.measure({80, 25}).height, 1);
}

TEST(OuiWidgetTests, APasswordLineEditCopiesNothingAndCutsNothing)
{
    oui::LineEdit edit;
    edit.set_password(true);
    edit.set_text("hola mundo");
    edit.select_all();
    std::string clipboard{"before"};
    edit.copy_selection(clipboard);
    EXPECT_EQ(clipboard, "before");
    edit.cut_selection(clipboard);
    EXPECT_EQ(clipboard, "before");
    EXPECT_EQ(edit.text(), "hola mundo");
    // Control and an arrow go to an end, not to a word's: where its words are is not told either.
    edit.handle_event(oui::KeyEvent{oui::Key::end});
    edit.handle_event(oui::KeyEvent{oui::Key::left, true});
    edit.handle_event(oui::TextEvent{U'Y'});
    EXPECT_EQ(edit.text(), "Yhola mundo");
    edit.handle_event(oui::KeyEvent{oui::Key::right, true});
    edit.handle_event(oui::TextEvent{U'Z'});
    EXPECT_EQ(edit.text(), "Yhola mundoZ");
}

TEST(OuiWidgetTests, LineEditMovesTheCursorToTheClickedCell)
{
    oui::LineEdit edit;
    edit.set_text("alpha");
    edit.arrange({0, 0, 10, 1});

    EXPECT_TRUE(edit.handle_event(oui::MouseEvent{2, 0, true}));
    static_cast<void>(edit.handle_event(oui::TextEvent{U'X'}));
    EXPECT_EQ(edit.text(), "alXpha");
}

TEST(OuiWidgetTests, LineEditExtendsItsSelectionWhileDraggingTheMouse)
{
    oui::LineEdit edit;
    edit.set_text("hello");
    edit.arrange({0, 0, 10, 1});

    EXPECT_TRUE(edit.handle_event(oui::MouseEvent{1, 0, true}));
    EXPECT_TRUE(edit.handle_event(oui::MouseEvent{4, 0, false, 0, true}));
    EXPECT_TRUE(edit.handle_event(oui::MouseEvent{4, 0, false, 0, false, true}));
    EXPECT_EQ(edit.selected_text(), "ello");
}

TEST(OuiWidgetTests, AnEditorsCursorInATerminalIsItsCharacterReversed)
{
    oui::CellBuffer buffer{4, 1};
    oui::CellBufferPainter painter{buffer};
    const oui::Color text{200, 200, 200};
    const oui::Color background{10, 20, 30};
    painter.draw_text_cursor({1, 0}, "a", {text, background});
    EXPECT_EQ(buffer.cell_at(1, 0).glyph, U'a');
    EXPECT_EQ(buffer.cell_at(1, 0).foreground, background);
    EXPECT_EQ(buffer.cell_at(1, 0).background, text);
}

TEST(OuiWidgetTests, LineEditPaintsABlockCursorWhenFocused)
{
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_reference{backend.get()};
    oui::Application application{std::move(backend)};
    auto editor{oui::line_edit()};
    editor->set_text("ñ");
    application.set_content(std::move(editor));
    application.render();

    ASSERT_TRUE(backend_reference->last_frame().has_value());
    EXPECT_EQ(backend_reference->last_frame()->cell_at(1, 0).background, oui::Theme{}.focus_text);
}

TEST(OuiWidgetTests, LineEditKeepsThePlaceholderGlyphVisibleUnderItsCursor)
{
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_reference{backend.get()};
    oui::Application application{std::move(backend)};
    auto editor{oui::line_edit()};
    editor->set_placeholder("Filter commands");
    application.set_content(std::move(editor));
    application.render();

    ASSERT_TRUE(backend_reference->last_frame().has_value());
    EXPECT_EQ(backend_reference->last_frame()->cell_at(0, 0).glyph, U'F');
    EXPECT_EQ(backend_reference->last_frame()->cell_at(0, 0).foreground, oui::Theme{}.focus_background);
    EXPECT_EQ(backend_reference->last_frame()->cell_at(0, 0).background, oui::Theme{}.focus_text);
}

TEST(OuiWidgetTests, TextEditEditsUtf8AndRestoresChangesThroughUndoRedo)
{
    oui::TextEdit editor;
    editor.set_text("uno");
    editor.handle_event(oui::Event{oui::KeyEvent{oui::Key::end}});
    editor.handle_event(oui::Event{oui::TextEvent{U'ñ'}});

    EXPECT_EQ(editor.text(), "unoñ");
    EXPECT_TRUE(editor.is_modified());
    EXPECT_TRUE(editor.undo());
    EXPECT_EQ(editor.text(), "uno");
    EXPECT_TRUE(editor.redo());
    EXPECT_EQ(editor.text(), "unoñ");
}

TEST(OuiWidgetTests, TextEditAcceptsSpaceKeyEvents)
{
    oui::TextEdit editor;
    editor.set_text("one");
    static_cast<void>(editor.handle_event(oui::KeyEvent{oui::Key::end}));

    EXPECT_TRUE(editor.handle_event(oui::KeyEvent{oui::Key::space}));
    EXPECT_EQ(editor.text(), "one ");
}

TEST(OuiWidgetTests, TextEditAcceptsDeadKeyAccentCompositions)
{
    struct Accent
    {
        char32_t base;
        char32_t composed;
    };
    const std::vector<Accent> accents{
        {U'a', U'â'}, {U'e', U'ê'}, {U'i', U'î'}, {U'o', U'ô'}, {U'u', U'û'}, {U'a', U'á'}, {U'e', U'é'}, {U'i', U'í'},
        {U'o', U'ó'}, {U'u', U'ú'}, {U'a', U'à'}, {U'e', U'è'}, {U'i', U'ì'}, {U'o', U'ò'}, {U'u', U'ù'}, {U'a', U'ä'},
        {U'e', U'ë'}, {U'i', U'ï'}, {U'o', U'ö'}, {U'u', U'ü'}, {U'a', U'ã'}, {U'e', U'ẽ'}, {U'i', U'ĩ'}, {U'o', U'õ'},
        {U'u', U'ũ'}, {U'A', U'Â'}, {U'E', U'Ê'}, {U'I', U'Î'}, {U'O', U'Ô'}, {U'U', U'Û'}};

    oui::TextEdit editor;
    for(const auto& accent : accents)
    {
        static_cast<void>(editor.handle_event(oui::Event{oui::TextEvent{accent.base}}));
        static_cast<void>(editor.handle_event(oui::Event{oui::TextEvent{accent.composed}}));
    }
    static_cast<void>(editor.handle_event(oui::Event{oui::TextEvent{U'ñ'}}));
    static_cast<void>(editor.handle_event(oui::Event{oui::TextEvent{U'ç'}}));
    static_cast<void>(editor.handle_event(oui::Event{oui::TextEvent{U'^'}}));

    EXPECT_EQ(editor.text(), "aâeêiîoôuûaáeéiíoóuúaàeèiìoòuùaäeëiïoöuüaãeẽiĩoõuũAÂEÊIÎOÔUÛñç^");
    EXPECT_TRUE(editor.is_modified());
    EXPECT_TRUE(editor.undo());
    EXPECT_TRUE(editor.text().empty());
}

TEST(OuiWidgetTests, LineEditAcceptsDeadKeyAccentCompositions)
{
    oui::LineEdit edit;
    static_cast<void>(edit.handle_event(oui::Event{oui::TextEvent{U'o'}}));
    static_cast<void>(edit.handle_event(oui::Event{oui::TextEvent{U'ô'}}));
    static_cast<void>(edit.handle_event(oui::Event{oui::TextEvent{U'u'}}));
    static_cast<void>(edit.handle_event(oui::Event{oui::TextEvent{U'ü'}}));
    static_cast<void>(edit.handle_event(oui::Event{oui::TextEvent{U'e'}}));
    static_cast<void>(edit.handle_event(oui::Event{oui::TextEvent{U'ê'}}));
    static_cast<void>(edit.handle_event(oui::Event{oui::TextEvent{U'^'}}));
    static_cast<void>(edit.handle_event(oui::Event{oui::TextEvent{U'x'}}));

    EXPECT_EQ(edit.text(), "oôuüeê^x");
}

TEST(OuiWidgetTests, TextEditIgnoresControlAccentsAsShortcuts)
{
    oui::TextEdit editor;
    static_cast<void>(editor.handle_event(oui::Event{oui::TextEvent{U'n', true}}));
    static_cast<void>(editor.handle_event(oui::Event{oui::TextEvent{U'ñ', false, true}}));
    static_cast<void>(editor.handle_event(oui::Event{oui::TextEvent{U'ñ'}}));

    EXPECT_EQ(editor.text(), "ñ");
}

TEST(OuiWidgetTests, TextEditCopiesCutsAndPastesItsSelection)
{
    oui::TextEdit editor;
    editor.set_text("archivo");
    editor.select_all();
    std::string clipboard;
    editor.cut_selection(clipboard);

    EXPECT_EQ(clipboard, "archivo");
    EXPECT_TRUE(editor.text().empty());

    editor.paste(clipboard);
    EXPECT_EQ(editor.text(), "archivo");
}

TEST(OuiWidgetTests, TextEditCopiesAndPastesPunctuationAndUtf8WithoutLoss)
{
    const auto text{"#include <c++> { ñ, áéíóú! }"};
    oui::TextEdit editor;
    editor.set_text(text);
    editor.select_all();

    std::string clipboard;
    editor.copy_selection(clipboard);
    EXPECT_EQ(clipboard, text);

    editor.cut_selection(clipboard);
    EXPECT_TRUE(editor.text().empty());
    editor.paste(clipboard);
    EXPECT_EQ(editor.text(), text);
    EXPECT_TRUE(editor.undo());
    EXPECT_TRUE(editor.text().empty());
}

TEST(OuiWidgetTests, TextEditFindsUtf8TextAndWrapsAround)
{
    oui::TextEdit edit;
    edit.set_text("uno ñ dos ñ");

    EXPECT_TRUE(edit.find_next("ñ"));
    EXPECT_EQ(edit.selected_text(), "ñ");
    EXPECT_TRUE(edit.find_next("ñ"));
    EXPECT_EQ(edit.selected_text(), "ñ");
    EXPECT_TRUE(edit.find_next("ñ"));
    EXPECT_EQ(edit.selected_text(), "ñ");
}

TEST(OuiWidgetTests, TextEditReplacesTheNextUtf8Match)
{
    oui::TextEdit edit;
    edit.set_text("ñ uno ñ");

    EXPECT_TRUE(edit.replace_next("ñ", "dos"));
    EXPECT_EQ(edit.text(), "dos uno ñ");
    EXPECT_TRUE(edit.replace_next("ñ", "tres"));
    EXPECT_EQ(edit.text(), "dos uno tres");
}

TEST(OuiWidgetTests, TextEditReplacesEveryMatchAsOneUndoStep)
{
    oui::TextEdit edit;
    edit.set_text("ñ uno ñ dos ñ");
    EXPECT_EQ(edit.replace_all("ñ", "x"), 3u);
    EXPECT_EQ(edit.text(), "x uno x dos x");
    EXPECT_TRUE(edit.is_modified());
    EXPECT_TRUE(edit.undo());
    EXPECT_EQ(edit.text(), "ñ uno ñ dos ñ");
    EXPECT_EQ(edit.replace_all("tres", "x"), 0u);
}

TEST(OuiWidgetTests, TextEditReplaceAllSkipsOverlappingMatchesAndKeepsTheCursorsPlace)
{
    oui::TextEdit edit;
    edit.set_text("aaaa end");
    // "aa" matches at 0, 1 and 2; replacing the first two pairs leaves nothing for an overlap.
    edit.select_range(8, 8);
    EXPECT_EQ(edit.replace_all("aa", "b"), 2u);
    EXPECT_EQ(edit.text(), "bb end");
    EXPECT_EQ(edit.cursor_offset(), 6u);
    oui::SearchOptions whole_words;
    whole_words.whole_word = true;
    edit.set_text("cat category cat");
    EXPECT_EQ(edit.replace_all("cat", "dog", whole_words), 2u);
    EXPECT_EQ(edit.text(), "dog category dog");
}

TEST(OuiWidgetTests, TextEditTabIndentsEverySelectedLineAndShiftTabTakesItBack)
{
    oui::TextEdit edit;
    edit.set_use_spaces(true);
    edit.set_tab_spaces(2);
    edit.set_text("one\n\ntwo\nthree\nfour");
    // From the start of "one" to the start of "three": that last line is not taken in, nor the empty one.
    edit.select_range(0, 9);
    EXPECT_TRUE(edit.handle_event(oui::KeyEvent{oui::Key::tab}));
    EXPECT_EQ(edit.text(), "  one\n\n  two\nthree\nfour");
    // The selection still covers the same lines, indentation included.
    EXPECT_EQ(edit.selected_text(), "  one\n\n  two\n");
    EXPECT_TRUE(edit.handle_event(oui::KeyEvent{oui::Key::shift_tab}));
    EXPECT_EQ(edit.text(), "one\n\ntwo\nthree\nfour");
    EXPECT_TRUE(edit.handle_event(oui::KeyEvent{oui::Key::tab}));
    EXPECT_TRUE(edit.undo());
    EXPECT_EQ(edit.text(), "one\n\ntwo\nthree\nfour");
}

TEST(OuiWidgetTests, TextEditShiftTabOutdentsTheCursorLineAndTabStillReplacesAOneLineSelection)
{
    oui::TextEdit edit;
    edit.set_text("\tindented\nplain");
    edit.select_range(3, 3);
    EXPECT_TRUE(edit.handle_event(oui::KeyEvent{oui::Key::shift_tab}));
    EXPECT_EQ(edit.text(), "indented\nplain");
    EXPECT_EQ(edit.cursor_offset(), 2u);
    edit.select_range(9, 14);
    EXPECT_TRUE(edit.handle_event(oui::KeyEvent{oui::Key::tab}));
    EXPECT_EQ(edit.text(), "indented\n\t");
}

TEST(OuiWidgetTests, TextEditSearchHighlightCountsMatchesAndFollowsEdits)
{
    oui::TextEdit edit;
    edit.set_text("one two one two one");
    EXPECT_EQ(edit.search_match_count(), 0u);
    edit.set_search_highlight("one");
    EXPECT_EQ(edit.search_match_count(), 3u);
    EXPECT_EQ(edit.selected_search_match(), std::nullopt);
    EXPECT_TRUE(edit.find_next("one"));
    EXPECT_TRUE(edit.find_next("one"));
    EXPECT_EQ(edit.selected_search_match(), std::optional<std::size_t>{2});
    EXPECT_EQ(edit.replace_all("two", "one"), 2u);
    EXPECT_EQ(edit.search_match_count(), 5u);
    edit.clear_search_highlight();
    EXPECT_EQ(edit.search_match_count(), 0u);
}

TEST(OuiWidgetTests, TextEditPaintsSearchMatchesWithTheSearchColours)
{
    oui::TextEdit edit;
    edit.set_text("ab ab");
    edit.set_search_highlight("ab");
    edit.select_range(3, 5);
    edit.arrange({0, 0, 10, 3});
    oui::CellBuffer buffer{10, 3};
    oui::CellBufferPainter painter{buffer};
    const oui::Theme theme{};
    edit.paint(painter, theme);
    // The first match is highlighted; the second is the selection, which keeps the selection colours.
    const auto column_of{[&buffer](char32_t glyph, int from)
                         {
                             for(int x{from}; x < buffer.width(); ++ x)
                             {
                                 if(buffer.cell_at(x, 0).glyph == glyph)
                                 {
                                     return x;
                                 }
                             }
                             return -1;
                         }};
    const auto first{column_of(U'a', 0)};
    const auto second{column_of(U'a', first + 1)};
    ASSERT_GE(second, 0);
    EXPECT_EQ(buffer.cell_at(first, 0).background, theme.search_match_background);
    EXPECT_EQ(buffer.cell_at(first + 2, 0).background, theme.editor_background);
    EXPECT_EQ(buffer.cell_at(second, 0).background, theme.selection_background);
}

TEST(OuiWidgetTests, TextEditPairsTheBracketBesideTheCursorAcrossNestedOnes)
{
    oui::TextEdit edit;
    edit.set_text("f(a[1], {b}) )");
    edit.select_range(1, 1);
    EXPECT_EQ(edit.matching_brackets(), (std::pair<std::size_t, std::size_t>{1, 11}))
        << "the bracket after the cursor comes first";
    edit.select_range(12, 12);
    EXPECT_EQ(edit.matching_brackets(), (std::pair<std::size_t, std::size_t>{11, 1})) << "otherwise the one before it";
    edit.select_range(14, 14);
    EXPECT_FALSE(edit.matching_brackets().has_value()) << "a closing bracket nothing opened has no pair";
    edit.select_range(7, 7);
    EXPECT_FALSE(edit.matching_brackets().has_value()) << "no bracket beside the cursor";
    edit.set_bracket_pairs({{'<', '>'}});
    edit.set_text("(<x>)");
    edit.select_range(0, 0);
    EXPECT_FALSE(edit.matching_brackets().has_value()) << "only the pairs set count";
    edit.select_range(1, 1);
    EXPECT_EQ(edit.matching_brackets(), (std::pair<std::size_t, std::size_t>{1, 3}));
}

TEST(OuiWidgetTests, TextEditJumpsBetweenABracketAndItsPair)
{
    oui::TextEdit edit;
    edit.set_text("if(x)\n{\n    y();\n}");
    edit.select_range(6, 6);
    ASSERT_TRUE(edit.go_to_matching_bracket());
    EXPECT_EQ(edit.cursor_offset(), 17u);
    ASSERT_TRUE(edit.go_to_matching_bracket());
    EXPECT_EQ(edit.cursor_offset(), 6u);
    edit.select_range(0, 0);
    EXPECT_FALSE(edit.go_to_matching_bracket());
    EXPECT_EQ(edit.cursor_offset(), 0u);
}

TEST(OuiWidgetTests, TextEditPaintsTheBracketBesideTheCursorAndItsPair)
{
    oui::TextEdit edit;
    edit.set_line_numbers_visible(false);
    edit.set_text("(a)b");
    edit.select_range(0, 0);
    edit.arrange({0, 0, 10, 3});
    oui::CellBuffer buffer{10, 3};
    oui::CellBufferPainter painter{buffer};
    const oui::Theme theme{};
    edit.paint(painter, theme);
    EXPECT_EQ(buffer.cell_at(0, 0).background, theme.bracket_match_background);
    EXPECT_EQ(buffer.cell_at(1, 0).background, theme.editor_background);
    EXPECT_EQ(buffer.cell_at(2, 0).background, theme.bracket_match_background);
    EXPECT_EQ(buffer.cell_at(3, 0).background, theme.editor_background);
}

namespace
{

using Selections = std::vector<std::pair<std::size_t, std::size_t>>;

void type(oui::TextEdit& edit, std::string_view text)
{
    for(const auto character : text)
    {
        edit.handle_event(oui::TextEvent{static_cast<char32_t>(character)});
    }
}

void press(oui::TextEdit& edit, oui::Key key, bool shift = false)
{
    edit.handle_event(oui::KeyEvent{key, false, false, shift});
}

} // namespace

TEST(OuiWidgetTests, TextEditAddsCursorsAboveAndBelowAtTheSameColumn)
{
    oui::TextEdit edit;
    edit.set_text("abcd\nab\nabcd\nabcd");
    edit.arrange({0, 0, 20, 10});
    edit.select_range(11, 11);
    ASSERT_TRUE(edit.add_cursor_below());
    EXPECT_EQ(edit.cursor_count(), 2u);
    EXPECT_EQ(edit.selections(), (Selections{{11, 11}, {16, 16}}));
    EXPECT_FALSE(edit.add_cursor_below()) << "no line below the last";
    ASSERT_TRUE(edit.add_cursor_above());
    EXPECT_EQ(edit.selections(), (Selections{{7, 7}, {11, 11}, {16, 16}})) << "a short line takes its end";
    press(edit, oui::Key::escape);
    EXPECT_EQ(edit.cursor_count(), 1u);
    EXPECT_EQ(edit.cursor_offset(), 11u) << "the first cursor stays";
}

TEST(OuiWidgetTests, TextEditTypesAndDeletesAtEveryCursorAsOneStepToUndo)
{
    oui::TextEdit edit;
    edit.set_text("one\ntwo\nsix");
    edit.arrange({0, 0, 20, 10});
    edit.select_range(0, 0);
    ASSERT_TRUE(edit.add_cursor_below());
    ASSERT_TRUE(edit.add_cursor_below());
    type(edit, "> ");
    EXPECT_EQ(edit.text(), "> one\n> two\n> six");
    press(edit, oui::Key::end);
    press(edit, oui::Key::backspace);
    EXPECT_EQ(edit.text(), "> on\n> tw\n> si");
    ASSERT_TRUE(edit.undo());
    EXPECT_EQ(edit.text(), "> one\n> two\n> six");
    EXPECT_EQ(edit.cursor_count(), 3u) << "undo brings the cursors back";
    press(edit, oui::Key::home, true);
    std::string clipboard;
    edit.cut_selection(clipboard);
    EXPECT_EQ(clipboard, "> one\n> two\n> six");
    EXPECT_EQ(edit.text(), "\n\n");
    edit.paste("1\n2\n3");
    EXPECT_EQ(edit.text(), "1\n2\n3") << "as many lines as cursors give each cursor its line";
    edit.paste("x");
    EXPECT_EQ(edit.text(), "1x\n2x\n3x");
}

TEST(OuiWidgetTests, TextEditJoinsCursorsThatMeet)
{
    oui::TextEdit edit;
    edit.set_text("a\nb");
    edit.arrange({0, 0, 20, 10});
    edit.select_range(0, 0);
    ASSERT_TRUE(edit.add_cursor_below());
    press(edit, oui::Key::up);
    EXPECT_EQ(edit.cursor_count(), 1u) << "both cursors went to the start of the text";
    edit.select_range(0, 0);
    ASSERT_TRUE(edit.add_cursor_below());
    edit.handle_event(oui::MouseEvent{3, 0, true});
    EXPECT_EQ(edit.cursor_count(), 1u) << "a click leaves one cursor";
}

TEST(OuiWidgetTests, TextEditPaintsEveryCursorAndSelection)
{
    oui::TextEdit edit;
    edit.set_line_numbers_visible(false);
    edit.set_text("abc\nabc");
    edit.select_range(0, 0);
    edit.arrange({0, 0, 10, 3});
    ASSERT_TRUE(edit.add_cursor_below());
    press(edit, oui::Key::right, true);
    oui::CellBuffer buffer{10, 3};
    oui::CellBufferPainter painter{buffer};
    const oui::Theme theme{};
    edit.paint(painter, theme);
    EXPECT_EQ(buffer.cell_at(0, 0).background, theme.selection_background);
    EXPECT_EQ(buffer.cell_at(0, 1).background, theme.selection_background);
    EXPECT_EQ(buffer.cell_at(1, 1).background, theme.editor_background);
}

namespace
{

// "if" is a keyword, and /* ... */ a comment that may run over lines: state 1 is inside one.
class ToyHighlighter final : public oui::SyntaxHighlighter
{
  public:
    State highlight_line(std::string_view line, State state, std::vector<oui::SyntaxSpan>& spans) const override
    {
        ++ calls;
        std::size_t index{};
        while(index < line.size())
        {
            if(state == 1)
            {
                const auto close{line.find("*/", index)};
                const auto end{close == std::string_view::npos ? line.size() : close + 2};
                spans.push_back({index, end, oui::SyntaxKind::comment});
                state = close == std::string_view::npos ? 1 : 0;
                index = end;
            }
            else if(line.substr(index, 2) == "/*")
            {
                state = 1;
            }
            else if(line.substr(index, 2) == "if")
            {
                spans.push_back({index, index + 2, oui::SyntaxKind::keyword});
                index += 2;
            }
            else
            {
                ++ index;
            }
        }
        return state;
    }
    mutable int calls{};
};

} // namespace

TEST(OuiWidgetTests, TextEditColoursLinesByWhatTheHighlighterTells)
{
    oui::TextEdit edit;
    edit.set_line_numbers_visible(false);
    auto highlighter{std::make_shared<ToyHighlighter>()};
    edit.set_syntax_highlighter(highlighter);
    edit.set_text("if a\n/* x\ny */ if");
    const auto kinds{[&edit](std::size_t line)
                     {
                         std::vector<std::tuple<std::size_t, std::size_t, oui::SyntaxKind>> result;
                         for(const auto& span : edit.line_syntax(line))
                         {
                             result.emplace_back(span.start, span.end, span.kind);
                         }
                         return result;
                     }};
    using Kind = oui::SyntaxKind;
    EXPECT_EQ(kinds(0), (std::vector<std::tuple<std::size_t, std::size_t, Kind>>{{0, 2, Kind::keyword}}));
    EXPECT_EQ(kinds(2),
              (std::vector<std::tuple<std::size_t, std::size_t, Kind>>{{0, 4, Kind::comment}, {5, 7, Kind::keyword}}))
        << "the comment opened on the line before carries on";
    // Closing the comment early changes how the next line starts.
    edit.select_range(9, 9);
    edit.paste(" */");
    EXPECT_EQ(kinds(2), (std::vector<std::tuple<std::size_t, std::size_t, Kind>>{{5, 7, Kind::keyword}}));

    edit.arrange({0, 0, 20, 4});
    oui::CellBuffer buffer{20, 4};
    oui::CellBufferPainter painter{buffer};
    const oui::Theme theme{};
    edit.paint(painter, theme);
    EXPECT_EQ(buffer.cell_at(0, 0).foreground, theme.syntax_keyword);
    EXPECT_EQ(buffer.cell_at(3, 0).foreground, theme.editor_foreground);
    EXPECT_EQ(buffer.cell_at(0, 1).foreground, theme.syntax_comment);
}

TEST(OuiWidgetTests, TextEditColoursOnlyWhatChangedOrIsShown)
{
    oui::TextEdit edit;
    auto highlighter{std::make_shared<ToyHighlighter>()};
    edit.set_syntax_highlighter(highlighter);
    std::string text;
    for(int line{}; line < 1000; ++ line)
    {
        text += "if x\n";
    }
    edit.set_text(text);
    edit.arrange({0, 0, 40, 10});
    oui::CellBuffer buffer{40, 10};
    oui::CellBufferPainter painter{buffer};
    edit.paint(painter, oui::Theme{});
    EXPECT_LE(highlighter->calls, 10) << "only the lines shown";
    highlighter->calls = 0;
    edit.select_range(1, 1);
    edit.paste("f");
    edit.paint(painter, oui::Theme{});
    EXPECT_EQ(highlighter->calls, 1) << "only the line that changed";
}

TEST(OuiWidgetTests, TextEditShowsACursorPlacedBeforeItHadASize)
{
    oui::TextEdit edit;
    edit.set_line_numbers_visible(false);
    edit.set_text("short line\n" + std::string(200, 'x'));
    // Before arranging, at the start: nothing to scroll, and nothing to scroll back once arranged.
    edit.select_range(3, 3);
    edit.arrange({0, 0, 20, 5});
    oui::CellBuffer buffer{20, 5};
    oui::CellBufferPainter painter{buffer};
    edit.paint(painter, {});
    EXPECT_EQ(buffer.cell_at(0, 0).glyph, U's');
    // Far along the long line: arranging brings the cursor into view.
    oui::TextEdit far;
    far.set_line_numbers_visible(false);
    far.set_text("short line\n" + std::string(200, 'x'));
    far.select_range(200, 200);
    far.arrange({0, 0, 20, 5});
    oui::CellBuffer far_buffer{20, 5};
    oui::CellBufferPainter far_painter{far_buffer};
    far.paint(far_painter, {});
    EXPECT_EQ(far_buffer.cell_at(0, 1).glyph, U'x');
    EXPECT_NE(far_buffer.cell_at(0, 0).glyph, U's');
}

TEST(OuiWidgetTests, TextEditDuplicatesTheSelectedLinesBelowAndSelectsTheCopy)
{
    oui::TextEdit edit;
    edit.set_text("one\ntwo\nthree");
    edit.select_range(1, 5);
    EXPECT_TRUE(edit.duplicate_lines());
    EXPECT_EQ(edit.text(), "one\ntwo\none\ntwo\nthree");
    EXPECT_EQ(edit.selected_text(), "ne\nt");
    EXPECT_TRUE(edit.undo());
    EXPECT_EQ(edit.text(), "one\ntwo\nthree");
}

TEST(OuiWidgetTests, TextEditMovesLinesUpAndDownAndStopsAtTheEnds)
{
    oui::TextEdit edit;
    edit.set_text("one\ntwo\nthree");
    edit.select_range(5, 5);
    EXPECT_TRUE(edit.move_lines(false));
    EXPECT_EQ(edit.text(), "two\none\nthree");
    EXPECT_EQ(edit.cursor_offset(), 1u);
    EXPECT_FALSE(edit.move_lines(false));
    EXPECT_TRUE(edit.move_lines(true));
    EXPECT_TRUE(edit.move_lines(true));
    EXPECT_EQ(edit.text(), "one\nthree\ntwo");
    EXPECT_EQ(edit.cursor_offset(), 11u);
    EXPECT_FALSE(edit.move_lines(true));
}

TEST(OuiWidgetTests, TextEditDeletesTheSelectedLinesWithTheirLineBreak)
{
    oui::TextEdit edit;
    edit.set_text("one\ntwo\nthree");
    edit.select_range(5, 5);
    EXPECT_TRUE(edit.delete_lines());
    EXPECT_EQ(edit.text(), "one\nthree");
    EXPECT_EQ(edit.cursor_offset(), 4u);
    // The last line takes the line break before it, and the cursor goes up a line.
    EXPECT_TRUE(edit.delete_lines());
    EXPECT_EQ(edit.text(), "one");
    EXPECT_EQ(edit.cursor_offset(), 0u);
    EXPECT_TRUE(edit.delete_lines());
    EXPECT_EQ(edit.text(), "");
    EXPECT_FALSE(edit.delete_lines());
}

TEST(OuiWidgetTests, TextEditTogglesLineCommentsAtTheBlocksIndentation)
{
    oui::TextEdit edit;
    edit.set_text("    a();\n\n        b();\nc();");
    edit.select_range(0, 21);
    EXPECT_TRUE(edit.toggle_line_comment("//"));
    // The prefix goes in at the shallowest indentation; the blank line is left alone, and so is "c();",
    // outside the selection.
    EXPECT_EQ(edit.text(), "    // a();\n\n    //     b();\nc();");
    EXPECT_TRUE(edit.toggle_line_comment("//"));
    EXPECT_EQ(edit.text(), "    a();\n\n        b();\nc();");
    // A block with any line not commented gets commented as a whole.
    edit.set_text("# x\ny");
    edit.select_all();
    EXPECT_TRUE(edit.toggle_line_comment("#"));
    EXPECT_EQ(edit.text(), "# # x\n# y");
    EXPECT_TRUE(edit.undo());
    EXPECT_EQ(edit.text(), "# x\ny");
}

TEST(OuiWidgetTests, TextEditFindsPreviousMatchAndWrapsAround)
{
    oui::TextEdit edit;
    edit.set_text("one two one");

    EXPECT_TRUE(edit.find_next("one"));
    EXPECT_TRUE(edit.find_previous("one"));
    EXPECT_EQ(edit.selected_text(), "one");
    EXPECT_TRUE(edit.find_previous("one"));
    EXPECT_EQ(edit.selected_text(), "one");
}

TEST(OuiWidgetTests, TextEditMovesToAOneBasedLineNumber)
{
    oui::TextEdit edit;
    edit.set_text("one\ntwo\nthree");

    EXPECT_TRUE(edit.go_to_line(3));
    EXPECT_EQ(edit.line_column().first, 3u);
    EXPECT_EQ(edit.line_column().second, 1u);
    EXPECT_FALSE(edit.go_to_line(4));
}

TEST(OuiWidgetTests, TextEditExtendsSelectionWithShiftNavigation)
{
    oui::TextEdit editor;
    editor.set_text("año");
    editor.handle_event(oui::Event{oui::KeyEvent{oui::Key::end}});
    editor.handle_event(oui::Event{oui::KeyEvent{oui::Key::left, false, false, true}});
    editor.handle_event(oui::Event{oui::KeyEvent{oui::Key::left, false, false, true}});

    EXPECT_EQ(editor.selected_text(), "ño");
}

TEST(OuiWidgetTests, TextEditJumpsWordsWithControlArrows)
{
    oui::TextEdit editor;
    editor.set_text("foo.bar(año)  baz\nqux");
    const oui::KeyEvent select_word_right{oui::Key::right, true, false, true};
    const oui::KeyEvent select_word_left{oui::Key::left, true, false, true};

    const std::vector<std::string> forward{"foo",
                                           "foo.",
                                           "foo.bar",
                                           "foo.bar(",
                                           "foo.bar(año",
                                           "foo.bar(año)",
                                           "foo.bar(año)  baz",
                                           "foo.bar(año)  baz\nqux",
                                           "foo.bar(año)  baz\nqux"};
    for(const auto& expected : forward)
    {
        editor.handle_event(oui::Event{select_word_right});
        EXPECT_EQ(editor.selected_text(), expected);
    }

    editor.handle_event(oui::Event{oui::KeyEvent{oui::Key::end, true}});
    const std::vector<std::string> backward{"qux", "baz\nqux", ")  baz\nqux", "año)  baz\nqux"};
    for(const auto& expected : backward)
    {
        editor.handle_event(oui::Event{select_word_left});
        EXPECT_EQ(editor.selected_text(), expected);
    }

    // Without Shift the selection collapses at the new position.
    editor.handle_event(oui::Event{oui::KeyEvent{oui::Key::left, true}});
    editor.handle_event(oui::Event{oui::KeyEvent{oui::Key::right, false, false, true}});
    EXPECT_EQ(editor.selected_text(), "(");
}

namespace
{

oui::KeyEvent key(oui::Key value, bool control = false)
{
    return oui::KeyEvent{value, control};
}

void type(oui::TextEdit& editor, std::u32string_view text)
{
    for(const auto character : text)
    {
        editor.handle_event(oui::TextEvent{character});
    }
}

} // namespace

TEST(OuiWidgetTests, TextEditEnterKeepsTheIndentation)
{
    oui::TextEdit editor;
    editor.set_auto_indent(true);
    editor.set_text("    if(x)\n\tbody");
    editor.handle_event(key(oui::Key::end));
    editor.handle_event(key(oui::Key::enter));
    type(editor, U"y");
    EXPECT_EQ(editor.text(), "    if(x)\n    y\n\tbody");

    editor.handle_event(key(oui::Key::end, true));
    editor.handle_event(key(oui::Key::enter));
    type(editor, U"z");
    EXPECT_EQ(editor.text(), "    if(x)\n    y\n\tbody\n\tz");
}

TEST(OuiWidgetTests, TextEditControlBackspaceDeletesTheWordBefore)
{
    oui::TextEdit editor;
    editor.set_text("foo bar baz\n    x");
    editor.handle_event(key(oui::Key::end));
    editor.handle_event(key(oui::Key::backspace, true));
    EXPECT_EQ(editor.text(), "foo bar \n    x");

    // At the start of a line only the line break goes.
    editor.handle_event(key(oui::Key::end, true));
    editor.handle_event(key(oui::Key::backspace, true));
    EXPECT_EQ(editor.text(), "foo bar \n    ");
    editor.handle_event(key(oui::Key::backspace, true));
    EXPECT_EQ(editor.text(), "foo bar \n");
    editor.handle_event(key(oui::Key::backspace, true));
    EXPECT_EQ(editor.text(), "foo bar ");

    EXPECT_TRUE(editor.undo());
    EXPECT_EQ(editor.text(), "foo bar \n");
}

TEST(OuiWidgetTests, TextEditControlDeleteDeletesTheWordAfter)
{
    oui::TextEdit editor;
    editor.set_text("foo bar\nnext");
    editor.handle_event(key(oui::Key::delete_, true));
    EXPECT_EQ(editor.text(), " bar\nnext");
    editor.handle_event(key(oui::Key::end));
    editor.handle_event(key(oui::Key::delete_, true));
    EXPECT_EQ(editor.text(), " barnext");
}

TEST(OuiWidgetTests, TextEditDoubleClickSelectsAWordAndTripleClickTheLine)
{
    oui::TextEdit editor;
    editor.set_line_numbers_visible(false);
    editor.set_text("foo bar_baz(qux)\nnext");
    editor.arrange({0, 0, 30, 4});
    oui::MouseEvent double_click{6, 0, true};
    double_click.clicks = 2;
    editor.handle_event(double_click);
    editor.handle_event(oui::MouseEvent{6, 0, false, 0, false, true});
    EXPECT_EQ(editor.selected_text(), "bar_baz");

    oui::MouseEvent triple_click{6, 0, true};
    triple_click.clicks = 3;
    editor.handle_event(triple_click);
    editor.handle_event(oui::MouseEvent{6, 0, false, 0, false, true});
    EXPECT_EQ(editor.selected_text(), "foo bar_baz(qux)\n");
}

TEST(OuiWidgetTests, TextEditShiftClickExtendsTheSelection)
{
    oui::TextEdit editor;
    editor.set_line_numbers_visible(false);
    editor.set_text("hello world");
    editor.arrange({0, 0, 30, 2});
    editor.handle_event(oui::MouseEvent{2, 0, true});
    editor.handle_event(oui::MouseEvent{2, 0, false, 0, false, true});
    oui::MouseEvent shift_click{8, 0, true};
    shift_click.shift = true;
    editor.handle_event(shift_click);
    editor.handle_event(oui::MouseEvent{8, 0, false, 0, false, true});
    EXPECT_EQ(editor.selected_text(), "llo wo");
}

TEST(OuiWidgetTests, TextEditOnlyIndentsNewLinesWhenAskedTo)
{
    oui::TextEdit editor;
    editor.set_text("    code");
    editor.handle_event(oui::KeyEvent{oui::Key::end});
    editor.handle_event(oui::KeyEvent{oui::Key::enter});
    EXPECT_EQ(editor.text(), "    code\n");
}

TEST(OuiWidgetTests, TextEditSearchesWithOptions)
{
    oui::TextEdit editor;
    editor.set_text("Cat catalog cat CAT");
    oui::SearchOptions options;
    options.match_case = false;
    options.whole_word = true;
    EXPECT_TRUE(editor.find_next("cat", options));
    EXPECT_EQ(editor.cursor_offset(), 3u) << "whole word skips catalog";
    EXPECT_TRUE(editor.find_next("cat", options));
    EXPECT_EQ(editor.cursor_offset(), 15u);
    EXPECT_TRUE(editor.find_previous("cat", options));
    EXPECT_EQ(editor.cursor_offset(), 3u);

    options = {};
    EXPECT_TRUE(editor.find_next("CAT", options));
    EXPECT_EQ(editor.selected_text(), "CAT");

    options.regex = true;
    editor.select_range(0, 0);
    EXPECT_TRUE(editor.find_next("c[a-z]+g", options));
    EXPECT_EQ(editor.selected_text(), "catalog");
    EXPECT_FALSE(editor.find_next("([", options)) << "an invalid expression finds nothing";
}

TEST(OuiWidgetTests, TextEditReadOnlyRefusesEditsButKeepsNavigating)
{
    oui::TextEdit editor;
    editor.set_text("fixed text");
    editor.set_read_only(true);
    editor.handle_event(oui::TextEvent{U'x'});
    editor.handle_event(oui::KeyEvent{oui::Key::delete_});
    editor.handle_event(oui::KeyEvent{oui::Key::enter});
    editor.paste("pasted");
    EXPECT_EQ(editor.text(), "fixed text");
    EXPECT_FALSE(editor.is_modified());
    editor.handle_event(oui::KeyEvent{oui::Key::right, true});
    EXPECT_EQ(editor.cursor_offset(), 5u);
}

TEST(OuiWidgetTests, TextEditTabStopsFollowTheTabDisplayWidth)
{
    oui::TextEdit editor;
    editor.set_scrollbar_visible(false);
    editor.set_tab_display_width(8);
    editor.set_text("\tx");
    editor.arrange({0, 0, 20, 1});
    oui::CellBuffer buffer{20, 1};
    oui::CellBufferPainter painter{buffer};
    editor.paint(painter, {});
    EXPECT_EQ(buffer.cell_at(8, 0).glyph, U'x');
}

TEST(OuiWidgetTests, TextEditAnnouncesTextAndCursorChanges)
{
    oui::TextEdit editor;
    editor.set_text("ab");
    auto changes{0};
    auto moves{0};
    const auto changed{editor.on_changed([&changes] { ++ changes; })};
    const auto moved{editor.on_cursor_moved([&moves] { ++ moves; })};
    editor.handle_event(oui::KeyEvent{oui::Key::right});
    EXPECT_EQ(changes, 0);
    EXPECT_EQ(moves, 1);
    editor.handle_event(oui::TextEvent{U'x'});
    EXPECT_EQ(changes, 1);
    EXPECT_EQ(moves, 2);
    EXPECT_TRUE(editor.undo());
    EXPECT_EQ(changes, 2);
}

TEST(OuiWidgetTests, TextEditMovesTheCursorToTheClickedCell)
{
    oui::TextEdit edit;
    edit.set_text("first\nsecond");
    edit.arrange({0, 0, 20, 4});

    EXPECT_TRUE(edit.handle_event(oui::MouseEvent{3, 1, true}));
    const auto [line, column]{edit.line_column()};
    EXPECT_EQ(line, 2u);
    EXPECT_EQ(column, 4u);
}

TEST(OuiWidgetTests, TextEditExtendsTheSelectionWhileDraggingTheMouse)
{
    oui::TextEdit edit;
    edit.set_text("hello");
    edit.arrange({0, 0, 20, 1});

    EXPECT_TRUE(edit.handle_event(oui::MouseEvent{1, 0, true}));
    EXPECT_TRUE(edit.handle_event(oui::MouseEvent{4, 0, false, 0, true}));
    EXPECT_TRUE(edit.handle_event(oui::MouseEvent{4, 0, false, 0, false, true}));
    EXPECT_EQ(edit.selected_text(), "ello");
}

TEST(OuiWidgetTests, TextEditUndoesConsecutiveTextEventsAsOneChange)
{
    oui::TextEdit edit;
    static_cast<void>(edit.handle_event(oui::TextEvent{U'a'}));
    static_cast<void>(edit.handle_event(oui::TextEvent{U'b'}));
    static_cast<void>(edit.handle_event(oui::TextEvent{U'ñ'}));

    EXPECT_EQ(edit.text(), "abñ");
    EXPECT_TRUE(edit.undo());
    EXPECT_TRUE(edit.text().empty());
}

TEST(OuiWidgetTests, TextEditPaintsSelectedSpacesWithTheSelectionBackground)
{
    oui::TextEdit edit;
    edit.set_text("a b");
    edit.arrange({0, 0, 10, 1});
    static_cast<void>(edit.handle_event(oui::MouseEvent{1, 0, true}));
    static_cast<void>(edit.handle_event(oui::MouseEvent{2, 0, false, 0, true}));
    oui::CellBuffer buffer{10, 1};
    oui::CellBufferPainter painter{buffer};
    edit.paint(painter, {});

    EXPECT_EQ(buffer.cell_at(1, 0).background, oui::Theme{}.selection_background);
}

TEST(OuiWidgetTests, TextEditMovesToDocumentEdgesWithControlHomeAndEnd)
{
    oui::TextEdit edit;
    edit.set_text("first\nsecond");
    edit.arrange({0, 0, 20, 4});
    static_cast<void>(edit.handle_event(oui::KeyEvent{oui::Key::end, true}));

    EXPECT_EQ(edit.line_column().first, 2u);
    EXPECT_EQ(edit.line_column().second, 7u);
    static_cast<void>(edit.handle_event(oui::KeyEvent{oui::Key::home, true}));
    EXPECT_EQ(edit.line_column().first, 1u);
    EXPECT_EQ(edit.line_column().second, 1u);
}

TEST(OuiWidgetTests, TextEditPaintsLineNumbersWhenEnabled)
{
    oui::TextEdit edit;
    edit.set_line_numbers_visible(true);
    edit.set_text("one\ntwo");
    edit.set_word_wrap(true);
    edit.arrange({0, 0, 10, 2});
    oui::CellBuffer buffer{10, 2};
    oui::CellBufferPainter painter{buffer};
    edit.paint(painter, {});

    EXPECT_EQ(buffer.cell_at(0, 0).glyph, U'1');
    EXPECT_EQ(buffer.cell_at(0, 1).glyph, U'2');
    EXPECT_EQ(buffer.cell_at(2, 0).glyph, U'o');
}

TEST(OuiWidgetTests, TextEditPaintsABlockCursorOverTabCharacters)
{
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_reference{backend.get()};
    oui::Application application{std::move(backend)};
    auto editor{oui::text_edit()};
    editor->set_text("\tx");
    application.set_content(std::move(editor));
    application.render();

    ASSERT_TRUE(backend_reference->last_frame().has_value());
    const auto& frame{*backend_reference->last_frame()};
    EXPECT_EQ(frame.cell_at(0, 0).background, oui::Theme{}.editor_foreground);
}

TEST(OuiWidgetTests, TextEditScrollsWithTheMouseWheel)
{
    oui::TextEdit edit;
    edit.set_text("one\ntwo\nthree");
    edit.arrange({0, 0, 10, 2});

    EXPECT_TRUE(edit.handle_event(oui::MouseEvent{0, 0, false, -1}));
    oui::CellBuffer buffer{10, 2};
    oui::CellBufferPainter painter{buffer};
    edit.paint(painter, {});
    EXPECT_EQ(buffer.cell_at(0, 0).glyph, U't');
}

namespace
{

// Twenty lines named L00..L19 in a 20x5 editor: the scrollbar sits in column 19.
std::unique_ptr<oui::TextEdit> twenty_line_editor()
{
    auto editor{oui::text_edit()};
    std::string text;
    for(auto line{0}; line < 20; ++ line)
    {
        text += (line < 10 ? "L0" : "L") + std::to_string(line) + (line < 19 ? "\n" : "");
    }
    editor->set_text(text);
    // Wrapping short lines changes nothing but removes the horizontal bar, so all five rows hold text.
    editor->set_word_wrap(true);
    editor->arrange({0, 0, 20, 5});
    return editor;
}

oui::CellBuffer painted(const oui::TextEdit& editor)
{
    oui::CellBuffer buffer{20, 5};
    oui::CellBufferPainter painter{buffer};
    editor.paint(painter, oui::Theme{});
    return buffer;
}

// Rows of the scrollbar column that show the thumb.
std::vector<int> thumb_rows(const oui::CellBuffer& buffer)
{
    std::vector<int> rows;
    for(auto y{0}; y < buffer.height(); ++ y)
    {
        if(buffer.cell_at(19, y).glyph == U'■')
        {
            rows.push_back(y);
        }
    }
    return rows;
}

std::string first_visible_line(const oui::CellBuffer& buffer)
{
    return {static_cast<char>(buffer.cell_at(0, 0).glyph), static_cast<char>(buffer.cell_at(1, 0).glyph),
            static_cast<char>(buffer.cell_at(2, 0).glyph)};
}

} // namespace

TEST(OuiWidgetTests, TextEditScrollbarShowsWhereTheViewIs)
{
    auto editor{twenty_line_editor()};
    const oui::Theme theme;
    const auto top{painted(*editor)};
    EXPECT_EQ(top.cell_at(19, 3).background, theme.scrollbar_background);
    // A one-cell indicator: at the top, then at the bottom.
    EXPECT_EQ(thumb_rows(top), (std::vector<int>{0}));

    editor->handle_event(oui::KeyEvent{oui::Key::end, true});
    EXPECT_EQ(thumb_rows(painted(*editor)), (std::vector<int>{4}));
}

TEST(OuiWidgetTests, TextEditScrollbarRestsAtTheTopWhenEverythingFits)
{
    oui::TextEdit editor;
    editor.set_text("one\ntwo");
    editor.set_word_wrap(true);
    editor.arrange({0, 0, 20, 5});
    EXPECT_EQ(thumb_rows(painted(editor)), (std::vector<int>{0}));
}

TEST(OuiWidgetTests, TextEditScrollbarTrackClickPagesWithoutMovingTheCursor)
{
    auto editor{twenty_line_editor()};
    editor->handle_event(oui::MouseEvent{19, 3, true});
    editor->handle_event(oui::MouseEvent{19, 3, false, 0, false, true});
    EXPECT_EQ(first_visible_line(painted(*editor)), "L04");

    editor->handle_event(oui::MouseEvent{19, 0, true});
    editor->handle_event(oui::MouseEvent{19, 0, false, 0, false, true});
    EXPECT_EQ(first_visible_line(painted(*editor)), "L00");

    editor->handle_event(oui::TextEvent{U'X'});
    EXPECT_EQ(editor->text().substr(0, 4), "XL00");
}

TEST(OuiWidgetTests, TextEditScrollbarThumbDragsTheView)
{
    auto editor{twenty_line_editor()};
    editor->handle_event(oui::MouseEvent{19, 0, true});
    editor->handle_event(oui::MouseEvent{19, 4, false, 0, true});
    EXPECT_EQ(first_visible_line(painted(*editor)), "L15");
    editor->handle_event(oui::MouseEvent{19, 2, false, 0, true});
    EXPECT_EQ(first_visible_line(painted(*editor)), "L08");
    editor->handle_event(oui::MouseEvent{19, 2, false, 0, false, true});
    EXPECT_EQ(editor->selected_text(), "");
}

namespace
{

// A 20x5 editor, without wrapping, holding one line of 60 digits and a short one: text in rows 0-3,
// the horizontal scrollbar in row 4 under the 19 text columns.
std::unique_ptr<oui::TextEdit> wide_line_editor()
{
    auto editor{oui::text_edit()};
    std::string line;
    for(auto index{0}; index < 60; ++ index)
    {
        line += static_cast<char>('0' + index % 10);
    }
    editor->set_text(line + "\nshort");
    editor->set_word_wrap(false);
    editor->arrange({0, 0, 20, 5});
    return editor;
}

std::vector<int> horizontal_thumb(const oui::CellBuffer& buffer)
{
    std::vector<int> columns;
    for(auto x{0}; x < 19; ++ x)
    {
        if(buffer.cell_at(x, 4).glyph == U'■')
        {
            columns.push_back(x);
        }
    }
    return columns;
}

} // namespace

TEST(OuiWidgetTests, TextEditHorizontalScrollbarAppearsWithoutWordWrap)
{
    auto editor{wide_line_editor()};
    const oui::Theme theme;
    const auto start{painted(*editor)};
    EXPECT_EQ(start.cell_at(10, 4).background, theme.scrollbar_background);
    EXPECT_EQ(horizontal_thumb(start), (std::vector<int>{0})) << "at the start";

    editor->handle_event(oui::KeyEvent{oui::Key::end});
    EXPECT_EQ(horizontal_thumb(painted(*editor)), (std::vector<int>{18})) << "at the end";

    editor->set_word_wrap(true);
    editor->arrange({0, 0, 20, 5});
    EXPECT_TRUE(horizontal_thumb(painted(*editor)).empty());
}

TEST(OuiWidgetTests, TextEditHorizontalScrollbarPagesAndDrags)
{
    auto editor{wide_line_editor()};
    editor->handle_event(oui::MouseEvent{15, 4, true});
    editor->handle_event(oui::MouseEvent{15, 4, false, 0, false, true});
    EXPECT_EQ(painted(*editor).cell_at(0, 0).glyph, U'8') << "one page is 18 columns";

    editor->handle_event(oui::MouseEvent{static_cast<double>(horizontal_thumb(painted(*editor)).front()), 4, true});
    editor->handle_event(oui::MouseEvent{0, 4, false, 0, true});
    editor->handle_event(oui::MouseEvent{0, 4, false, 0, false, true});
    EXPECT_EQ(painted(*editor).cell_at(0, 0).glyph, U'0');

    editor->handle_event(oui::TextEvent{U'X'});
    EXPECT_EQ(editor->text().substr(0, 2), "X0") << "the scrollbar never moved the cursor";
}

TEST(OuiWidgetTests, TextEditScrollbarCanBeHidden)
{
    oui::TextEdit editor;
    editor.set_scrollbar_visible(false);
    editor.set_text("abcdefghijklmnopqrstuvwxyz");
    editor.arrange({0, 0, 20, 5});
    EXPECT_EQ(painted(editor).cell_at(19, 0).glyph, U't');
}

TEST(OuiWidgetTests, TextEditWordWrapsImmediatelyAfterTheLastAvailableColumn)
{
    oui::TextEdit edit;
    edit.set_text("abcd");
    edit.set_word_wrap(true);
    // The wrap is measured against the full width, so the scrollbar is kept out of the way.
    edit.set_scrollbar_visible(false);
    edit.arrange({0, 0, 3, 2});
    oui::CellBuffer buffer{3, 2};
    oui::CellBufferPainter painter{buffer};
    edit.paint(painter, {});

    EXPECT_EQ(buffer.cell_at(0, 0).glyph, U'a');
    EXPECT_EQ(buffer.cell_at(2, 0).glyph, U'c');
    EXPECT_EQ(buffer.cell_at(0, 1).glyph, U'd');
}

TEST(OuiWidgetTests, TextEditWordWrapPreservesTheCharacterThatTriggersTheWrap)
{
    oui::TextEdit edit;
    edit.set_text("one two");
    edit.set_word_wrap(true);
    edit.set_scrollbar_visible(false);
    edit.arrange({0, 0, 4, 2});
    oui::CellBuffer buffer{4, 2};
    oui::CellBufferPainter painter{buffer};
    edit.paint(painter, {});

    EXPECT_EQ(buffer.cell_at(0, 0).glyph, U'o');
    EXPECT_EQ(buffer.cell_at(3, 0).glyph, U' ');
    EXPECT_EQ(buffer.cell_at(0, 1).glyph, U't');
    EXPECT_EQ(buffer.cell_at(2, 1).glyph, U'o');
}

TEST(OuiWidgetTests, TextEditCursorUsesTheNextVisualLineAtAWordWrapBoundary)
{
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_reference{backend.get()};
    oui::Application application{std::move(backend)};
    auto editor{oui::text_edit()};
    editor->set_word_wrap(true);
    editor->set_scrollbar_visible(false);
    editor->set_text("abcdefghijklmnopqrstu");
    application.set_content(std::move(editor));
    application.render();

    for(std::size_t index{}; index < 20; ++ index)
    {
        application.dispatch_event(oui::KeyEvent{oui::Key::right});
    }
    application.render();

    ASSERT_TRUE(backend_reference->last_frame().has_value());
    const auto& first_boundary_frame{*backend_reference->last_frame()};
    const auto theme{oui::Theme{}};
    EXPECT_EQ(first_boundary_frame.cell_at(0, 1).glyph, U'u');
    EXPECT_EQ(first_boundary_frame.cell_at(0, 1).background, theme.editor_foreground);

    application.dispatch_event(oui::KeyEvent{oui::Key::right});
    application.render();

    const auto& after_boundary_frame{*backend_reference->last_frame()};
    EXPECT_EQ(after_boundary_frame.cell_at(0, 1).glyph, U'u');
    EXPECT_EQ(after_boundary_frame.cell_at(0, 1).background, theme.editor_background);
    EXPECT_EQ(after_boundary_frame.cell_at(1, 1).background, theme.editor_foreground);
}

TEST(OuiWidgetTests, TextEditCursorAfterAWrappedLineThatFillsTheWidthShowsOnTheNextRow)
{
    // At the end of the text a line as wide as the editor would leave the cursor under the scrollbar, or past
    // the edge beside a split's divider.
    for(const auto scrollbar : {true, false})
    {
        auto backend{std::make_unique<MemoryBackend>()};
        auto* const backend_reference{backend.get()};
        oui::Application application{std::move(backend)};
        auto editor{oui::text_edit()};
        auto* const editor_reference{editor.get()};
        editor->set_word_wrap(true);
        editor->set_scrollbar_visible(scrollbar);
        application.set_content(std::move(editor));
        application.render();
        const auto width{backend_reference->last_frame()->width() - (scrollbar ? 1 : 0)};
        editor_reference->set_text(std::string(static_cast<std::size_t>(width), 'a') + "\nb");
        application.dispatch_event(oui::KeyEvent{oui::Key::end});
        application.render();

        const auto& frame{*backend_reference->last_frame()};
        const oui::Theme theme;
        EXPECT_EQ(frame.cell_at(0, 1).background, theme.editor_foreground) << "the cursor, on a row of its own";
        EXPECT_EQ(frame.cell_at(0, 2).glyph, U'b') << "the next line below it";
        application.dispatch_event(oui::KeyEvent{oui::Key::down});
        EXPECT_EQ(editor_reference->line_column(), (std::pair<std::size_t, std::size_t>{2, 1}));
    }
}

TEST(OuiWidgetTests, TextEditHomeAndEndGoToTheEdgesOfTheWrappedRowFirst)
{
    oui::TextEdit edit;
    edit.set_text("one two three");
    edit.set_word_wrap(true);
    edit.set_scrollbar_visible(false);
    // "one two " on the first row, "three" on the second.
    edit.arrange({0, 0, 10, 3});
    const auto column{[&edit] { return edit.line_column().second; }};

    edit.handle_event(oui::KeyEvent{oui::Key::end, true});
    edit.handle_event(oui::KeyEvent{oui::Key::home});
    EXPECT_EQ(column(), 9u) << "the start of the row";
    edit.handle_event(oui::KeyEvent{oui::Key::home});
    EXPECT_EQ(column(), 1u) << "then the start of the line";

    edit.handle_event(oui::KeyEvent{oui::Key::end});
    EXPECT_EQ(column(), 8u) << "the end of the row, before the space it wraps at";
    edit.handle_event(oui::KeyEvent{oui::Key::end});
    EXPECT_EQ(column(), 14u) << "then the end of the line";

    edit.handle_event(oui::KeyEvent{oui::Key::home});
    edit.handle_event(oui::KeyEvent{oui::Key::home});
    EXPECT_EQ(column(), 1u) << "from the second row its start, then the line's";
    edit.handle_event(oui::KeyEvent{oui::Key::end, false, false, true});
    EXPECT_EQ(edit.selected_text(), "one two") << "Shift extends to the row's end";
}

TEST(OuiWidgetTests, TextEditCursorKeepsTheLogicalLineAtItsNewlineBoundary)
{
    oui::TextEdit edit;
    edit.set_text("one\ntwo");
    edit.arrange({0, 0, 10, 2});

    EXPECT_TRUE(edit.handle_event(oui::KeyEvent{oui::Key::end}));
    EXPECT_EQ(edit.line_column().first, 1u);
    EXPECT_EQ(edit.line_column().second, 4u);

    EXPECT_TRUE(edit.handle_event(oui::KeyEvent{oui::Key::down}));
    EXPECT_EQ(edit.line_column().first, 2u);
    EXPECT_EQ(edit.line_column().second, 4u);
}

TEST(OuiWidgetTests, TextEditPageDownKeepsOneVisibleRowOfOverlap)
{
    oui::TextEdit edit;
    edit.set_text("one\ntwo\nthree\nfour\nfive\nsix\nseven\neight\nnine");
    // No horizontal bar: all three rows hold text.
    edit.set_word_wrap(true);
    edit.arrange({0, 0, 10, 3});

    EXPECT_TRUE(edit.handle_event(oui::KeyEvent{oui::Key::page_down}));
    EXPECT_EQ(edit.line_column().first, 3u);
    EXPECT_EQ(edit.line_column().second, 1u);

    EXPECT_TRUE(edit.handle_event(oui::KeyEvent{oui::Key::page_up}));
    EXPECT_EQ(edit.line_column().first, 1u);

    EXPECT_TRUE(edit.go_to_line(2));
    EXPECT_TRUE(edit.handle_event(oui::KeyEvent{oui::Key::page_down}));
    EXPECT_EQ(edit.line_column().first, 4u);
    EXPECT_TRUE(edit.handle_event(oui::KeyEvent{oui::Key::page_up}));
    EXPECT_EQ(edit.line_column().first, 2u);
}

TEST(OuiWidgetTests, FindsWidgetsByIdAndType)
{
    auto root{oui::column()};
    auto& editor{oui::line_edit(*root)};
    editor.set_id("project_name");

    EXPECT_TRUE(root->find<oui::LineEdit>("project_name").has_value());
    EXPECT_EQ(&root->get<oui::LineEdit>("project_name"), &editor);
    EXPECT_FALSE(root->find<oui::Button>("project_name").has_value());
}

TEST(OuiApplicationTests, FocusesButtonsDispatchesKeysAndRendersTheTree)
{
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_reference{backend.get()};
    oui::Application application{std::move(backend)};
    auto content{oui::row()};
    auto calls{0};
    auto& first{oui::button("First", *content, [&calls] { ++ calls; })};
    auto& second{oui::button("Second", *content, [&calls] { calls += 10; })};

    application.set_content(std::move(content));
    EXPECT_TRUE(first.is_focused());
    application.dispatch_event(oui::KeyEvent{oui::Key::enter});
    application.dispatch_event(oui::KeyEvent{oui::Key::tab});
    EXPECT_TRUE(second.is_focused());
    application.dispatch_event(oui::KeyEvent{oui::Key::space});
    application.render();

    EXPECT_EQ(calls, 11);
    ASSERT_TRUE(backend_reference->last_frame().has_value());
    EXPECT_EQ(backend_reference->last_frame()->cell_at(2, 0).glyph, U'F');
    EXPECT_EQ(backend_reference->last_frame()->cell_at(12, 0).glyph, U'S');
}

TEST(OuiApplicationTests, LetsTheFocusedTextEditConsumeTabBeforeFocusTraversal)
{
    auto backend{std::make_unique<MemoryBackend>()};
    oui::Application application{std::move(backend)};
    auto edit{oui::text_edit()};
    auto* const edit_reference{edit.get()};
    application.set_content(std::move(edit));

    application.dispatch_event(oui::KeyEvent{oui::Key::tab});

    EXPECT_EQ(edit_reference->text(), "\t");
}

TEST(OuiApplicationTests, LetsTabLeaveAReadOnlyTextEditBothWays)
{
    auto backend{std::make_unique<MemoryBackend>()};
    oui::Application application{std::move(backend)};
    auto content{oui::column()};
    auto edit{oui::text_edit()};
    edit->set_read_only(true);
    auto& edit_reference{content->add(std::move(edit))};
    auto& button{oui::button("Close", *content, [] {})};
    application.set_content(std::move(content));
    application.focus(edit_reference);

    application.dispatch_event(oui::KeyEvent{oui::Key::tab});
    EXPECT_TRUE(button.is_focused());
    application.dispatch_event(oui::KeyEvent{oui::Key::shift_tab});
    EXPECT_TRUE(edit_reference.is_focused());
    application.dispatch_event(oui::KeyEvent{oui::Key::shift_tab});
    EXPECT_TRUE(button.is_focused());
    EXPECT_EQ(edit_reference.text(), "");
}

TEST(OuiTreeViewTests, ExpandsBranchesAndNavigatesVisibleNodes)
{
    oui::TreeView tree;
    auto& source{tree.add("source")};
    source.add("main.cpp");
    tree.add("readme.md");

    EXPECT_EQ(tree.measure({80, 25}).height, 2);
    EXPECT_TRUE(tree.handle_event(oui::KeyEvent{oui::Key::right}));
    EXPECT_EQ(tree.measure({80, 25}).height, 3);
    EXPECT_TRUE(tree.handle_event(oui::KeyEvent{oui::Key::down}));
    EXPECT_TRUE(tree.handle_event(oui::KeyEvent{oui::Key::up}));
    EXPECT_TRUE(tree.handle_event(oui::KeyEvent{oui::Key::left}));
    EXPECT_EQ(tree.measure({80, 25}).height, 2);
}

TEST(OuiTreeViewTests, MeasuresItsPrefixesAsPartOfItsWidth)
{
    oui::TreeView tree;
    tree.add("Command");

    EXPECT_EQ(tree.measure({80, 25}).width, 9);
}

TEST(OuiTreeViewTests, StaleSelectionDoesNotThrowOnNavigationKeys)
{
    oui::TreeView tree;
    auto& root{tree.add("Root")};
    root.expanded = true;
    root.add("First");
    root.add("Second");
    tree.add("Tail");
    tree.arrange({0, 0, 40, 10});

    static_cast<void>(tree.handle_event(oui::KeyEvent{oui::Key::down}));
    static_cast<void>(tree.handle_event(oui::KeyEvent{oui::Key::down}));
    static_cast<void>(tree.handle_event(oui::KeyEvent{oui::Key::down}));

    root.expanded = false;
    bool threw{false};
    try
    {
        static_cast<void>(tree.handle_event(oui::KeyEvent{oui::Key::right}));
        static_cast<void>(tree.handle_event(oui::KeyEvent{oui::Key::left}));
    }
    catch(const std::exception&)
    {
        threw = true;
    }
    EXPECT_FALSE(threw) << "navigation keys with a stale selection must clamp instead of throwing";
}

TEST(OuiTreeViewTests, HighlightsTheSelectionOnlyWhenFocused)
{
    oui::TreeView tree;
    tree.add("source");
    tree.arrange({0, 0, 10, 1});
    oui::CellBuffer unfocused_buffer{10, 1};
    oui::CellBufferPainter unfocused_painter{unfocused_buffer};
    tree.paint(unfocused_painter, {});
    EXPECT_EQ(unfocused_buffer.cell_at(0, 0).background, oui::Theme{}.background);
}

TEST(OuiTreeViewTests, HighlightsTheFullRowWhenItsFocusIsDelegated)
{
    oui::TreeView tree;
    tree.add("source");
    tree.arrange({0, 0, 10, 1});
    tree.set_focus_proxy_active(true);

    oui::Theme theme{};
    theme.item_selected_background = {12, 34, 56};
    oui::CellBuffer buffer{10, 1};
    oui::CellBufferPainter painter{buffer};
    tree.paint(painter, theme);

    EXPECT_EQ(buffer.cell_at(9, 0).background, theme.item_selected_background);
}

TEST(OuiTreeViewTests, ActivatesTheClickedNode)
{
    oui::TreeView tree;
    tree.add("main.cpp");
    std::size_t activated{std::numeric_limits<std::size_t>::max()};
    tree.on_activated([&activated](std::size_t index) { activated = index; });
    tree.arrange({0, 0, 20, 4});

    EXPECT_TRUE(tree.handle_event(oui::Event{oui::MouseEvent{1, 0, true}}));
    EXPECT_TRUE(tree.handle_event(oui::Event{oui::MouseEvent{1, 0, false, 0, false, true}}));
    EXPECT_EQ(activated, 0u);
}

TEST(OuiTreeViewTests, ActivatesTheVisualRowWhenTopPaddingAndIndentationAreConfigured)
{
    oui::TreeView tree;
    auto& root{tree.add("This PC")};
    root.expanded = true;
    root.add("Home");
    root.add("Desktop");
    root.add("Documents");
    root.add("Downloads");
    root.add("");
    root.add("C:\\");
    root.add("G:\\");
    tree.set_show_expanders(false);
    tree.set_indentation(2);
    tree.set_top_padding(1);

    std::size_t activated{std::numeric_limits<std::size_t>::max()};
    tree.on_activated([&activated](std::size_t index) { activated = index; });
    tree.arrange({0, 0, 12, 12});

    EXPECT_TRUE(tree.handle_event(oui::MouseEvent{1, 5, true}));
    EXPECT_TRUE(tree.handle_event(oui::MouseEvent{1, 5, false, 0, false, true}));
    EXPECT_EQ(activated, 4u);
}

TEST(OuiTreeViewTests, ScrollsWithTheMouseWheelAndKeepsKeyboardSelectionVisible)
{
    oui::TreeView tree;
    for(int index{}; index < 6; ++ index)
    {
        tree.add("Item " + std::to_string(index));
    }
    tree.arrange({0, 0, 16, 2});

    EXPECT_TRUE(tree.handle_event(oui::MouseEvent{0, 0, false, -1}));
    oui::CellBuffer buffer{16, 2};
    oui::CellBufferPainter painter{buffer};
    tree.paint(painter, {});
    EXPECT_EQ(buffer.cell_at(2, 0).glyph, U'I');
    EXPECT_EQ(buffer.cell_at(7, 0).glyph, U'3');

    for(int index{}; index < 5; ++ index)
    {
        static_cast<void>(tree.handle_event(oui::KeyEvent{oui::Key::down}));
    }
    tree.paint(painter, {});
    EXPECT_EQ(buffer.cell_at(7, 1).glyph, U'5');
}

TEST(OuiTreeViewTests, UsesTheThemeColorsForItsVerticalScrollbar)
{
    oui::TreeView tree;
    for(int index{}; index < 6; ++ index)
    {
        tree.add("Item " + std::to_string(index));
    }
    tree.arrange({0, 0, 8, 2});

    oui::Theme theme{};
    theme.scrollbar_background = {12, 34, 56};
    theme.scrollbar_foreground = {210, 220, 230};
    oui::CellBuffer buffer{8, 2};
    oui::CellBufferPainter painter{buffer};
    tree.paint(painter, theme);

    EXPECT_EQ(buffer.cell_at(7, 0).glyph, U'■');
    EXPECT_EQ(buffer.cell_at(7, 0).foreground, theme.scrollbar_foreground);
    EXPECT_EQ(buffer.cell_at(7, 1).background, theme.scrollbar_background);
}

TEST(OuiTreeViewTests, DragsItsScrollbarToTheMatchingScrollPosition)
{
    oui::TreeView tree;
    for(int index{}; index < 20; ++ index)
    {
        tree.add("Item " + std::to_string(index));
    }
    tree.arrange({0, 0, 10, 4});

    EXPECT_TRUE(tree.handle_event(oui::MouseEvent{9, 0, true}));
    EXPECT_TRUE(tree.handle_event(oui::MouseEvent{9, 3, false, 0, true}));
    EXPECT_TRUE(tree.handle_event(oui::MouseEvent{9, 3, false, 0, false, true}));
    tree.arrange({0, 0, 10, 4});

    oui::CellBuffer buffer{10, 4};
    oui::CellBufferPainter painter{buffer};
    tree.paint(painter, {});
    EXPECT_EQ(buffer.cell_at(8, 3).glyph, U'9');
}

TEST(OuiTreeViewTests, MapsWheelScrollingAndClicksUsingTheScrollbarContentWidth)
{
    oui::TreeView tree;
    auto& root{tree.add("root")};
    root.expanded = true;
    root.add("abcdef");
    root.add("one");
    root.add("two");
    root.add("three");
    std::size_t activated{std::numeric_limits<std::size_t>::max()};
    tree.on_activated([&activated](std::size_t index) { activated = index; });
    tree.arrange({0, 0, 10, 3});

    EXPECT_TRUE(tree.handle_event(oui::MouseEvent{0, 0, false, -1}));
    EXPECT_TRUE(tree.handle_event(oui::MouseEvent{0, 0, true}));
    EXPECT_TRUE(tree.handle_event(oui::MouseEvent{0, 0, false, 0, false, true}));
    EXPECT_EQ(activated, 2u);
}

TEST(OuiCommandTests, BindingKeysToAnUnknownCommandFailsAtOnce)
{
    oui::CommandRegistry commands;
    EXPECT_THROW(commands.bind("not.yet", {U'p', true}), std::logic_error);
    commands.register_command("not.yet", "Now", [] {});
    EXPECT_NO_THROW(commands.bind("not.yet", {U'p', true}));
}

TEST(OuiCommandTests, DispatchesWindowsControlCharactersAsTheirShortcutLetters)
{
    oui::CommandRegistry commands;
    auto executed{false};
    commands.register_command("file.quit", "Quit", [&executed] { executed = true; });
    commands.bind("file.quit", {U'q', true});

    EXPECT_TRUE(commands.dispatch(oui::Event{oui::TextEvent{17, true}}));
    EXPECT_TRUE(executed);
}

TEST(OuiCommandTests, MatchesCtrlShiftKeyToTheExactShiftBindingWhenPresent)
{
    oui::CommandRegistry commands;
    auto save_executed{false};
    auto save_as_executed{false};
    commands.register_command("file.save", "Save", [&save_executed] { save_executed = true; });
    commands.register_command("file.save_as", "Save As", [&save_as_executed] { save_as_executed = true; });
    commands.bind("file.save", {U's', true, false, false});
    commands.bind("file.save_as", {U's', true, false, true});

    EXPECT_TRUE(commands.dispatch(oui::Event{oui::TextEvent{0x13, true, false, true}}));
    EXPECT_TRUE(save_as_executed);
    EXPECT_FALSE(save_executed);
}

TEST(OuiCommandTests, MatchesCtrlShiftKeyToCtrlBindingWhenNoShiftBindingExists)
{
    oui::CommandRegistry commands;
    auto executed{false};
    commands.register_command("edit.paste", "Paste", [&executed] { executed = true; });
    commands.bind("edit.paste", {U'v', true, false, false});

    EXPECT_TRUE(commands.dispatch(oui::Event{oui::TextEvent{0x16, true, false, true}}));
    EXPECT_TRUE(executed);
}

TEST(OuiCommandTests, DoesNotMatchShiftedTextEventToCtrlBindingWithoutShift)
{
    oui::CommandRegistry commands;
    auto executed{false};
    commands.register_command("edit.paste", "Paste", [&executed] { executed = true; });
    commands.bind("edit.paste", {U'v', true, false, false});

    EXPECT_FALSE(commands.dispatch(oui::Event{oui::TextEvent{U'V', false, false, true}}));
    EXPECT_FALSE(executed);
}

TEST(OuiCommandLineTests, SeparatesOptionsAndPositionalArguments)
{
    const oui::CommandLine command_line{{"--no-menu", "example.cpp", "--", "--literal-name"}};

    EXPECT_TRUE(command_line.has_option("no-menu"));
    ASSERT_TRUE(command_line.positional(0));
    EXPECT_EQ(*command_line.positional(0), "example.cpp");
    ASSERT_TRUE(command_line.positional(1));
    EXPECT_EQ(*command_line.positional(1), "--literal-name");
}

TEST(OuiCommandPaletteTests, FormatsModifierShortcuts)
{
    EXPECT_EQ(oui::format_shortcut({U'p', true, false, false}), "Ctrl+P");
    EXPECT_EQ(oui::format_shortcut({U's', true, false, true}), "Ctrl+Shift+S");
}

TEST(OuiCommandPaletteTests, ReadsShortcutsWrittenAsText)
{
    const auto text_of{[](std::string_view text)
                       {
                           const auto shortcut{oui::parse_shortcut(text)};
                           return shortcut ? oui::format_shortcut(*shortcut) : std::string{"-"};
                       }};
    EXPECT_EQ(text_of("Ctrl+Alt+U"), "Ctrl+Alt+U");
    EXPECT_EQ(text_of("shift+f3"), "Shift+F3");
    EXPECT_EQ(text_of("Control+PageDown"), "Ctrl+PgDn");
    EXPECT_EQ(text_of("Ctrl++"), "Ctrl++");
    EXPECT_EQ(text_of("Alt+ñ"), "Alt+ñ");
    EXPECT_EQ(text_of("Hyper+X"), "-");
    EXPECT_EQ(text_of("Ctrl+Word"), "-");
    EXPECT_EQ(text_of(""), "-");
    EXPECT_EQ(oui::parse_shortcut("Ctrl+K")->character, U'k') << "letters bind in lowercase";
}

TEST(OuiCommandPaletteTests, FormatsSpecialKeyShortcutsByName)
{
    const auto key{[](oui::Key value, bool control = false, bool shift = false)
                   { return oui::Shortcut{0, control, false, shift, value}; }};
    EXPECT_EQ(oui::format_shortcut(key(oui::Key::f3)), "F3");
    EXPECT_EQ(oui::format_shortcut(key(oui::Key::f3, false, true)), "Shift+F3");
    EXPECT_EQ(oui::format_shortcut(key(oui::Key::f12)), "F12");
    EXPECT_EQ(oui::format_shortcut(key(oui::Key::page_down, true)), "Ctrl+PgDn");
    EXPECT_EQ(oui::format_shortcut(key(oui::Key::insert_, true)), "Ctrl+Ins");
    EXPECT_EQ(oui::format_shortcut(key(oui::Key::delete_, false, true)), "Shift+Del");
    EXPECT_EQ(oui::format_shortcut(key(oui::Key::f7)), "F7");
}

TEST(OuiCommandPaletteTests, ShortcutNamesCanBeReplaced)
{
    oui::ShortcutNames names;
    names.control = "Strg+";
    names.keys.at(static_cast<std::size_t>(oui::Key::delete_)) = "Entf";
    EXPECT_EQ(oui::format_shortcut({0, true, false, false, oui::Key::delete_}, names), "Strg+Entf");
}

TEST(OuiCommandTests, MnemonicsAreMarkedWithAnAmpersand)
{
    EXPECT_EQ(oui::strip_mnemonic("&Open File"), "Open File");
    EXPECT_EQ(oui::mnemonic("&Open File"), U'o');
    EXPECT_EQ(oui::mnemonic("Save &As"), U'a');
    EXPECT_EQ(oui::mnemonic_offset("Save &As"), std::optional<std::size_t>{5});
    EXPECT_EQ(oui::strip_mnemonic("Fish && Chips"), "Fish & Chips");
    EXPECT_EQ(oui::mnemonic("Fish && Chips"), U'\0');
}

TEST(OuiCommandTests, DisabledCommandsDoNotRunAndLeaveThePalette)
{
    oui::CommandRegistry commands;
    auto runs{0};
    commands.register_command("edit.undo", "Undo", [&runs] { ++ runs; });
    commands.bind("edit.undo", {U'z', true});
    commands.set_enabled("edit.undo", false);
    EXPECT_FALSE(commands.execute("edit.undo"));
    EXPECT_FALSE(commands.dispatch(oui::TextEvent{U'z', true}));
    EXPECT_TRUE(commands.commands().empty());
    commands.set_enabled("edit.undo", true);
    EXPECT_TRUE(commands.dispatch(oui::TextEvent{U'z', true}));
    EXPECT_EQ(runs, 1);

    commands.unregister_command("edit.undo");
    EXPECT_FALSE(commands.dispatch(oui::TextEvent{U'z', true}));
    EXPECT_FALSE(commands.title("edit.undo"));
}

// A list of every key, as a program's help gives it, finds the commands the palette leaves out and every shortcut
// of each.
TEST(OuiCommandTests, ListsEveryCommandWithEveryShortcut)
{
    oui::CommandRegistry commands;
    commands.register_command("edit.redo", "&Redo", [] {});
    commands.bind("edit.redo", {U'z', true, false, true});
    commands.bind("edit.redo", {U'y', true});
    commands.set_enabled("edit.redo", false);
    commands.register_command("menu.file", "File", [] {});
    commands.set_palette_visible("menu.file", false);
    commands.register_command("file.close", "Close", [] {});

    EXPECT_EQ(commands.commands().size(), 1u) << "the palette offers only the enabled and visible";
    const auto all{commands.all_commands()};
    ASSERT_EQ(all.size(), 3u);
    EXPECT_EQ(all[0].id, "edit.redo");
    EXPECT_FALSE(all[0].enabled);
    EXPECT_TRUE(all[0].palette_visible);
    EXPECT_EQ(all[1].id, "menu.file");
    EXPECT_FALSE(all[1].palette_visible);
    EXPECT_EQ(all[2].id, "file.close");

    const auto redo{commands.shortcuts("edit.redo")};
    ASSERT_EQ(redo.size(), 2u);
    EXPECT_EQ(oui::format_shortcut(redo[0]), "Ctrl+Shift+Z");
    EXPECT_EQ(oui::format_shortcut(redo[1]), "Ctrl+Y");
    EXPECT_TRUE(commands.shortcuts("file.close").empty());
    EXPECT_TRUE(commands.shortcuts("nothing").empty());
}

TEST(OuiCommandTests, NamedKeysAndCharactersNeverMatchEachOther)
{
    oui::CommandRegistry commands;
    auto key_runs{0};
    commands.register_command("find_next", "Find Next", [&key_runs] { ++ key_runs; });
    commands.bind("find_next", oui::KeyShortcut{oui::Key::f3});
    // Key::f3 used to share its number with the control character U+0012.
    EXPECT_FALSE(commands.dispatch(oui::TextEvent{static_cast<char32_t>(oui::Key::f3)}));
    EXPECT_TRUE(commands.dispatch(oui::KeyEvent{oui::Key::f3}));
    EXPECT_EQ(key_runs, 1);
}

TEST(OuiFuzzyTests, MatchesCharactersInOrderIgnoringCase)
{
    EXPECT_TRUE(oui::fuzzy_score("OPEN", "Open File"));
    EXPECT_TRUE(oui::fuzzy_score("opf", "Open File"));
    EXPECT_FALSE(oui::fuzzy_score("fo", "Open File"));
    EXPECT_FALSE(oui::fuzzy_score("openx", "Open File"));
    EXPECT_EQ(oui::fuzzy_score("", "anything"), std::optional<int>{0});
}

TEST(OuiFuzzyTests, WildcardsMatchTheWholeTextAsAShellDoes)
{
    EXPECT_TRUE(oui::wildcard_match("*.h", "oui/include/oui/table.h"));
    EXPECT_FALSE(oui::wildcard_match("*.h", "oui/src/table.cpp"));
    EXPECT_TRUE(oui::wildcard_match("*TEST*.cpp", "tests/oui/table_test.cpp"));
    EXPECT_TRUE(oui::wildcard_match("oui/*/table*", "oui/src/table.cpp"));
    EXPECT_TRUE(oui::wildcard_match("tab?e.h", "table.h"));
    EXPECT_FALSE(oui::wildcard_match("tab?e.h", "tabe.h"));
    EXPECT_TRUE(oui::wildcard_match("*", ""));
}

TEST(OuiFuzzyTests, PrefersLettersTogetherWordStartsAndTheFileName)
{
    // Together beats scattered.
    EXPECT_GT(*oui::fuzzy_score("main", "src/main.cpp"), *oui::fuzzy_score("main", "src/m_a_i_n.cpp"));
    // The file name beats the folders on the way to it.
    EXPECT_GT(*oui::fuzzy_score("list", "oui/src/list.cpp"), *oui::fuzzy_score("list", "lists/src/other.cpp"));
    // Word starts: "tv" finds tree_view better than a word that merely contains both letters.
    EXPECT_GT(*oui::fuzzy_score("tv", "tree_view.cpp"), *oui::fuzzy_score("tv", "ntvx.cpp"));
    EXPECT_GT(*oui::fuzzy_score("tv", "TreeView.h"), *oui::fuzzy_score("tv", "treeview.h"));
}

TEST(OuiCommandPaletteTests, ExcludesInternalCommandsAndPreservesCheckedState)
{
    oui::CommandRegistry commands;
    commands.register_command("view.word_wrap", "Toggle Word Wrap", [] {});
    commands.set_checked("view.word_wrap", true);
    commands.register_command("menu.view", "View menu", [] {});
    commands.set_palette_visible("menu.view", false);

    const auto palette_commands{commands.commands()};
    ASSERT_EQ(palette_commands.size(), 1u);
    EXPECT_EQ(palette_commands.front().id, "view.word_wrap");
    ASSERT_TRUE(palette_commands.front().checked.has_value());
    EXPECT_TRUE(*palette_commands.front().checked);
}

TEST(OuiLayoutTests, ColumnKeepsAFollowingScrollableChildInsideItsRemainingHeight)
{
    auto content{oui::column()};
    content->add(oui::line_edit());
    auto& results{content->add(oui::tree_view())};
    for(int index{}; index < 20; ++ index)
    {
        results.add("Command " + std::to_string(index));
    }
    content->arrange({0, 0, 30, 5});

    EXPECT_EQ(results.bounds().y, 1);
    EXPECT_EQ(results.bounds().height, 4);
}

namespace
{

// Shows a resizable dialog with a field holding the focus, plays `events` at it, and reports where the dialog
// was when it closed, what it saved as its placement, and what the field got.
struct Placed
{
    oui::Rect start;
    oui::Rect end;
    oui::WindowPlacement saved;
    std::string typed;
};

Placed place_dialog(std::vector<oui::Event> events, bool resizable = true)
{
    auto backend{oui::scripted_backend({}, {60, 20})};
    auto& script{*backend};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));
    Placed placed;
    application.set_window_placement_store([](std::string_view) { return std::nullopt; },
                                           [&placed](std::string_view, const oui::WindowPlacement& placement)
                                           { placed.saved = placement; });
    auto dialog{oui::window("Placed")};
    auto& dialog_reference{*dialog};
    dialog->set_preferred_size({20, 6});
    dialog->set_resizable(resizable);
    dialog->set_placement_key("placed");
    auto& field{dialog->add(oui::line_edit())};
    application.post(
        [&application, &field, &script, &dialog_reference, &placed, events]
        {
            application.focus(field);
            application.render();
            placed.start = dialog_reference.bounds();
            for(const auto& event : events)
            {
                script.push(event);
            }
        });
    script.set_on_drained(
        [&dialog_reference, &placed, &field, &application]
        {
            application.render();
            placed.end = dialog_reference.bounds();
            placed.typed = std::string{field.text()};
            dialog_reference.close(oui::DialogResult::cancelled);
        });
    static_cast<void>(application.show_modal(std::move(dialog)));
    return placed;
}

const oui::Event control_l{oui::TextEvent{U'l', true}};
const oui::Event right{oui::KeyEvent{oui::Key::right}};
const oui::Event down{oui::KeyEvent{oui::Key::down}};
const oui::Event control_right{oui::KeyEvent{oui::Key::right, true}};
const oui::Event control_down{oui::KeyEvent{oui::Key::down, true}};
const oui::Event enter{oui::KeyEvent{oui::Key::enter}};
const oui::Event escape{oui::KeyEvent{oui::Key::escape}};

} // namespace

TEST(OuiApplicationTests, CtrlLMovesAndResizesADialogFromTheKeyboard)
{
    const auto placed{place_dialog({control_l, right, right, down, control_right, control_down, control_down,
                                    oui::TextEvent{U'x'}, enter, oui::TextEvent{U'y'}})};
    EXPECT_EQ(placed.end.x, placed.start.x + 2);
    EXPECT_EQ(placed.end.y, placed.start.y + 1);
    EXPECT_EQ(placed.end.width, placed.start.width + 1);
    EXPECT_EQ(placed.end.height, placed.start.height + 2);
    EXPECT_EQ(placed.typed, "y") << "keys go to the placing until Enter, then to the dialog again";
    ASSERT_TRUE(placed.saved.position.has_value()) << "remembered as a drag with the mouse is";
    EXPECT_EQ(placed.saved.position->x, placed.end.x);
    ASSERT_TRUE(placed.saved.size.has_value());
    EXPECT_EQ(placed.saved.size->width, placed.end.width);
}

TEST(OuiApplicationTests, EscapePutsAPlacedDialogBackAndKeepsItOpen)
{
    const auto placed{place_dialog({control_l, right, down, control_right, escape, oui::TextEvent{U'z'}})};
    EXPECT_EQ(placed.end.x, placed.start.x);
    EXPECT_EQ(placed.end.y, placed.start.y);
    EXPECT_EQ(placed.end.width, placed.start.width);
    EXPECT_FALSE(placed.saved.position.has_value());
    EXPECT_EQ(placed.typed, "z") << "the Escape that ends the placing does not close the dialog";
}

TEST(OuiApplicationTests, ADialogThatCannotBeResizedOnlyMoves)
{
    const auto placed{place_dialog({control_l, control_right, control_down, right, enter}, false)};
    EXPECT_EQ(placed.end.width, placed.start.width);
    EXPECT_EQ(placed.end.height, placed.start.height);
    EXPECT_EQ(placed.end.x, placed.start.x + 1);
}

TEST(OuiApplicationTests, AKeyModeHintTooWideTakesMoreLines)
{
    auto backend{oui::scripted_backend({}, {24, 6})};
    auto& script{*backend};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));
    application.start_key_mode("Split: arrows · Move tab: Shift · Esc",
                               [](const oui::Event&) { return oui::KeyModeAnswer::keep; });
    application.render();
    const auto& frame{*script.last_frame()};
    const auto row{[&frame](int y)
                   {
                       std::string text;
                       for(int x{}; x < frame.width(); ++ x)
                       {
                           text += oui::encode_utf8(frame.cell_at(x, y).glyph);
                       }
                       return text.substr(0, text.find_last_not_of(' ') + 1);
                   }};
    EXPECT_EQ(row(4), " Split: arrows");
    EXPECT_EQ(row(5), " Move tab: Shift · Esc");
}

TEST(OuiApplicationTests, AKeyModeCanEndAndLetItsKeyThrough)
{
    oui::Application application{oui::scripted_backend()};
    auto field_owner{oui::line_edit()};
    auto& field{*field_owner};
    application.set_content(std::move(field_owner));
    application.focus(field);
    application.start_key_mode("Keys",
                               [](const oui::Event& event)
                               {
                                   const auto* const text{std::get_if<oui::TextEvent>(&event)};
                                   return text != nullptr && text->codepoint == U'a' ? oui::KeyModeAnswer::keep
                                                                                     : oui::KeyModeAnswer::end_and_pass;
                               });
    application.dispatch_event(oui::TextEvent{U'a'});
    EXPECT_TRUE(application.in_key_mode());
    application.dispatch_event(oui::TextEvent{U'b'});
    EXPECT_FALSE(application.in_key_mode());
    EXPECT_EQ(field.text(), "b") << "the key that ended the mode still did its work";
}

TEST(OuiApplicationTests, DownMovesFocusFromAnUnconsumingLineEditToTheNextWidget)
{
    // The command palette's pattern: a filter field above a list, inside a dialog.
    auto backend{oui::scripted_backend(std::vector<oui::Event>{oui::KeyEvent{oui::Key::down}})};
    auto& script{*backend};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));
    auto dialog{oui::window("Palette")};
    auto& dialog_reference{*dialog};
    auto content{oui::column()};
    auto& filter{oui::line_edit(*content)};
    auto& results{content->add(oui::tree_view())};
    dialog->add(std::move(content));
    oui::Widget* focused_after{};
    script.set_on_drained(
        [&application, &dialog_reference, &focused_after]
        {
            focused_after = application.focused();
            dialog_reference.close(oui::DialogResult::cancelled);
        });
    application.post([&application, &filter] { application.focus(filter); });
    static_cast<void>(application.show_modal(std::move(dialog)));
    EXPECT_EQ(focused_after, &results);
}

// Runs a palette-like dialog, a filter field above a two-command list, through `events` with the focus on
// the filter, and reports where the focus ends and what the filter holds.
std::pair<oui::Widget*, std::string> run_filter_and_list(std::vector<oui::Event> events, oui::Widget** filter_out,
                                                         oui::Widget** list_out)
{
    auto backend{oui::scripted_backend(std::move(events))};
    auto& script{*backend};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));
    auto dialog{oui::window("Palette")};
    auto& dialog_reference{*dialog};
    auto content{oui::column()};
    auto& filter{oui::line_edit(*content)};
    auto& results{content->add(oui::tree_view())};
    results.add("First");
    results.add("Second");
    results.delegate_typing_to(filter);
    dialog->add(std::move(content));
    *filter_out = &filter;
    *list_out = &results;
    std::pair<oui::Widget*, std::string> outcome;
    script.set_on_drained(
        [&application, &dialog_reference, &filter, &outcome]
        {
            outcome = {application.focused(), std::string{filter.text()}};
            dialog_reference.close(oui::DialogResult::cancelled);
        });
    application.post([&application, &filter] { application.focus(filter); });
    static_cast<void>(application.show_modal(std::move(dialog)));
    return outcome;
}

TEST(OuiApplicationTests, UpFromTheTopOfAListStaysInTheList)
{
    oui::Widget* filter{};
    oui::Widget* list{};
    // Down from the filter into the list, then Up at its top, which stays there, as in Turbo Vision.
    const auto [focused, text]{run_filter_and_list(
        {oui::KeyEvent{oui::Key::down}, oui::KeyEvent{oui::Key::up}, oui::KeyEvent{oui::Key::up}}, &filter, &list)};
    EXPECT_EQ(focused, list);
}

TEST(OuiApplicationTests, TypingInAListGoesToTheFieldItDelegatesTo)
{
    oui::Widget* filter{};
    oui::Widget* list{};
    // The second Down moves within the list; the typing then goes to the filter, focus and all.
    const auto [focused, text]{run_filter_and_list(
        {oui::KeyEvent{oui::Key::down}, oui::KeyEvent{oui::Key::down}, oui::TextEvent{U'q'}, oui::TextEvent{U'u'}},
        &filter, &list)};
    EXPECT_EQ(focused, filter);
    EXPECT_EQ(text, "qu");
}

TEST(OuiApplicationTests, DownStaysWithTheFocusedWidgetOutsideDialogs)
{
    oui::Application application{std::make_unique<MemoryBackend>()};
    auto content{oui::column()};
    auto& filter{oui::line_edit(*content)};
    content->add(oui::tree_view());
    application.set_content(std::move(content));
    ASSERT_EQ(application.focused(), &filter);
    application.dispatch_event(oui::KeyEvent{oui::Key::down});
    EXPECT_EQ(application.focused(), &filter);
}

TEST(OuiMenuBarTests, OpensAMenuFromAMousePress)
{
    oui::CommandRegistry commands;
    commands.register_command("file.quit", "Quit", [] {});
    oui::MenuBar menu_bar{commands};
    menu_bar.add_menu("File", {"file.quit"});
    menu_bar.arrange({0, 0, 30, 1});

    EXPECT_TRUE(menu_bar.handle_event(oui::Event{oui::MouseEvent{1, 0, true}}));
    EXPECT_EQ(menu_bar.measure({30, 10}).height, 1);
}

TEST(OuiMenuBarTests, RoutesKeysToAnOpenMenuWithoutChangingEditorFocus)
{
    auto backend{std::make_unique<MemoryBackend>()};
    oui::Application application{std::move(backend)};
    auto root{std::make_unique<OverlayTestContainer>()};
    auto menu_bar{oui::menu_bar(application.commands())};
    auto* const menu_bar_reference{menu_bar.get()};
    menu_bar->add_menu("File", {"file.first", "file.second"});
    auto editor{oui::text_edit()};
    auto* const editor_reference{editor.get()};
    root->add(std::move(editor));
    root->add(std::move(menu_bar));

    auto calls{0};
    application.commands().register_command("file.first", "First", [&calls] { calls += 1; });
    application.commands().register_command("file.second", "Second", [&calls] { calls += 10; });
    application.commands().register_command("menu.file", "File menu",
                                            [menu_bar_reference] { menu_bar_reference->open_menu(0); });
    application.commands().bind("menu.file", {U'f', false, true});
    application.set_content(std::move(root));
    application.render();
    application.focus(*editor_reference);

    application.dispatch_event(oui::TextEvent{U'f', false, true});
    application.dispatch_event(oui::KeyEvent{oui::Key::down});
    application.dispatch_event(oui::KeyEvent{oui::Key::enter});

    EXPECT_EQ(calls, 10);
    EXPECT_EQ(application.focused(), editor_reference);
}

namespace
{

// An application whose content is a text editor under a menu bar, as most programs look.
struct MenuFixture
{
    MenuFixture()
    {
        auto root{std::make_unique<OverlayTestContainer>()};
        auto bar{oui::menu_bar(application.commands())};
        menu_bar = bar.get();
        auto text{oui::text_edit()};
        editor = text.get();
        root->add(std::move(text));
        root->add(std::move(bar));
        application.set_content(std::move(root));
    }

    void start()
    {
        application.render();
        application.focus(*editor);
    }

    void key(oui::Key value) { application.dispatch_event(oui::KeyEvent{value}); }

    oui::Application application{std::make_unique<MemoryBackend>()};
    oui::MenuBar* menu_bar{};
    oui::TextEdit* editor{};
};

} // namespace

TEST(OuiMenuBarTests, AltAndTheMnemonicOpenAMenuAndLettersPickItsEntries)
{
    MenuFixture fixture;
    std::vector<std::string> ran;
    for(const auto& [id, title] : {std::pair{"file.new", "&New"}, std::pair{"file.save", "&Save"}})
    {
        fixture.application.commands().register_command(id, title, [&ran, id = id] { ran.emplace_back(id); });
    }
    fixture.menu_bar->add_menu("&File", {"file.new", "file.save"});
    fixture.start();

    fixture.application.dispatch_event(oui::TextEvent{U'f', false, true});
    EXPECT_TRUE(fixture.menu_bar->is_open());
    fixture.application.dispatch_event(oui::TextEvent{U's'});
    EXPECT_EQ(ran, (std::vector<std::string>{"file.save"}));
    EXPECT_FALSE(fixture.menu_bar->is_open());
    EXPECT_EQ(fixture.editor->text(), "") << "the letter did not reach the editor";
}

TEST(OuiMenuBarTests, KeyboardSkipsSeparatorsAndDisabledEntriesDoNotRun)
{
    MenuFixture fixture;
    std::vector<std::string> ran;
    for(const auto* const id : {"edit.undo", "edit.copy", "edit.paste"})
    {
        fixture.application.commands().register_command(id, id, [&ran, id] { ran.emplace_back(id); });
    }
    fixture.application.commands().set_enabled("edit.copy", false);
    fixture.menu_bar->add_menu("Edit", {"edit.undo", oui::menu_separator(), "edit.copy", "edit.paste"});
    fixture.start();
    fixture.menu_bar->open_menu(0);

    fixture.key(oui::Key::down);
    fixture.key(oui::Key::enter);
    EXPECT_TRUE(ran.empty()) << "down lands on the disabled Copy, past the separator";
    EXPECT_TRUE(fixture.menu_bar->is_open());
    fixture.key(oui::Key::down);
    fixture.key(oui::Key::enter);
    EXPECT_EQ(ran, (std::vector<std::string>{"edit.paste"}));
}

TEST(OuiMenuBarTests, SubmenusAreBuiltEachTimeTheyOpen)
{
    MenuFixture fixture;
    std::vector<std::string> recent{"one.txt"};
    std::string opened;
    fixture.menu_bar->add_menu(
        "File",
        {oui::menu_submenu("Recent", oui::MenuItem::Items{[&recent, &opened]
                                                          {
                                                              std::vector<oui::MenuItem> items;
                                                              for(const auto& name : recent)
                                                              {
                                                                  items.push_back(oui::menu_action(name, [&opened, name]
                                                                                                   { opened = name; }));
                                                              }
                                                              return items;
                                                          }})});
    fixture.start();
    recent.insert(recent.begin(), "two.txt");
    fixture.menu_bar->open_menu(0);
    fixture.key(oui::Key::right);
    fixture.key(oui::Key::enter);
    EXPECT_EQ(opened, "two.txt");
}

TEST(OuiMenuBarTests, RemovesItsShortcutsWhenDestroyed)
{
    oui::CommandRegistry commands;
    {
        oui::MenuBar bar{commands};
        bar.add_menu("&File", {});
        EXPECT_TRUE(commands.dispatch(oui::TextEvent{U'f', false, true}));
    }
    EXPECT_FALSE(commands.dispatch(oui::TextEvent{U'f', false, true}));
}

TEST(OuiMenuBarTests, ClosesAnOpenMenuWhenClickingOutsideItsOverlay)
{
    auto backend{std::make_unique<MemoryBackend>()};
    oui::Application application{std::move(backend)};
    application.commands().register_command("file.quit", "Quit", [] {});
    auto menu_bar{oui::menu_bar(application.commands())};
    auto* const menu_bar_reference{menu_bar.get()};
    menu_bar->add_menu("File", {"file.quit"});
    application.set_content(std::move(menu_bar));
    application.render();
    menu_bar_reference->open_menu(0);

    application.dispatch_event(oui::MouseEvent{10, 3, true});

    EXPECT_EQ(menu_bar_reference->measure({20, 4}).height, 1);
}

TEST(OuiMenuTests, ExecutesRegisteredCommandsFromMenusAndShortcuts)
{
    auto backend{std::make_unique<MemoryBackend>()};
    oui::Application application{std::move(backend)};
    auto calls{0};
    application.commands().register_command("file.save", "Save", [&calls] { ++ calls; });
    application.commands().bind("file.save", {U's', true});

    auto menu{oui::menu(application.commands())};
    menu->add_item("file.save");
    EXPECT_TRUE(menu->handle_event(oui::KeyEvent{oui::Key::enter}));
    application.dispatch_event(oui::TextEvent{U's', true});

    EXPECT_EQ(calls, 2);
}

TEST(OuiMenuTests, DisplaysCheckedCommandsWithAStateMarker)
{
    oui::CommandRegistry commands;
    commands.register_command("view.word_wrap", "Word Wrap", [] {});
    commands.set_checked("view.word_wrap", true);
    auto menu{oui::menu(commands)};
    menu->add_item("view.word_wrap");
    menu->arrange({0, 0, 20, 1});

    oui::CellBuffer checked_buffer{20, 1};
    oui::CellBufferPainter checked_painter{checked_buffer};
    menu->paint(checked_painter, {});
    EXPECT_EQ(checked_buffer.cell_at(0, 0).glyph, U'[');
    EXPECT_EQ(checked_buffer.cell_at(1, 0).glyph, U'\u25a0');
    EXPECT_EQ(checked_buffer.cell_at(4, 0).glyph, U'W');

    commands.set_checked("view.word_wrap", false);
    oui::CellBuffer unchecked_buffer{20, 1};
    oui::CellBufferPainter unchecked_painter{unchecked_buffer};
    menu->paint(unchecked_painter, {});
    EXPECT_EQ(unchecked_buffer.cell_at(1, 0).glyph, U' ');
}

TEST(OuiScriptedBackendTests, PlaysItsScriptThenReportsItRanOut)
{
    oui::ScriptedBackend backend{{oui::TextEvent{U'a'}, oui::TextEvent{U'b'}}};
    auto drained{0};
    backend.set_on_drained([&drained] { ++ drained; });
    EXPECT_TRUE(backend.poll_event());
    EXPECT_TRUE(backend.poll_event());
    EXPECT_FALSE(backend.poll_event());
    EXPECT_FALSE(backend.poll_event());
    EXPECT_EQ(drained, 1);
}

TEST(OuiScriptedBackendTests, CanWaitForAFrameBeforeEachEvent)
{
    oui::ScriptedBackend backend{{oui::TextEvent{U'a'}, oui::TextEvent{U'b'}}};
    backend.set_one_event_per_frame(true);
    backend.set_keep_frames(true);
    EXPECT_FALSE(backend.poll_event()) << "nothing is drawn yet";
    backend.present(oui::CellBuffer{2, 1});
    EXPECT_TRUE(backend.poll_event());
    EXPECT_FALSE(backend.poll_event());
    backend.present(oui::CellBuffer{2, 1});
    EXPECT_TRUE(backend.poll_event());
    EXPECT_EQ(backend.frames().size(), 2u);
    EXPECT_TRUE(backend.last_frame());
}

TEST(OuiWidgetTests, ThemeColoursAreReadWhenPainting)
{
    oui::Label label{"warning"};
    label.set_color(&oui::Theme::error_foreground);
    label.arrange({0, 0, 10, 1});
    oui::CellBuffer buffer{10, 1};
    oui::CellBufferPainter painter{buffer};
    oui::Theme theme;
    theme.error_foreground = {200, 10, 10};
    label.paint(painter, theme);
    EXPECT_EQ(buffer.cell_at(0, 0).foreground, theme.error_foreground);
    theme.error_foreground = {10, 200, 10};
    label.paint(painter, theme);
    EXPECT_EQ(buffer.cell_at(0, 0).foreground, theme.error_foreground) << "a new theme recolours the label";
}

TEST(OuiDockLayoutTests, EdgesTakeStripsInOrderAndFillTakesTheRest)
{
    auto layout{oui::dock_layout()};
    auto& top{layout->add(oui::Dock::top, std::make_unique<FixedWidget>(oui::Size{5, 1}))};
    auto& bottom{layout->add(oui::Dock::bottom, std::make_unique<FixedWidget>(oui::Size{5, 1}))};
    auto& side{layout->add(oui::Dock::left, std::make_unique<FixedWidget>(oui::Size{1, 1}), 10)};
    auto& center{layout->add(oui::Dock::fill, std::make_unique<FixedWidget>(oui::Size{1, 1}))};
    layout->arrange({0, 0, 40, 20});
    EXPECT_EQ(top.bounds().y, 0);
    EXPECT_EQ(top.bounds().width, 40);
    EXPECT_EQ(bottom.bounds().y, 19);
    EXPECT_EQ(side.bounds().y, 1);
    EXPECT_EQ(side.bounds().height, 18);
    EXPECT_EQ(side.bounds().width, 10);
    EXPECT_EQ(center.bounds().x, 10);
    EXPECT_EQ(center.bounds().width, 30);
    EXPECT_EQ(center.bounds().height, 18);

    side.set_visible(false);
    layout->arrange({0, 0, 40, 20});
    EXPECT_EQ(center.bounds().x, 0) << "a hidden child takes no space";
}

TEST(OuiDockLayoutTests, TheMouseDragsTheLineOfAResizableStrip)
{
    auto layout{oui::dock_layout()};
    auto& left{layout->add(oui::Dock::left, std::make_unique<FixedWidget>(oui::Size{1, 1}), 10)};
    auto& right{layout->add(oui::Dock::right, std::make_unique<FixedWidget>(oui::Size{1, 1}), 10)};
    auto& center{layout->add(oui::Dock::fill, std::make_unique<FixedWidget>(oui::Size{1, 1}))};
    layout->set_resizable(left);
    layout->set_resizable(right);
    std::vector<int> resized;
    layout->on_resized([&resized](const oui::Widget&, int size) { resized.push_back(size); });
    layout->arrange({0, 0, 60, 20});
    // The line takes the strip's inner column.
    EXPECT_EQ(left.bounds().width, 9);
    EXPECT_EQ(right.bounds().x, 51);
    EXPECT_EQ(center.bounds().x, 10);
    EXPECT_EQ(layout->hit_test(9, 5), layout.get());
    EXPECT_EQ(layout->hit_test(50, 5), layout.get());

    oui::MouseEvent press{9, 5, true};
    ASSERT_TRUE(layout->handle_event(press));
    oui::MouseEvent drag{19, 5, false};
    drag.dragging = true;
    ASSERT_TRUE(layout->handle_event(drag));
    oui::MouseEvent release{19, 5, false};
    release.released = true;
    ASSERT_TRUE(layout->handle_event(release));
    EXPECT_EQ(left.bounds().width, 19);
    EXPECT_EQ(center.bounds().x, 20);
    EXPECT_EQ(resized, std::vector<int>{20});

    // The right strip grows to the left, and never past what leaves the rest its minimum.
    ASSERT_TRUE(layout->handle_event(oui::MouseEvent{50, 5, true}));
    oui::MouseEvent far{0, 5, false};
    far.released = true;
    ASSERT_TRUE(layout->handle_event(far));
    EXPECT_GE(center.bounds().width, 0);
    EXPECT_EQ(layout->size_of(right), 32);
}

TEST(OuiStatusBarTests, ShowsLeftAndRightTextOrWhatItsProviderSays)
{
    oui::StatusBar bar;
    bar.set_left("left");
    bar.set_right("right");
    bar.arrange({0, 0, 20, 1});
    oui::CellBuffer buffer{20, 1};
    oui::CellBufferPainter painter{buffer};
    bar.paint(painter, {});
    EXPECT_EQ(buffer.cell_at(0, 0).glyph, U'l');
    EXPECT_EQ(buffer.cell_at(15, 0).glyph, U'r');

    auto counter{0};
    bar.set_provider([&counter]() -> oui::StatusText { return {std::to_string(++ counter), ""}; });
    bar.paint(painter, {});
    bar.paint(painter, {});
    EXPECT_EQ(buffer.cell_at(0, 0).glyph, U'2');
}

TEST(OuiTabViewTests, ShowsOnlyTheSelectedTabAndCyclesWithArrowKeys)
{
    auto tabs{oui::tab_view()};
    tabs->add_tab("One", std::make_unique<FixedWidget>(oui::Size{4, 2}));
    tabs->add_tab("Two", std::make_unique<FixedWidget>(oui::Size{7, 3}));

    const auto first_size{tabs->measure({80, 25})};
    EXPECT_EQ(first_size.width, 4);
    EXPECT_EQ(first_size.height, 3);
    EXPECT_TRUE(tabs->handle_event(oui::KeyEvent{oui::Key::right}));
    EXPECT_EQ(tabs->selected_index(), 1u);
    const auto second_size{tabs->measure({80, 25})};
    EXPECT_EQ(second_size.width, 7);
    EXPECT_EQ(second_size.height, 4);
}

TEST(OuiTabViewTests, SwitchesTabsWithArrowKeys)
{
    auto tabs{oui::tab_view()};
    tabs->add_tab("One", std::make_unique<FixedWidget>(oui::Size{1, 1}));
    tabs->add_tab("Two", std::make_unique<FixedWidget>(oui::Size{1, 1}));

    EXPECT_TRUE(tabs->handle_event(oui::Event{oui::KeyEvent{oui::Key::right}}));
    EXPECT_EQ(tabs->selected_index(), 1u);
    EXPECT_TRUE(tabs->handle_event(oui::Event{oui::KeyEvent{oui::Key::left}}));
    EXPECT_EQ(tabs->selected_index(), 0u);
}

TEST(OuiTabViewTests, ClickingATitleActivatesThatTab)
{
    auto tabs{oui::tab_view()};
    tabs->add_tab("One", std::make_unique<FixedWidget>(oui::Size{1, 1}));
    tabs->add_tab("Two", std::make_unique<FixedWidget>(oui::Size{1, 1}));
    tabs->arrange({0, 0, 40, 5});
    std::vector<std::size_t> activated;
    const auto connection{tabs->on_tab_activated([&activated](std::size_t index) { activated.push_back(index); })};

    // " One × " spans columns 0-6 and " Two × " starts at column 7.
    EXPECT_TRUE(tabs->handle_event(oui::MouseEvent{8, 0, true}));
    EXPECT_EQ(tabs->selected_index(), 1u);
    ASSERT_EQ(activated.size(), 1u);
    EXPECT_EQ(activated.front(), 1u);

    // Keyboard navigation and the close button do not activate.
    EXPECT_TRUE(tabs->handle_event(oui::KeyEvent{oui::Key::left}));
    EXPECT_TRUE(tabs->handle_event(oui::MouseEvent{5, 0, true}));
    EXPECT_EQ(activated.size(), 1u);
}

TEST(OuiTabViewTests, AnnouncesEveryChangeOfTheShownTab)
{
    auto tabs{oui::tab_view()};
    tabs->add_tab("One", std::make_unique<FixedWidget>(oui::Size{1, 1}));
    tabs->add_tab("Two", std::make_unique<FixedWidget>(oui::Size{1, 1}));
    tabs->add_tab("Three", std::make_unique<FixedWidget>(oui::Size{1, 1}));
    std::vector<std::size_t> shown;
    const auto connection{tabs->on_selection_changed([&shown](std::size_t index) { shown.push_back(index); })};

    tabs->select(1);
    tabs->select(1);
    tabs->handle_event(oui::KeyEvent{oui::Key::right});
    // Closing the shown tab shows another one, even when the index stays the same.
    tabs->close_tab(2);
    EXPECT_EQ(shown, (std::vector<std::size_t>{1, 2, 1}));
}

TEST(OuiWidgetTests, TextEditCanBeMarkedModifiedWithoutAnEdit)
{
    oui::TextEdit editor;
    editor.set_text("text");
    EXPECT_FALSE(editor.is_modified());
    editor.mark_modified();
    EXPECT_TRUE(editor.is_modified());
    editor.mark_saved();
    EXPECT_FALSE(editor.is_modified());
}

TEST(OuiTabViewTests, RightClickClosesOnlyWhenTheApplicationWantsIt)
{
    auto tabs{oui::tab_view()};
    tabs->add_tab("One", std::make_unique<FixedWidget>(oui::Size{1, 1}));
    tabs->arrange({0, 0, 40, 5});
    std::vector<std::size_t> asked;
    const auto connection{tabs->on_close_requested([&asked](std::size_t index) { asked.push_back(index); })};
    oui::MouseEvent right_click{2, 0, true};
    right_click.right = true;
    tabs->handle_event(right_click);
    EXPECT_TRUE(asked.empty());
    tabs->set_close_on_right_click(true);
    tabs->handle_event(right_click);
    EXPECT_EQ(asked, (std::vector<std::size_t>{0}));
}

TEST(OuiTabViewTests, TitlesAreMeasuredInColumnsNotBytes)
{
    auto tabs{oui::tab_view()};
    tabs->add_tab("Año", std::make_unique<FixedWidget>(oui::Size{1, 1}));
    tabs->add_tab("Two", std::make_unique<FixedWidget>(oui::Size{1, 1}));
    tabs->arrange({0, 0, 40, 5});
    // " Año × " takes 7 columns, so column 7 is already the second tab.
    EXPECT_TRUE(tabs->handle_event(oui::MouseEvent{7, 0, true}));
    EXPECT_EQ(tabs->selected_index(), 1u);
}

TEST(OuiTabViewTests, ClosesTheSelectedTabAndKeepsAValidSelection)
{
    auto tabs{oui::tab_view()};
    tabs->add_tab("One", std::make_unique<FixedWidget>(oui::Size{1, 1}));
    tabs->add_tab("Two", std::make_unique<FixedWidget>(oui::Size{1, 1}));
    tabs->select(1);

    tabs->close_tab(1);

    EXPECT_EQ(tabs->child_count(), 1u);
    EXPECT_EQ(tabs->selected_index(), 0u);
    EXPECT_EQ(tabs->title(0), "One");
    EXPECT_NE(tabs->content_at<FixedWidget>(0), nullptr);
}

TEST(OuiTabViewTests, SelectsTabsWithAMousePress)
{
    auto tabs{oui::tab_view()};
    tabs->add_tab("One", std::make_unique<FixedWidget>(oui::Size{1, 1}));
    tabs->add_tab("Two", std::make_unique<FixedWidget>(oui::Size{1, 1}));
    tabs->arrange({0, 0, 20, 3});

    EXPECT_TRUE(tabs->handle_event(oui::MouseEvent{9, 0, true}));
    EXPECT_EQ(tabs->selected_index(), 1u);
}

TEST(OuiTabViewTests, MarksModifiedTabTitles)
{
    auto tabs{oui::tab_view()};
    tabs->add_tab("One", std::make_unique<FixedWidget>(oui::Size{1, 1}));
    tabs->set_highlight_predicate([](std::size_t) { return true; });
    tabs->arrange({0, 0, 20, 3});
    oui::CellBuffer buffer{20, 3};
    oui::CellBufferPainter painter{buffer};
    tabs->paint(painter, {});

    EXPECT_EQ(buffer.cell_at(5, 0).glyph, U'\u00d7');
    EXPECT_EQ(buffer.cell_at(5, 0).foreground, oui::Theme{}.warning_foreground);
}

TEST(OuiApplicationTests, RestoresThePreviousContentAfterAModalDialogCloses)
{
    auto backend{std::make_unique<MemoryBackend>()};
    oui::Application application{std::move(backend)};
    auto original{oui::button("Original")};
    auto* const original_reference{original.get()};
    application.set_content(std::move(original));

    auto dialog{oui::window("Modal")};
    auto* const dialog_reference{dialog.get()};
    application.post([dialog_reference] { dialog_reference->close(oui::DialogResult::accepted); });

    EXPECT_EQ(application.show_modal(std::move(dialog)), oui::DialogResult::accepted);
    EXPECT_TRUE(original_reference->is_focused());
}

TEST(OuiApplicationTests, AKeptModalDialogComesBackWholeToBeShownAgain)
{
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_reference{backend.get()};
    oui::Application application{std::move(backend)};
    application.set_content(oui::button("Original"));

    auto dialog{oui::window("Kept")};
    auto& field{dialog->add(oui::line_edit())};
    auto* const dialog_reference{dialog.get()};
    application.post(
        [&application, &field, dialog_reference]
        {
            application.focus(field);
            dialog_reference->close(oui::DialogResult::accepted);
        });

    EXPECT_EQ(application.show_modal_and_keep(dialog), oui::DialogResult::accepted);
    ASSERT_EQ(dialog.get(), dialog_reference);
    EXPECT_EQ(&dialog->child_at(0), &field);
    EXPECT_FALSE(field.is_focused());

    backend_reference->push_event(oui::KeyEvent{oui::Key::escape});
    EXPECT_EQ(application.show_modal_and_keep(dialog), oui::DialogResult::cancelled);
    application.post([dialog_reference] { dialog_reference->close(oui::DialogResult::accepted); });
    EXPECT_EQ(application.show_modal_and_keep(dialog), oui::DialogResult::accepted);
    EXPECT_EQ(dialog.get(), dialog_reference);
}

TEST(OuiApplicationTests, DoesNotBubbleEventsIntoADestroyedModalLayer)
{
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_reference{backend.get()};
    oui::Application application{std::move(backend)};
    auto background{oui::window("Background")};
    auto content{oui::column()};
    auto button{oui::button("Focused")};
    auto* const button_reference{button.get()};
    content->add(std::move(button));
    background->add(std::move(content));
    application.set_content(std::move(background));
    application.focus(*button_reference);

    auto dialog{oui::window("Modal")};
    backend_reference->push_event(oui::KeyEvent{oui::Key::escape});
    static_cast<void>(application.show_modal(std::move(dialog)));

    application.dispatch_event(oui::TextEvent{U'x'});
    EXPECT_EQ(application.focused(), button_reference);
}

namespace
{

// Closes its window as soon as it is pressed, the way a double-click on a file closes the file picker.
class CloseOnPress final : public oui::Widget
{
  public:
    explicit CloseOnPress(oui::Window& window) : m_window{window} {}

    [[nodiscard]] oui::Size measure(const oui::Constraints& constraints) const override
    {
        return constraints.constrain({constraints.max_width, constraints.max_height});
    }

    bool handle_event(const oui::Event& event) override
    {
        const auto* const mouse{std::get_if<oui::MouseEvent>(&event)};
        if(mouse == nullptr || !mouse->pressed)
        {
            return false;
        }
        m_window.close(oui::DialogResult::accepted);
        return true;
    }

  private:
    oui::Window& m_window;
};

// Room enough for a bordered dialog to give its content a real area. Like a person at the mouse, it only
// clicks once something has been drawn, so the widgets have their positions.
class RoomyBackend final : public oui::Backend
{
  public:
    [[nodiscard]] oui::Size size() const override { return {40, 12}; }

    std::optional<oui::Event> poll_event() override
    {
        if(m_events.empty() || !m_presented)
        {
            return std::nullopt;
        }
        const auto event{m_events.front()};
        m_events.erase(m_events.begin());
        return event;
    }

    void present(const oui::CellBuffer&) override { m_presented = true; }

    void push_event(const oui::Event& event) { m_events.push_back(event); }

  private:
    std::vector<oui::Event> m_events;
    bool m_presented{false};
};

} // namespace

TEST(OuiApplicationTests, TheRestOfAClickThatClosedAModalDoesNotReachTheWidgetBelow)
{
    auto backend{std::make_unique<RoomyBackend>()};
    auto* const backend_reference{backend.get()};
    oui::Application application{std::move(backend)};
    auto editor_pointer{oui::text_edit()};
    auto& editor{*editor_pointer};
    editor.set_text("abcdef\nghijkl\nmnopqr");
    application.set_content(std::move(editor_pointer));
    application.focus(editor);

    auto dialog{oui::window("Modal")};
    dialog->add(std::make_unique<CloseOnPress>(*dialog));
    backend_reference->push_event(oui::MouseEvent{20, 6, true});
    static_cast<void>(application.show_modal(std::move(dialog)));

    // The same click goes on: a small drag while the button is held, then the release.
    application.dispatch_event(oui::MouseEvent{22, 1, false, 0, true});
    application.dispatch_event(oui::MouseEvent{22, 1, false, 0, false, true});
    EXPECT_EQ(editor.selected_text(), "");
    application.dispatch_event(oui::TextEvent{U'X'});
    EXPECT_EQ(editor.text(), "Xabcdef\nghijkl\nmnopqr");

    // The next click is an ordinary one again.
    application.dispatch_event(oui::MouseEvent{3, 1, true});
    application.dispatch_event(oui::MouseEvent{3, 1, false, 0, false, true});
    application.dispatch_event(oui::TextEvent{U'Y'});
    EXPECT_NE(editor.text(), "XYabcdef\nghijkl\nmnopqr");
}

TEST(OuiApplicationTests, ModalsRestoreAndSaveTheirPlacementByKey)
{
    auto backend{std::make_unique<RoomyBackend>()};
    auto* const backend_reference{backend.get()};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));
    std::vector<std::string> loaded;
    std::optional<oui::WindowPlacement> saved;
    application.set_window_placement_store(
        [&loaded](std::string_view key) -> std::optional<oui::WindowPlacement>
        {
            loaded.emplace_back(key);
            return oui::WindowPlacement{oui::Point{4, 2}, oui::Size{24, 7}};
        },
        [&saved](std::string_view key, const oui::WindowPlacement& placement)
        {
            if(key == "remembered")
            {
                saved = placement;
            }
        });

    auto dialog{oui::window("Remembered")};
    dialog->set_resizable(true);
    dialog->set_placement_key("remembered");
    backend_reference->push_event(oui::KeyEvent{oui::Key::escape});
    static_cast<void>(application.show_modal(std::move(dialog)));

    EXPECT_EQ(loaded, (std::vector<std::string>{"remembered"}));
    ASSERT_TRUE(saved);
    ASSERT_TRUE(saved->position);
    ASSERT_TRUE(saved->size);
    EXPECT_EQ(saved->position->x, 4);
    EXPECT_EQ(saved->position->y, 2);
    EXPECT_EQ(saved->size->width, 24);
    EXPECT_EQ(saved->size->height, 7);

    // A window without a key is neither restored nor saved.
    saved.reset();
    backend_reference->push_event(oui::KeyEvent{oui::Key::escape});
    static_cast<void>(application.show_modal(oui::window("Anonymous")));
    EXPECT_EQ(loaded.size(), 1u);
    EXPECT_FALSE(saved);
}

namespace
{

// Counts how the application waits: a real terminal backend would block in wait_for_input.
class WaitCountingBackend final : public oui::Backend
{
  public:
    [[nodiscard]] oui::Size size() const override { return {20, 4}; }

    std::optional<oui::Event> poll_event() override
    {
        if(m_events.empty())
        {
            return std::nullopt;
        }
        const auto event{m_events.front()};
        m_events.erase(m_events.begin());
        return event;
    }

    void present(const oui::CellBuffer&) override {}

    void wait_for_input(std::optional<std::chrono::steady_clock::time_point> deadline) override
    {
        ++ waits;
        last_deadline = deadline;
        m_events.push_back(oui::TextEvent{U'x'});
    }

    void wake() override { ++ wakes; }

    int waits{};
    int wakes{};
    std::optional<std::chrono::steady_clock::time_point> last_deadline;

  private:
    std::vector<oui::Event> m_events;
};

} // namespace

TEST(OuiApplicationTests, WaitsForInputOnlyWhenIdle)
{
    auto backend{std::make_unique<WaitCountingBackend>()};
    auto& counter{*backend};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Idle"));

    application.run_frame();
    EXPECT_EQ(counter.waits, 1) << "nothing happened, so the frame ends waiting for input";
    application.run_frame();
    EXPECT_EQ(counter.waits, 1) << "the frame handled the input that woke it, so it does not wait";
    application.run_frame();
    EXPECT_EQ(counter.waits, 2);

    application.post([] {});
    EXPECT_EQ(counter.wakes, 1) << "post must wake a waiting loop";
    application.run_frame();
    EXPECT_EQ(counter.waits, 2) << "a frame that ran posted work does not wait";
}

namespace
{

std::tuple<int, int, int, int> place(const oui::Widget& widget)
{
    const auto bounds{widget.bounds()};
    return {bounds.x, bounds.y, bounds.width, bounds.height};
}

} // namespace

TEST(OuiWidgetTests, TabViewWithoutCloseMarksKeepsItsTabs)
{
    oui::TabView tabs;
    tabs.set_closable(false);
    tabs.set_close_on_right_click(true);
    tabs.add_tab("one", oui::label("1"));
    tabs.add_tab("two", oui::label("2"));
    auto asked{0};
    const auto connection{tabs.on_close_requested([&asked](std::size_t) { ++ asked; })};
    tabs.arrange({0, 0, 20, 3});
    oui::CellBuffer buffer{20, 3};
    oui::CellBufferPainter painter{buffer};
    tabs.paint(painter, oui::Theme{});
    std::string header;
    for(int x{}; x < 12; ++ x)
    {
        header += oui::encode_utf8(buffer.cell_at(x, 0).glyph);
    }
    EXPECT_EQ(header, " one  two   ") << "titles only, two cells apart";
    oui::MouseEvent right_click{2, 0, true};
    right_click.right = true;
    tabs.handle_event(right_click);
    tabs.handle_event(oui::MouseEvent{6, 0, true});
    EXPECT_EQ(asked, 0);
    EXPECT_EQ(tabs.selected_index(), 1u) << "a click on a title still selects it";
}

TEST(OuiWidgetTests, TabViewSwitchesTabsWithCtrlPageKeysEvenFromAMarkdownView)
{
    oui::TabView tabs;
    auto& first{tabs.add_tab("one", oui::markdown_view("one"))};
    tabs.add_tab("two", oui::markdown_view("two"));
    tabs.arrange({0, 0, 20, 5});
    // The view leaves the keys unused, so they reach the tabs it is in.
    EXPECT_FALSE(first.handle_event(oui::KeyEvent{oui::Key::page_down, true}));
    EXPECT_TRUE(tabs.handle_event(oui::KeyEvent{oui::Key::page_down, true}));
    EXPECT_EQ(tabs.selected_index(), 1u);
    EXPECT_TRUE(tabs.handle_event(oui::KeyEvent{oui::Key::page_up, true}));
    EXPECT_EQ(tabs.selected_index(), 0u);
    EXPECT_FALSE(tabs.handle_event(oui::KeyEvent{oui::Key::page_down})) << "Page Down alone is the content's";
}

TEST(OuiWidgetTests, TabViewHandsATabOverToAnotherOne)
{
    oui::TabView from;
    oui::TabView to;
    from.add_tab("one", oui::label("1"));
    auto& moving{from.add_tab("two", oui::label("2"))};
    from.select(1);
    auto content{from.take_tab(1)};
    ASSERT_EQ(content.get(), &moving);
    EXPECT_EQ(from.child_count(), 1u);
    EXPECT_EQ(from.selected_index(), 0u);
    to.add_tab("two", std::move(content));
    EXPECT_EQ(to.content_at<oui::Label>(0), &moving);
    EXPECT_EQ(moving.parent(), &to);
    EXPECT_EQ(from.take_tab(5), nullptr);
}

TEST(OuiWidgetTests, SplitViewSharesTheSpaceAndGivesItAllToTheOneLeft)
{
    oui::SplitView split;
    auto& left{split.add(oui::label("L"))};
    auto& right{split.add(oui::label("R"))};
    EXPECT_THROW(split.add(oui::label("X")), std::logic_error);
    split.arrange({0, 0, 21, 5});
    EXPECT_EQ(place(left), std::make_tuple(0, 0, 10, 5));
    EXPECT_EQ(place(right), std::make_tuple(11, 0, 10, 5)) << "the line takes the column between";
    split.set_orientation(oui::SplitView::Orientation::vertical);
    split.arrange({0, 0, 21, 5});
    EXPECT_EQ(place(left), std::make_tuple(0, 0, 21, 2));
    EXPECT_EQ(place(right), std::make_tuple(0, 3, 21, 2)) << "turned, the line runs across";
    right.set_visible(false);
    split.arrange({0, 0, 21, 5});
    EXPECT_EQ(place(left), std::make_tuple(0, 0, 21, 5));
}

TEST(OuiWidgetTests, SplitViewNestsAnotherInPlaceOfAChild)
{
    oui::SplitView outer;
    auto& left{outer.add(oui::label("L"))};
    auto& right{outer.add(oui::label("R"))};
    auto inner{oui::split_view(oui::SplitView::Orientation::vertical)};
    auto& inner_reference{*inner};
    auto taken{outer.replace(right, std::move(inner))};
    ASSERT_EQ(taken.get(), &right);
    auto& below{inner_reference.add(oui::label("B"))};
    inner_reference.add(std::move(taken));
    outer.arrange({0, 0, 21, 9});
    EXPECT_EQ(place(left), std::make_tuple(0, 0, 10, 9));
    EXPECT_EQ(place(below), std::make_tuple(11, 0, 10, 4));
    EXPECT_EQ(place(right), std::make_tuple(11, 5, 10, 4));
    EXPECT_THROW(static_cast<void>(outer.replace(right, oui::label("X"))), std::logic_error);
}

TEST(OuiWidgetTests, SplitViewMovesItsLineByCells)
{
    oui::SplitView split;
    auto& left{split.add(oui::label("L"))};
    split.add(oui::label("R"));
    split.arrange({0, 0, 21, 5});
    ASSERT_EQ(left.bounds().width, 10);
    split.move_divider(3);
    EXPECT_EQ(left.bounds().width, 13);
    split.move_divider(-2);
    EXPECT_EQ(left.bounds().width, 11);
    split.move_divider(-50);
    EXPECT_EQ(left.bounds().width, 1) << "each side keeps a cell";
    split.arrange({0, 0, 21, 5});
    EXPECT_EQ(left.bounds().width, 1) << "and the ratio keeps it there";
}

TEST(OuiWidgetTests, SplitViewMovesItsLineWhereTheMouseDragsIt)
{
    oui::SplitView split{oui::SplitView::Orientation::vertical};
    auto& top{split.add(oui::label("T"))};
    split.add(oui::label("B"));
    split.arrange({0, 0, 10, 11});
    ASSERT_EQ(top.bounds().height, 5);
    EXPECT_TRUE(split.handle_event(oui::MouseEvent{3, 5, true}));
    oui::MouseEvent drag{3, 8, false};
    drag.dragging = true;
    EXPECT_TRUE(split.handle_event(drag));
    oui::MouseEvent release{3, 8, false};
    release.released = true;
    EXPECT_TRUE(split.handle_event(release));
    split.arrange({0, 0, 10, 11});
    EXPECT_EQ(top.bounds().height, 8);
    EXPECT_FALSE(split.handle_event(oui::MouseEvent{3, 2, true})) << "a press off the line is not its";
}

TEST(OuiApplicationTests, SleepsOnlyUntilTheNextTimerIsDue)
{
    auto backend{std::make_unique<WaitCountingBackend>()};
    auto& counter{*backend};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Idle"));

    application.run_frame();
    EXPECT_FALSE(counter.last_deadline.has_value()) << "without timers the wait has no end but input";

    const auto before{std::chrono::steady_clock::now()};
    const auto timer{application.start_timer(std::chrono::hours{1}, [] {})};
    application.run_frame();
    application.run_frame();
    ASSERT_TRUE(counter.last_deadline.has_value());
    EXPECT_GE(*counter.last_deadline, before + std::chrono::hours{1});
}

TEST(OuiApplicationTests, RunsATimerEachTimeItIsDueUntilItIsStopped)
{
    oui::Application application{std::make_unique<MemoryBackend>()};
    auto calls{0};
    auto timer{application.start_timer(std::chrono::milliseconds{0}, [&calls] { ++ calls; })};
    EXPECT_TRUE(timer.is_active());

    application.run_frame();
    application.run_frame();
    EXPECT_EQ(calls, 2);

    timer.stop();
    EXPECT_FALSE(timer.is_active());
    application.run_frame();
    EXPECT_EQ(calls, 2);
}

TEST(OuiApplicationTests, RunsNoTimerBeforeItsIntervalOrAfterItsHandleIsGone)
{
    oui::Application application{std::make_unique<MemoryBackend>()};
    auto slow_calls{0};
    auto gone_calls{0};
    const auto slow{application.start_timer(std::chrono::hours{1}, [&slow_calls] { ++ slow_calls; })};
    {
        const auto gone{application.start_timer(std::chrono::milliseconds{0}, [&gone_calls] { ++ gone_calls; })};
    }
    application.run_frame();
    EXPECT_EQ(slow_calls, 0);
    EXPECT_EQ(gone_calls, 0);
}

TEST(OuiApplicationTests, ATimerStoppedByAnotherInTheSameFrameDoesNotRun)
{
    oui::Application application{std::make_unique<MemoryBackend>()};
    oui::Timer second;
    auto second_calls{0};
    const auto first{application.start_timer(std::chrono::milliseconds{0}, [&second] { second.stop(); })};
    second = application.start_timer(std::chrono::milliseconds{0}, [&second_calls] { ++ second_calls; });
    application.run_frame();
    EXPECT_EQ(second_calls, 0);
}

TEST(OuiApplicationTests, AnnouncesWhenTheTerminalGainsOrLosesFocus)
{
    oui::Application application{std::make_unique<MemoryBackend>()};
    std::vector<bool> changes;
    const auto connection{
        application.on_terminal_focus_changed([&changes](bool focused) { changes.push_back(focused); })};
    application.dispatch_event(oui::FocusEvent{false});
    application.dispatch_event(oui::FocusEvent{true});
    EXPECT_EQ(changes, (std::vector<bool>{false, true}));
}

namespace
{

// Remembers the click count of every press it receives.
class ClickProbe final : public oui::Widget
{
  public:
    [[nodiscard]] oui::Size measure(const oui::Constraints& constraints) const override
    {
        return constraints.constrain({constraints.max_width, constraints.max_height});
    }

    bool handle_event(const oui::Event& event) override
    {
        if(const auto* const mouse{std::get_if<oui::MouseEvent>(&event)}; mouse && mouse->pressed)
        {
            clicks.push_back(mouse->clicks);
        }
        return true;
    }

    std::vector<int> clicks;
};

} // namespace

TEST(OuiApplicationTests, CountsRepeatedClicksOnTheSamePlace)
{
    oui::Application application{std::make_unique<MemoryBackend>()};
    auto probe{std::make_unique<ClickProbe>()};
    auto& probe_reference{*probe};
    application.set_content(std::move(probe));
    application.render();
    const auto click{[&application](double x)
                     {
                         application.dispatch_event(oui::MouseEvent{x, 1, true});
                         application.dispatch_event(oui::MouseEvent{x, 1, false, 0, false, true});
                     }};
    click(3);
    click(3);
    click(3);
    click(3);
    click(7);
    click(7);
    EXPECT_EQ(probe_reference.clicks, (std::vector<int>{1, 2, 3, 1, 1, 2}));
}

TEST(OuiApplicationTests, BoundShortcutsTakePriorityOverTheFocusedWidget)
{
    oui::Application application{std::make_unique<MemoryBackend>()};
    auto editor_pointer{oui::text_edit()};
    auto& editor{*editor_pointer};
    editor.set_text("one\ntwo\nthree\nfour\nfive\nsix");
    application.set_content(std::move(editor_pointer));
    application.focus(editor);
    auto switched{0};
    application.commands().register_command("tabs.next", "Next Tab", [&switched] { ++ switched; });
    application.commands().bind("tabs.next", oui::KeyShortcut{oui::Key::page_down, true});

    application.dispatch_event(oui::KeyEvent{oui::Key::page_down, true});
    EXPECT_EQ(switched, 1);
    EXPECT_EQ(editor.line_column().first, 1u) << "the editor did not page as well";

    // Keys nobody bound still reach the widget.
    application.dispatch_event(oui::KeyEvent{oui::Key::page_down});
    EXPECT_GT(editor.line_column().first, 1u);
}

TEST(OuiApplicationTests, OpensAModalFromAControlShortcutWhileTextEditHasFocus)
{
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_reference{backend.get()};
    oui::Application application{std::move(backend)};
    auto editor{oui::text_edit()};
    auto* const editor_reference{editor.get()};
    application.set_content(std::move(editor));
    application.render();

    auto opened{false};
    application.commands().register_command("edit.find", "Find",
                                            [&application, &opened]
                                            {
                                                opened = true;
                                                auto dialog{oui::window("Find")};
                                                auto content{oui::column()};
                                                content->add(oui::line_edit());
                                                dialog->add(std::move(content));
                                                static_cast<void>(application.show_modal(std::move(dialog)));
                                            });
    application.commands().bind("edit.find", {U'f', true});
    backend_reference->push_event(oui::KeyEvent{oui::Key::escape});

    application.dispatch_event(oui::TextEvent{6, true});

    EXPECT_TRUE(opened);
    EXPECT_EQ(application.focused(), editor_reference);
}

TEST(OuiApplicationTests, DoesNotDispatchGlobalShortcutsBehindAModalDialog)
{
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_reference{backend.get()};
    oui::Application application{std::move(backend)};
    application.set_content(oui::button("Background"));
    auto calls{0};
    application.commands().register_command("file.close", "Close", [&calls] { ++ calls; });
    application.commands().bind("file.close", {U'w', true});

    auto dialog{oui::window("Modal")};
    backend_reference->push_event(oui::TextEvent{U'w', true});
    backend_reference->push_event(oui::KeyEvent{oui::Key::escape});

    static_cast<void>(application.show_modal(std::move(dialog)));

    EXPECT_EQ(calls, 0);
}

TEST(OuiApplicationTests, ReplacesDestroyedFocusedWidgetsBeforeDispatchingEvents)
{
    auto backend{std::make_unique<MemoryBackend>()};
    oui::Application application{std::move(backend)};
    auto root{std::make_unique<OverlayTestContainer>()};
    auto* const root_reference{root.get()};
    auto removed{oui::button("Removed")};
    auto* const removed_reference{removed.get()};
    auto fallback{oui::button("Fallback")};
    auto* const fallback_reference{fallback.get()};
    root->add(std::move(removed));
    root->add(std::move(fallback));
    application.set_content(std::move(root));
    application.focus(*removed_reference);

    static_cast<void>(root_reference->remove_at(0));
    application.dispatch_event(oui::KeyEvent{oui::Key::space});

    EXPECT_EQ(application.focused(), fallback_reference);
}

TEST(OuiApplicationTests, DropsMouseCaptureWhenItsWidgetIsDestroyed)
{
    auto backend{std::make_unique<MemoryBackend>()};
    oui::Application application{std::move(backend)};
    auto root{std::make_unique<OverlayTestContainer>()};
    auto* const root_reference{root.get()};
    auto fallback{oui::button("Fallback")};
    auto* const fallback_reference{fallback.get()};
    auto removed{oui::button("Removed")};
    root->add(std::move(fallback));
    root->add(std::move(removed));
    application.set_content(std::move(root));
    application.render();

    application.dispatch_event(oui::MouseEvent{1, 0, true});
    static_cast<void>(root_reference->remove_at(1));
    application.dispatch_event(oui::MouseEvent{1, 0, false, 0, false, true});

    EXPECT_EQ(application.focused(), fallback_reference);
}

TEST(OuiApplicationTests, StressesWidgetLifetimeAndModalIsolation)
{
    constexpr int cycle_count{250};
    for(int cycle{}; cycle < cycle_count; ++ cycle)
    {
        auto backend{std::make_unique<MemoryBackend>()};
        auto* const backend_reference{backend.get()};
        oui::Application application{std::move(backend)};
        auto root{std::make_unique<OverlayTestContainer>()};
        auto* const root_reference{root.get()};
        auto fallback{oui::button("Fallback")};
        auto* const fallback_reference{fallback.get()};
        auto transient{oui::button("Transient")};
        auto* const transient_reference{transient.get()};
        root->add(std::move(fallback));
        root->add(std::move(transient));
        application.set_content(std::move(root));
        application.render();
        application.focus(*transient_reference);

        application.dispatch_event(oui::MouseEvent{1, 0, true});
        static_cast<void>(root_reference->remove_at(1));
        application.dispatch_event(oui::MouseEvent{1, 0, false, 0, false, true});
        application.dispatch_event(oui::KeyEvent{oui::Key::tab});

        EXPECT_EQ(application.focused(), fallback_reference);

        auto global_command_calls{0};
        application.commands().register_command("file.close", "Close",
                                                [&global_command_calls] { ++ global_command_calls; });
        application.commands().bind("file.close", {U'w', true});
        auto dialog{oui::window("Modal")};
        backend_reference->push_event(oui::TextEvent{U'w', true});
        backend_reference->push_event(oui::KeyEvent{oui::Key::escape});

        EXPECT_EQ(application.show_modal(std::move(dialog)), oui::DialogResult::cancelled);
        EXPECT_EQ(global_command_calls, 0);
        EXPECT_EQ(application.focused(), fallback_reference);
    }
}

TEST(OuiScrollViewTests, ScrollsWithTheMouseWheel)
{
    auto scroll{oui::scroll_view()};
    scroll->add(std::make_unique<FixedWidget>(oui::Size{8, 10}));
    scroll->arrange({0, 0, 8, 3});

    EXPECT_TRUE(scroll->handle_event(oui::MouseEvent{0, 0, false, -2}));
    EXPECT_EQ(scroll->scroll_offset(), 2);
    EXPECT_TRUE(scroll->handle_event(oui::MouseEvent{0, 0, false, 2}));
    EXPECT_EQ(scroll->scroll_offset(), 0);
}

TEST(OuiScrollViewTests, PaintsAScrollBarWhenContentExceedsTheViewport)
{
    auto scroll{oui::scroll_view()};
    scroll->add(std::make_unique<FixedWidget>(oui::Size{8, 10}));
    scroll->arrange({0, 0, 8, 3});
    oui::CellBuffer buffer{8, 3};
    oui::CellBufferPainter painter{buffer};
    scroll->paint(painter, {});

    EXPECT_EQ(buffer.cell_at(7, 0).background, oui::Theme{}.panel_background);
}

TEST(OuiApplicationTests, RoutesMousePressToTheWidgetUnderThePointer)
{
    auto backend{std::make_unique<MemoryBackend>()};
    oui::Application application{std::move(backend)};
    auto content{oui::row()};
    auto calls{0};
    oui::button("Click", *content, [&calls] { ++ calls; });
    application.set_content(std::move(content));
    application.render();
    application.dispatch_event(oui::MouseEvent{1, 0, true});
    application.dispatch_event(oui::MouseEvent{1, 0, false, 0, false, true});

    EXPECT_EQ(calls, 1);
}

TEST(OuiApplicationTests, RunsPostedWorkOnTheUiThreadDuringTheNextFrame)
{
    auto backend{std::make_unique<MemoryBackend>()};
    oui::Application application{std::move(backend)};
    auto calls{0};

    application.post([&calls] { ++ calls; });
    application.run_frame();

    EXPECT_EQ(calls, 1);
}

TEST(OuiApplicationTests, QuitsWhenItsWindowCloses)
{
    auto backend{std::make_unique<MemoryBackend>()};
    oui::Application application{std::move(backend)};
    auto window{oui::window("Close")};
    auto* const window_reference{window.get()};
    application.set_window(std::move(window));
    application.post([window_reference] { window_reference->close(oui::DialogResult::cancelled); });

    EXPECT_EQ(application.exec(), 0);
}

TEST(OuiApplicationTests, RoutesEscapeFromTheFocusedWidgetToItsWindow)
{
    auto backend{std::make_unique<MemoryBackend>()};
    oui::Application application{std::move(backend)};
    auto window{oui::window("Close")};
    auto content{oui::column()};
    oui::button("Focus", *content);
    window->add(std::move(content));
    application.set_window(std::move(window));
    application.post([&application] { application.dispatch_event(oui::KeyEvent{oui::Key::escape}); });

    EXPECT_EQ(application.exec(), 0);
}

TEST(OuiCommandTests, CtrlInsertDispatchesToCopyNotFileOpen)
{
    oui::CommandRegistry commands;
    auto copy_executed{false};
    auto open_executed{false};
    commands.register_command("edit.copy", "Copy", [&copy_executed] { copy_executed = true; });
    commands.register_command("file.open", "Open", [&open_executed] { open_executed = true; });
    commands.bind("edit.copy", {oui::Key::insert_, true});
    commands.bind("file.open", {U'o', true});

    EXPECT_TRUE(commands.dispatch(oui::Event{oui::KeyEvent{oui::Key::insert_, true}}));
    EXPECT_TRUE(copy_executed);
    EXPECT_FALSE(open_executed);
}

TEST(OuiCommandTests, ShiftInsertDispatchesToPaste)
{
    oui::CommandRegistry commands;
    auto paste_executed{false};
    commands.register_command("edit.paste", "Paste", [&paste_executed] { paste_executed = true; });
    commands.bind("edit.paste", {oui::Key::insert_, false, false, true});

    EXPECT_TRUE(commands.dispatch(oui::Event{oui::KeyEvent{oui::Key::insert_, false, false, true}}));
    EXPECT_TRUE(paste_executed);
}

TEST(OuiApplicationTests, CopyAndPasteWithInsertKeysPreservesSpecialCharacters)
{
    auto backend{std::make_unique<MemoryBackend>()};
    oui::Application application{std::move(backend)};
    auto tabs{oui::tab_view()};
    auto editor{oui::text_edit()};
    auto* const editor_reference{editor.get()};
    const auto original{"#include { code } here"};
    editor->set_text(original);
    tabs->add_tab("test", std::move(editor));
    application.set_content(std::move(tabs));
    application.focus(*editor_reference);
    application.render();

    oui::Clipboard clipboard;
    application.commands().register_command(
        "edit.copy", "Copy",
        [&editor_reference, &clipboard] { static_cast<void>(clipboard.set_text(editor_reference->selected_text())); });
    application.commands().register_command("edit.paste", "Paste", [&editor_reference, &clipboard]
                                            { editor_reference->paste(clipboard.text()); });
    application.commands().register_command("edit.select_all", "Select All",
                                            [&editor_reference] { editor_reference->select_all(); });
    application.commands().bind("edit.copy", {oui::Key::insert_, true});
    application.commands().bind("edit.paste", {oui::Key::insert_, false, false, true});
    application.commands().bind("edit.select_all", {U'a', true});

    application.dispatch_event(oui::Event{oui::TextEvent{U'a', true}});
    EXPECT_EQ(editor_reference->selected_text(), original);

    application.dispatch_event(oui::Event{oui::KeyEvent{oui::Key::insert_, true}});
    EXPECT_EQ(clipboard.text(), original);

    editor_reference->select_all();
    editor_reference->paste({});
    EXPECT_TRUE(editor_reference->text().empty());

    application.dispatch_event(oui::Event{oui::KeyEvent{oui::Key::insert_, false, false, true}});
    EXPECT_EQ(editor_reference->text(), original);
}

TEST(OuiApplicationTests, CtrlCAndCtrlVPreserveSpecialCharacters)
{
    auto backend{std::make_unique<MemoryBackend>()};
    oui::Application application{std::move(backend)};
    auto tabs{oui::tab_view()};
    auto editor{oui::text_edit()};
    auto* const editor_reference{editor.get()};
    const auto original{"#include <stdio.h> { return 0; } #endif"};
    editor->set_text(original);
    tabs->add_tab("test", std::move(editor));
    application.set_content(std::move(tabs));
    application.focus(*editor_reference);
    application.render();

    oui::Clipboard clipboard;
    application.commands().register_command(
        "edit.copy", "Copy",
        [&editor_reference, &clipboard] { static_cast<void>(clipboard.set_text(editor_reference->selected_text())); });
    application.commands().register_command("edit.paste", "Paste", [&editor_reference, &clipboard]
                                            { editor_reference->paste(clipboard.text()); });
    application.commands().register_command("edit.select_all", "Select All",
                                            [&editor_reference] { editor_reference->select_all(); });
    application.commands().bind("edit.copy", {U'c', true});
    application.commands().bind("edit.paste", {U'v', true});
    application.commands().bind("edit.select_all", {U'a', true});

    application.dispatch_event(oui::Event{oui::TextEvent{U'a', true}});
    EXPECT_EQ(editor_reference->selected_text(), original);

    application.dispatch_event(oui::Event{oui::TextEvent{U'c', true}});
    EXPECT_EQ(clipboard.text(), original);

    editor_reference->select_all();
    editor_reference->paste({});
    EXPECT_TRUE(editor_reference->text().empty());

    application.dispatch_event(oui::Event{oui::TextEvent{U'v', true}});
    EXPECT_EQ(editor_reference->text(), original);
}

// A backend that keeps the clipboard itself, so the tests never reach the system's.
class ClipboardKeepingBackend final : public oui::Backend
{
  public:
    [[nodiscard]] oui::Size size() const override { return {40, 6}; }
    std::optional<oui::Event> poll_event() override { return std::nullopt; }
    void present(const oui::CellBuffer&) override {}
    [[nodiscard]] bool keeps_clipboard() const override { return true; }
    [[nodiscard]] std::string clipboard_text() const override { return *m_clipboard; }
    void set_clipboard_text(std::string_view text) override { *m_clipboard = text; }

    std::shared_ptr<std::string> m_clipboard{std::make_shared<std::string>()};
};

TEST(OuiApplicationTests, ATextEditCopiesThroughTheClipboardWhereNoCommandDoes)
{
    // As a script's read-only log in oui_run, which registers no edit commands: copying out of it must still work.
    auto backend{std::make_unique<ClipboardKeepingBackend>()};
    const auto clipboard{backend->m_clipboard};
    oui::Application application{std::move(backend)};
    auto log{oui::text_edit()};
    auto* const log_reference{log.get()};
    log->set_text("first ñ\nsecond");
    log->set_read_only(true);
    application.set_content(std::move(log));
    application.focus(*log_reference);
    application.render();

    application.dispatch_event(oui::Event{oui::TextEvent{U'a', true}});
    application.dispatch_event(oui::Event{oui::TextEvent{U'c', true}});
    EXPECT_EQ(*clipboard, "first ñ\nsecond");

    *clipboard = "pasted";
    application.dispatch_event(oui::Event{oui::TextEvent{U'x', true}});
    EXPECT_EQ(*clipboard, "first ñ\nsecond") << "cutting from a read-only text copies";
    EXPECT_EQ(log_reference->text(), "first ñ\nsecond") << "and leaves it whole";
    application.dispatch_event(oui::Event{oui::TextEvent{U'v', true}});
    EXPECT_EQ(log_reference->text(), "first ñ\nsecond");

    log_reference->set_read_only(false);
    *clipboard = "pasted";
    application.dispatch_event(oui::Event{oui::TextEvent{U'v', true}});
    EXPECT_EQ(log_reference->text(), "pasted");
}

TEST(OuiApplicationTests, ShiftInsertPastesWhereCtrlVDoesAndNoCommandIsBound)
{
    // As a dialog's field, where the application's shortcuts wait, or any field in oui_run.
    auto backend{std::make_unique<ClipboardKeepingBackend>()};
    const auto clipboard{backend->m_clipboard};
    oui::Application application{std::move(backend)};
    auto content{oui::column()};
    auto& field{content->add(oui::line_edit())};
    auto& edit{content->add(oui::text_edit())};
    application.set_content(std::move(content));
    application.focus(field);
    application.render();

    *clipboard = "pegado";
    application.dispatch_event(oui::Event{oui::KeyEvent{oui::Key::insert_, false, false, true}});
    EXPECT_EQ(field.text(), "pegado");

    application.focus(edit);
    application.dispatch_event(oui::Event{oui::KeyEvent{oui::Key::insert_, false, false, true}});
    EXPECT_EQ(edit.text(), "pegado");
    application.dispatch_event(oui::Event{oui::KeyEvent{oui::Key::insert_}});
    EXPECT_EQ(edit.text(), "pegado") << "Insert alone pastes nothing";
}

TEST(OuiApplicationTests, TheClipboardFollowsTheBackendToTheOneThatReplacedIt)
{
    // As oui_run's, which starts in one backend and opens its window in another: the first is gone by the copy.
    oui::Application application{std::make_unique<ClipboardKeepingBackend>()};
    auto window{std::make_unique<ClipboardKeepingBackend>()};
    const auto clipboard{window->m_clipboard};
    application.set_backend(std::move(window));

    EXPECT_TRUE(application.clipboard().set_text("copied"));
    EXPECT_EQ(*clipboard, "copied");
    EXPECT_EQ(application.clipboard().text(), "copied");
}

TEST(OuiTabSwitcherTests, IsInitiallyClosed)
{
    oui::TabSwitcher switcher;
    EXPECT_FALSE(switcher.is_open());
}

TEST(OuiTabSwitcherTests, OpensAsOverlay)
{
    auto backend{std::make_unique<MemoryBackend>()};
    oui::Application application{std::move(backend)};
    application.set_content(oui::button("bg"));

    oui::TabSwitcher switcher;
    switcher.open(application, {"Tab A", "Tab B", "Tab C"}, 0);
    EXPECT_TRUE(switcher.is_open());
}

TEST(OuiTabSwitcherTests, DoesNotOpenTwice)
{
    auto backend{std::make_unique<MemoryBackend>()};
    oui::Application application{std::move(backend)};
    application.set_content(oui::button("bg"));

    oui::TabSwitcher switcher;
    switcher.open(application, {"Tab A", "Tab B"}, 0);
    EXPECT_TRUE(switcher.is_open());
    switcher.open(application, {"Tab A", "Tab B"}, 0);
    EXPECT_TRUE(switcher.is_open());
}

TEST(OuiTabSwitcherTests, DoesNotOpenWithEmptyTitles)
{
    auto backend{std::make_unique<MemoryBackend>()};
    oui::Application application{std::move(backend)};
    application.set_content(oui::button("bg"));

    oui::TabSwitcher switcher;
    switcher.open(application, {}, 0);
    EXPECT_FALSE(switcher.is_open());
}

TEST(OuiTabSwitcherTests, ConfirmsSelection)
{
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_ref{backend.get()};
    oui::Application application{std::move(backend)};
    application.set_content(oui::button("bg"));

    oui::TabSwitcher switcher;
    std::size_t confirmed_index{std::numeric_limits<std::size_t>::max()};
    switcher.on_confirmed([&confirmed_index](std::size_t index) { confirmed_index = index; });
    switcher.open(application, {"Tab A", "Tab B", "Tab C"}, 0);

    backend_ref->push_event(oui::KeyEvent{oui::Key::down});
    backend_ref->push_event(oui::KeyEvent{oui::Key::enter});
    application.run_frame();
    application.run_frame();

    EXPECT_EQ(confirmed_index, 1u);
    EXPECT_FALSE(switcher.is_open());
}

TEST(OuiTabSwitcherTests, CancelsWithEscape)
{
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_ref{backend.get()};
    oui::Application application{std::move(backend)};
    application.set_content(oui::button("bg"));

    oui::TabSwitcher switcher;
    bool cancelled{false};
    switcher.on_cancelled([&cancelled] { cancelled = true; });
    switcher.open(application, {"Tab A", "Tab B"}, 0);

    backend_ref->push_event(oui::KeyEvent{oui::Key::escape});
    application.run_frame();
    application.run_frame();

    EXPECT_TRUE(cancelled);
    EXPECT_FALSE(switcher.is_open());
}

TEST(OuiTabSwitcherTests, ConfirmedHandlerCanFocusTheSelectedContent)
{
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_ref{backend.get()};
    oui::Application application{std::move(backend)};
    auto tabs{oui::tab_view()};
    tabs->add_tab("Doc", oui::text_edit());
    tabs->add_tab("Info", oui::label("about"));
    auto& tabs_reference{*tabs};
    application.set_content(std::move(tabs));

    oui::TabSwitcher switcher;
    bool focused_editor{false};
    switcher.on_confirmed(
        [&application, &tabs_reference, &focused_editor](std::size_t index)
        {
            tabs_reference.select(index);
            if(auto* const editor{tabs_reference.selected_content<oui::TextEdit>()})
            {
                application.focus(*editor);
                focused_editor = true;
            }
        });
    switcher.open(application, {"Doc", "Info"}, 0);
    EXPECT_TRUE(switcher.is_open());

    backend_ref->push_event(oui::KeyEvent{oui::Key::enter});
    application.run_frame();
    application.run_frame();

    EXPECT_EQ(tabs_reference.selected_index(), 0u);
    EXPECT_TRUE(focused_editor) << "confirming a document tab must focus its editor";
    EXPECT_FALSE(switcher.is_open());
}

TEST(OuiWindowTests, ClosingTwiceFiresTheSignalOnce)
{
    auto dialog{oui::window("Dialog")};
    int fires{};
    dialog->on_closed([&fires](oui::DialogResult) { ++ fires; });

    dialog->close(oui::DialogResult::accepted);
    dialog->close(oui::DialogResult::cancelled);

    EXPECT_EQ(fires, 1);
}

TEST(OuiFolderViewTests, CustomBackgroundReplacesTheThemePanelBackground)
{
    const oui::Color teal{0, 85, 85};
    auto tree{oui::folder_view()};
    tree->set_colors(teal, &oui::Theme::panel_foreground);
    tree->arrange({0, 0, 24, 6});
    oui::CellBuffer buffer{24, 6};
    oui::CellBufferPainter painter{buffer};
    tree->paint(painter, {});
    EXPECT_EQ(buffer.cell_at(0, 0).background, teal);

    oui::Theme theme;
    theme.panel_background = {40, 40, 40};
    auto default_tree{oui::folder_view()};
    default_tree->arrange({0, 0, 24, 6});
    oui::CellBuffer default_buffer{24, 6};
    oui::CellBufferPainter default_painter{default_buffer};
    default_tree->paint(default_painter, theme);
    EXPECT_EQ(default_buffer.cell_at(0, 0).background, theme.panel_background);
}

TEST(OuiFolderViewTests, ItsNamesAreInThePanelsTextColourNotTheListsOne)
{
    const auto root{std::filesystem::temp_directory_path() / "olagarro_folder_view_text_colour"};
    std::error_code error;
    std::filesystem::remove_all(root, error);
    ASSERT_TRUE(std::filesystem::create_directories(root / "child", error));
    // The first entry is selected; a second folder after it is not.
    ASSERT_TRUE(std::filesystem::create_directories(root / "other", error));
    // As a dark desktop theme has them: the panel dark with light text, a list's text dark.
    oui::Theme theme;
    theme.panel_background = {30, 30, 40};
    theme.panel_foreground = {220, 220, 220};
    theme.list_foreground = {0, 0, 0};
    auto tree{oui::folder_view()};
    tree->set_scope(root);
    tree->show_folder(root);
    tree->arrange({0, 0, 24, 6});
    oui::CellBuffer buffer{24, 6};
    oui::CellBufferPainter painter{buffer};
    tree->paint(painter, theme);
    // Under the header that names the panel.
    auto found{false};
    for(int y{}; y < 6 && !found; ++ y)
    {
        std::u32string row;
        for(int x{}; x < 24; ++ x)
        {
            row += buffer.cell_at(x, y).glyph;
        }
        if(const auto name{row.find(U"other")}; name != std::u32string::npos)
        {
            found = true;
            EXPECT_EQ(buffer.cell_at(static_cast<int>(name), y).foreground, theme.panel_foreground);
        }
    }
    EXPECT_TRUE(found);
    std::filesystem::remove_all(root, error);
}

TEST(OuiFolderViewTests, BackspaceMovesUpWithoutLeavingTheOpenedRoot)
{
    const auto root{std::filesystem::temp_directory_path() / "olagarro_folder_tree_backspace"};
    const auto child{root / "child"};
    std::error_code error;
    std::filesystem::remove_all(root, error);
    ASSERT_TRUE(std::filesystem::create_directories(child, error));

    auto tree{oui::folder_view()};
    tree->set_scope(root);
    tree->show_folder(root);
    tree->arrange({0, 0, 24, 6});
    // The project's own folder has no ".." row: its first entry is the child folder.
    EXPECT_TRUE(tree->handle_event(oui::KeyEvent{oui::Key::enter}));
    EXPECT_EQ(tree->current_path(), child);

    EXPECT_TRUE(tree->handle_event(oui::KeyEvent{oui::Key::backspace}));
    EXPECT_EQ(tree->current_path(), root);
    EXPECT_FALSE(tree->handle_event(oui::KeyEvent{oui::Key::backspace}));
    EXPECT_EQ(tree->current_path(), root);

    std::filesystem::remove_all(root, error);
}

TEST(OuiFolderViewTests, KeepsItsOwnKeysFromTheApplicationsShortcuts)
{
    const auto root{std::filesystem::temp_directory_path() / "olagarro_folder_view_claims_keys"};
    std::error_code error;
    std::filesystem::remove_all(root, error);
    ASSERT_TRUE(std::filesystem::create_directories(root / "child", error));

    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_ref{backend.get()};
    oui::Application application{std::move(backend)};
    auto column{oui::column()};
    auto& tree{column->add(oui::folder_view())};
    auto& other{column->add(oui::button("other"))};
    application.set_content(std::move(column));
    tree.set_scope(root);
    tree.show_folder(root);
    int copies{};
    int runs{};
    tree.on_copy_requested([&copies](const std::filesystem::path&) { ++ copies; });
    application.commands().register_command("run", "Run", [&runs] { ++ runs; });
    application.commands().bind("run", {oui::Key::f5});
    application.commands().register_command("run_for_real", "Run for real", [&runs] { runs += 10; });
    application.commands().bind("run_for_real", {oui::Key::f5, false, false, true});
    application.render();

    application.focus(tree);
    backend_ref->push_event(oui::KeyEvent{oui::Key::f5});
    application.run_frame();
    EXPECT_EQ(copies, 1) << "F5 is the panel's own: it copies";
    EXPECT_EQ(runs, 0);
    backend_ref->push_event(oui::KeyEvent{oui::Key::f5, false, false, true});
    application.run_frame();
    EXPECT_EQ(copies, 1) << "Shift+F5 is not the panel's";
    EXPECT_EQ(runs, 10);

    application.focus(other);
    backend_ref->push_event(oui::KeyEvent{oui::Key::f5});
    application.run_frame();
    EXPECT_EQ(copies, 1);
    EXPECT_EQ(runs, 11) << "away from the panel, F5 is the application's";

    std::filesystem::remove_all(root, error);
}

TEST(OuiTextEditTests, ClaimingTypingKeepsSingleKeyShortcutsFromTakingWhatIsTyped)
{
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_ref{backend.get()};
    oui::Application application{std::move(backend)};
    auto column{oui::column()};
    auto& editor{column->add(std::make_unique<oui::TextEdit>())};
    application.set_content(std::move(column));
    int swaps{};
    int erases{};
    int saves{};
    application.commands().register_command("swap", "Swap", [&swaps] { ++ swaps; });
    application.commands().bind("swap", {U'x'});
    application.commands().register_command("erase", "Erase", [&erases] { ++ erases; });
    application.commands().bind("erase", oui::KeyShortcut{oui::Key::delete_});
    application.commands().register_command("save", "Save", [&saves] { ++ saves; });
    application.commands().bind("save", {U's', true});
    application.render();
    application.focus(editor);
    editor.set_text("ab");

    backend_ref->push_event(oui::TextEvent{U'x'});
    backend_ref->push_event(oui::KeyEvent{oui::Key::delete_});
    application.run_frame();
    EXPECT_EQ(swaps, 1) << "without claiming, the shortcuts come first";
    EXPECT_EQ(erases, 1);
    EXPECT_EQ(editor.text(), "ab");

    editor.set_claims_typing(true);
    editor.select_range(0, 0);
    backend_ref->push_event(oui::TextEvent{U'x'});
    backend_ref->push_event(oui::KeyEvent{oui::Key::delete_});
    backend_ref->push_event(oui::TextEvent{U's', true});
    application.run_frame();
    EXPECT_EQ(swaps, 1);
    EXPECT_EQ(erases, 1);
    EXPECT_EQ(editor.text(), "xb") << "x typed, then the a after it deleted";
    EXPECT_EQ(saves, 1) << "Ctrl+S edits nothing, so it is still the application's";
}

TEST(OuiLineEditTests, AFieldWithTheFocusTakesWhatIsTypedAndItsEditingKeysBeforeTheShortcuts)
{
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_ref{backend.get()};
    oui::Application application{std::move(backend)};
    auto column{oui::column()};
    auto& field{oui::line_edit(*column)};
    auto& number{oui::number_field(*column)};
    number.set_range(0, 1000);
    auto& button{oui::button("Go", *column, [] {})};
    application.set_content(std::move(column));
    int swaps{};
    int erases{};
    int fills{};
    int saves{};
    application.commands().register_command("swap", "Swap", [&swaps] { ++ swaps; });
    application.commands().bind("swap", {U'x'});
    application.commands().register_command("erase", "Erase", [&erases] { ++ erases; });
    application.commands().bind("erase", oui::KeyShortcut{oui::Key::delete_});
    application.commands().register_command("fill", "Fill", [&fills] { ++ fills; });
    application.commands().bind("fill", oui::KeyShortcut{oui::Key::backspace});
    application.commands().register_command("save", "Save", [&saves] { ++ saves; });
    application.commands().bind("save", {U's', true});
    application.render();

    application.focus(field);
    field.set_text("abc");
    field.select_range(1, 1);
    backend_ref->push_event(oui::TextEvent{U'x'});
    backend_ref->push_event(oui::KeyEvent{oui::Key::delete_});
    backend_ref->push_event(oui::KeyEvent{oui::Key::backspace});
    backend_ref->push_event(oui::TextEvent{U's', true});
    application.run_frame();
    EXPECT_EQ(field.text(), "ac") << "x typed after the a, the b after it deleted, then the x before the cursor";
    EXPECT_EQ(swaps, 0);
    EXPECT_EQ(erases, 0);
    EXPECT_EQ(fills, 0);
    EXPECT_EQ(saves, 1) << "Ctrl+S edits nothing, so it is still the application's";

    // A number field's own field, as a size in a tool bar.
    application.focus(number.text_field());
    number.set_value(32);
    backend_ref->push_event(oui::KeyEvent{oui::Key::end});
    backend_ref->push_event(oui::KeyEvent{oui::Key::backspace});
    application.run_frame();
    EXPECT_EQ(number.text_field().text(), "3");
    EXPECT_EQ(fills, 0);

    // With nothing to delete there, Delete at its end and Backspace at its start are still the field's.
    backend_ref->push_event(oui::KeyEvent{oui::Key::delete_});
    backend_ref->push_event(oui::KeyEvent{oui::Key::home});
    backend_ref->push_event(oui::KeyEvent{oui::Key::backspace});
    application.run_frame();
    EXPECT_EQ(number.text_field().text(), "3");
    EXPECT_EQ(erases, 0);
    EXPECT_EQ(fills, 0);

    // A button has the focus: the keys are the application's.
    application.focus(button);
    backend_ref->push_event(oui::KeyEvent{oui::Key::backspace});
    application.run_frame();
    EXPECT_EQ(fills, 1);
}

TEST(OuiLineEditTests, EnterInAFieldNothingHearsPressesTheDefaultButton)
{
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_ref{backend.get()};
    oui::Application application{std::move(backend)};
    auto window{oui::window("Dialog")};
    auto& content{oui::column(*window)};
    auto& name{oui::line_edit(content)};
    auto& number{oui::number_field(content)};
    number.set_range(0, 100);
    auto& heard{oui::line_edit(content)};
    int clicks{};
    int submitted{};
    oui::button("OK", content, [&clicks] { ++ clicks; }).set_default(true);
    static_cast<void>(heard.on_submitted([&submitted](std::string_view) { ++ submitted; }));
    application.set_window(std::move(window));
    application.run_frame();

    application.focus(name);
    backend_ref->push_event(oui::KeyEvent{oui::Key::enter});
    application.run_frame();
    EXPECT_EQ(clicks, 1) << "nothing hears the field: the default button does";

    application.focus(number.text_field());
    number.text_field().set_text("42");
    backend_ref->push_event(oui::KeyEvent{oui::Key::enter});
    application.run_frame();
    EXPECT_EQ(number.value(), 42) << "what was typed taken";
    EXPECT_EQ(clicks, 2) << "and the default button pressed";

    application.focus(heard);
    backend_ref->push_event(oui::KeyEvent{oui::Key::enter});
    application.run_frame();
    EXPECT_EQ(submitted, 1);
    EXPECT_EQ(clicks, 2) << "a field that hears its Enter keeps it";
}

// The text a frame shows on row `y`.
[[nodiscard]] std::string frame_row(const oui::CellBuffer& frame, int y)
{
    std::string text;
    for(int x{}; x < frame.width(); ++ x)
    {
        text += oui::encode_utf8(frame.cell_at(x, y).glyph);
    }
    return text;
}

TEST(OuiTooltipTests, ShowWhenThePointerRestsAndGoWhenItMoves)
{
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_ref{backend.get()};
    oui::Application application{std::move(backend)};
    auto column{oui::column()};
    auto& told{column->add(oui::label("told"))};
    told.set_tooltip("a tip");
    column->add(oui::label("silent"));
    application.set_content(std::move(column));
    const auto rest_and_render{[&application]
                               {
                                   std::this_thread::sleep_for(std::chrono::milliseconds{700});
                                   application.run_frame();
                                   application.run_frame();
                               }};
    const auto shows_tip{[backend_ref]
                         {
                             const auto& frame{*backend_ref->last_frame()};
                             for(int y{}; y < frame.height(); ++ y)
                             {
                                 if(frame_row(frame, y).find("a tip") != std::string::npos)
                                 {
                                     return true;
                                 }
                             }
                             return false;
                         }};

    backend_ref->push_event(oui::MouseEvent{1, 0, false, 0, false, false, true});
    application.run_frame();
    EXPECT_FALSE(shows_tip()) << "not before the pointer has rested";
    EXPECT_FALSE(application.tooltip());
    rest_and_render();
    EXPECT_TRUE(shows_tip());
    EXPECT_EQ(application.tooltip(), "a tip");
    EXPECT_NE(frame_row(*backend_ref->last_frame(), 1).find("a tip"), std::string::npos) << "the line under it";

    backend_ref->push_event(oui::MouseEvent{2, 0, false, 0, false, false, true});
    application.run_frame();
    EXPECT_FALSE(shows_tip()) << "a move hides it";
    EXPECT_FALSE(application.tooltip());
    rest_and_render();
    ASSERT_TRUE(shows_tip());
    backend_ref->push_event(oui::KeyEvent{oui::Key::down});
    application.run_frame();
    EXPECT_FALSE(shows_tip()) << "a key hides it";

    backend_ref->push_event(oui::MouseEvent{1, 1, false, 0, false, false, true});
    rest_and_render();
    EXPECT_FALSE(shows_tip()) << "a widget with nothing to say shows nothing";
}

TEST(OuiTooltipTests, StartAfterThePointerOrEndBeforeItSoThePointerCoversNone)
{
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_ref{backend.get()};
    oui::Application application{std::move(backend)};
    // A row of twenty, the backend's width, that says something wherever the pointer rests on it.
    auto column{oui::column()};
    auto& told{column->add(oui::label("abcdefghijklmnopqrst"))};
    told.set_tooltip("a tip");
    application.set_content(std::move(column));
    const auto rest_at{[&](double x)
                       {
                           backend_ref->push_event(oui::MouseEvent{x, 0, false, 0, false, false, true});
                           application.run_frame();
                           std::this_thread::sleep_for(std::chrono::milliseconds{700});
                           application.run_frame();
                           application.run_frame();
                           return frame_row(*backend_ref->last_frame(), 1);
                       }};
    // Room after the pointer: it starts a column clear of the pointer's, as the arrow drawn in pixels reaches into the
    // next. " a tip " is seven columns.
    EXPECT_EQ(rest_at(2).substr(4, 7), " a tip ");
    // None: it ends in the column before.
    EXPECT_EQ(rest_at(17).substr(10, 7), " a tip ");

    // A big pointer, as a window says its system's is: it starts after all the columns the pointer covers.
    backend_ref->set_pointer_columns(5);
    EXPECT_EQ(rest_at(2).substr(7, 7), " a tip ");
    EXPECT_EQ(rest_at(2).substr(4, 3), "   ") << "nothing under the pointer";
    backend_ref->set_pointer_columns(2);

    // Wider than either side: on the wider, broken to fit it, still clear of the pointer.
    told.set_tooltip("one two three four");
    rest_at(5);
    const auto& frame{*backend_ref->last_frame()};
    for(int y{1}; y < frame.height(); ++ y)
    {
        EXPECT_EQ(frame_row(frame, y).substr(0, 6), "      ") << y << ": " << frame_row(frame, y);
    }
    EXPECT_NE(frame_row(frame, 1).find("one two"), std::string::npos) << frame_row(frame, 1);
}

TEST(OuiTabSwitcherTests, WrapsSelectionAtEdges)
{
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_ref{backend.get()};
    oui::Application application{std::move(backend)};
    application.set_content(oui::button("bg"));

    oui::TabSwitcher switcher;
    std::size_t confirmed_index{std::numeric_limits<std::size_t>::max()};
    switcher.on_confirmed([&confirmed_index](std::size_t index) { confirmed_index = index; });
    switcher.open(application, {"Tab A", "Tab B", "Tab C"}, 0);

    backend_ref->push_event(oui::KeyEvent{oui::Key::up});
    backend_ref->push_event(oui::KeyEvent{oui::Key::enter});
    application.run_frame();
    application.run_frame();

    EXPECT_EQ(confirmed_index, 2u);
}

TEST(OuiTabSwitcherTests, RestoresFocusAfterCancel)
{
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_ref{backend.get()};
    oui::Application application{std::move(backend)};
    auto bg{oui::button("bg")};
    auto* const bg_ref{bg.get()};
    application.set_content(std::move(bg));
    application.focus(*bg_ref);

    oui::TabSwitcher switcher;
    switcher.open(application, {"Tab A", "Tab B"}, 0);

    backend_ref->push_event(oui::KeyEvent{oui::Key::escape});
    application.run_frame();
    application.run_frame();

    EXPECT_EQ(application.focused(), bg_ref);
}

TEST(OuiTreeViewTests, PreservesScrollOffsetAfterClear)
{
    oui::TreeView tree;
    for(int i{}; i < 20; ++ i)
    {
        tree.add("Item " + std::to_string(i));
    }
    tree.arrange({0, 0, 16, 4});

    tree.handle_event(oui::MouseEvent{0, 0, false, -5});
    oui::CellBuffer buffer_before{16, 4};
    oui::CellBufferPainter painter_before{buffer_before};
    tree.paint(painter_before, {});
    const auto glyph_before{buffer_before.cell_at(0, 0).glyph};

    tree.clear();
    for(int i{}; i < 20; ++ i)
    {
        tree.add("Item " + std::to_string(i));
    }
    tree.arrange({0, 0, 16, 4});
    oui::CellBuffer buffer_after{16, 4};
    oui::CellBufferPainter painter_after{buffer_after};
    tree.paint(painter_after, {});

    EXPECT_EQ(buffer_after.cell_at(0, 0).glyph, glyph_before);
}

TEST(OuiLayoutTests, AnOpenComboBoxCoversTheWidgetsAfterItsContainer)
{
    // As in a dialog: the combo box sits in a grid row, and a column puts a label under that row.
    auto content{oui::column()};
    content->set_gap(oui::Spacing::none);
    auto& row{oui::grid(*content)};
    row.set_columns({oui::Length::content(), oui::Length::fill()});
    row.add(oui::label("Pick:"));
    auto& combo{oui::combo_box({"First", "Second", "Third"}, 0, row)};
    content->add(oui::label("Below the row"));
    content->add(oui::label("Further down"));
    content->arrange({0, 0, 20, 3});
    ASSERT_TRUE(combo.handle_event(oui::KeyEvent{oui::Key::enter}));

    oui::CellBuffer buffer{20, 3};
    oui::CellBufferPainter painter{buffer};
    content->paint(painter, {});
    const auto x{oui::whole(combo.bounds().x)};
    EXPECT_EQ(buffer.cell_at(x + 1, 1).glyph, U'S');
    EXPECT_EQ(buffer.cell_at(x + 1, 2).glyph, U'T');
    EXPECT_EQ(content->hit_test(x + 1, 1), &combo);
    EXPECT_EQ(content->hit_test(x + 1, 2), &combo);
}

TEST(OuiLayoutTests, AClosedComboBoxPaintsUnderWhatComesAfterIt)
{
    // A menu that opens over a closed combo box must cover it, as anything later in the tree does.
    auto content{oui::column()};
    content->set_gap(oui::Spacing::none);
    auto& combo{oui::combo_box({"First", "Second"}, 0, *content)};
    content->arrange({0, 0, 20, 1});
    EXPECT_FALSE(combo.is_overlay());
    ASSERT_TRUE(combo.handle_event(oui::KeyEvent{oui::Key::enter}));
    EXPECT_TRUE(combo.is_overlay());
}

TEST(OuiLayoutTests, AnOpenComboBoxMarksTheOptionUnderThePointer)
{
    auto content{oui::column()};
    auto& combo{oui::combo_box({"First", "Second", "Third"}, 0, *content)};
    content->arrange({0, 0, 20, 3});
    ASSERT_TRUE(combo.handle_event(oui::KeyEvent{oui::Key::enter}));
    oui::MouseEvent hover{combo.bounds().x + 1, 2, false};
    hover.hover = true;
    ASSERT_TRUE(combo.handle_event(hover));

    const oui::Theme theme{};
    oui::CellBuffer buffer{20, 3};
    oui::CellBufferPainter painter{buffer};
    content->paint(painter, theme);
    const auto x{oui::whole(combo.bounds().x) + 1};
    EXPECT_EQ(buffer.cell_at(x, 2).background, theme.item_selected_background);
    EXPECT_EQ(buffer.cell_at(x, 0).background, theme.table_background);
    // Nothing is chosen until it is picked, and Enter picks it.
    EXPECT_EQ(combo.selected(), 0u);
    ASSERT_TRUE(combo.handle_event(oui::KeyEvent{oui::Key::enter}));
    EXPECT_EQ(combo.selected(), 2u);
}

TEST(OuiWindowTests, AHeightShareSizesTheWindowFromTheScreenNotItsContent)
{
    auto dialog{oui::window("Palette")};
    auto content{oui::column()};
    for(int line{}; line < 100; ++ line)
    {
        content->add(oui::label("command"));
    }
    dialog->add(std::move(content));
    dialog->set_height_share(0.8);
    // The share of the screen, whatever the content measures, and never under the minimum.
    EXPECT_EQ(dialog->measure({80, 40}).height, 32);
    EXPECT_EQ(dialog->measure({80, 25}).height, 20);
    dialog->set_minimum_size({10, 22});
    EXPECT_EQ(dialog->measure({80, 25}).height, 22);
}

TEST(OuiWidgetTests, SpaceTicksABoxAndEnterIsLeftToTheDialogsDefaultButton)
{
    // As Turbo Vision's: Enter in a dialog presses its default button, whatever has the focus, a box too.
    oui::CheckButton box{"&Wrap"};
    EXPECT_FALSE(box.handle_event(oui::KeyEvent{oui::Key::enter}));
    EXPECT_FALSE(box.is_checked());
    EXPECT_TRUE(box.handle_event(oui::KeyEvent{oui::Key::space}));
    EXPECT_TRUE(box.is_checked());
}

TEST(OuiWidgetTests, TheDefaultButtonShowsInItsColourAndTakesEnterFromTheDialog)
{
    const auto theme{oui::TurboCTheme::make()};
    oui::Window dialog{"Ask"};
    auto& content{oui::column(dialog)};
    auto& field{oui::line_edit(content)};
    auto clicked{0};
    auto& ok{oui::button("OK", content, [&clicked] { ++ clicked; })};
    ok.set_default(true);
    auto& cancel{oui::button("Cancel", content)};
    dialog.arrange({0, 0, 30, 10});
    oui::CellBuffer buffer{30, 10};
    oui::CellBufferPainter painter{buffer};
    dialog.paint(painter, theme);

    // As Turbo Vision: the default button's text light cyan, another's black, both on green.
    const auto letter{[&buffer](const oui::Button& button, char32_t glyph)
                      {
                          const auto y{oui::whole(button.bounds().y)};
                          for(int x{oui::whole(button.bounds().x)};
                              x < oui::whole(button.bounds().x + button.bounds().width); ++ x)
                          {
                              if(buffer.cell_at(x, y).glyph == glyph)
                              {
                                  return buffer.cell_at(x, y);
                              }
                          }
                          return oui::Cell{};
                      }};
    EXPECT_EQ(letter(ok, U'O').foreground, theme.button_default_foreground);
    EXPECT_EQ(letter(cancel, U'C').foreground, theme.button_foreground);

    // Enter that the field with the focus leaves goes to it, wherever the focus is.
    static_cast<void>(field);
    EXPECT_TRUE(dialog.handle_event(oui::KeyEvent{oui::Key::enter}));
    EXPECT_EQ(clicked, 1);
}

TEST(OuiWidgetTests, AltAndAShortcutLetterInADialogGoesToWhatALabelNamesOrPressesAButton)
{
    auto backend{oui::scripted_backend({}, {40, 12})};
    auto* const screen{backend.get()};
    const auto theme{oui::TurboCTheme::make()};
    oui::Application application{std::move(backend), theme};
    application.set_content(oui::label("behind"));
    auto dialog{oui::window("Ask")};
    auto& dialog_reference{*dialog};
    auto& content{oui::column(*dialog)};
    auto& name_label{oui::label("&Name:", content)};
    auto& name{oui::line_edit(content)};
    name_label.set_buddy(name);
    auto& other{oui::line_edit(content)};
    auto saved{0};
    auto& save{oui::button("&Save", content,
                           [&saved, &dialog_reference]
                           {
                               ++ saved;
                               dialog_reference.close(oui::DialogResult::accepted);
                           })};
    // A label shows its letter apart only when it names something.
    auto& plain{oui::label("Fish & chips", content)};
    auto name_focused{false};
    std::optional<oui::Cell> letter;
    std::u32string shown;
    std::u32string plain_shown;
    application.post(
        [&]
        {
            application.focus(other);
            application.render();
            const auto& frame{*screen->last_frame()};
            const auto name_x{oui::whole(name_label.bounds().x)};
            const auto name_y{oui::whole(name_label.bounds().y)};
            letter = frame.cell_at(name_x, name_y);
            for(int x{name_x}; x < name_x + 5; ++ x)
            {
                shown += frame.cell_at(x, name_y).glyph;
            }
            for(int x{oui::whole(plain.bounds().x)}; x < oui::whole(plain.bounds().x) + 12; ++ x)
            {
                plain_shown += frame.cell_at(x, oui::whole(plain.bounds().y)).glyph;
            }
            application.dispatch_event(oui::TextEvent{U'n', false, true});
            name_focused = name.is_focused();
            application.dispatch_event(oui::TextEvent{U'S', false, true, true});
            // So a failure ends here rather than waits for ever.
            if(saved == 0)
            {
                dialog_reference.close(oui::DialogResult::cancelled);
            }
        });
    static_cast<void>(application.show_modal(std::move(dialog)));
    static_cast<void>(save);

    EXPECT_EQ(shown, U"Name:");
    ASSERT_TRUE(letter.has_value());
    EXPECT_EQ(letter->glyph, U'N');
    EXPECT_EQ(letter->foreground, theme.label_shortcut_foreground);
    EXPECT_EQ(plain_shown, U"Fish & chips");
    EXPECT_TRUE(name_focused) << "Alt+N goes to the field the label names";
    EXPECT_EQ(saved, 1) << "Alt+S presses the button, whatever the case";
}

TEST(OuiWidgetTests, AListKeepsItsArrowsAtItsEndsAsTurboVisionsDo)
{
    oui::Table table;
    table.add_line({"first"});
    table.add_line({"last"});
    table.arrange({0, 0, 10, 4});
    table.select_row(1);
    EXPECT_TRUE(table.handle_event(oui::KeyEvent{oui::Key::down})) << "not to the next field";
    EXPECT_EQ(table.selected_row(), std::optional<std::size_t>{1});
    table.select_row(0);
    EXPECT_TRUE(table.handle_event(oui::KeyEvent{oui::Key::up})) << "not to the field before";
    EXPECT_EQ(table.selected_row(), std::optional<std::size_t>{0});

    oui::TreeView tree;
    tree.add("alpha");
    tree.add("beta");
    tree.arrange({0, 0, 10, 4});
    EXPECT_TRUE(tree.handle_event(oui::KeyEvent{oui::Key::up}));
    EXPECT_TRUE(tree.handle_event(oui::KeyEvent{oui::Key::down}));
    EXPECT_TRUE(tree.handle_event(oui::KeyEvent{oui::Key::down}));
}

TEST(OuiWidgetTests, InADialogTheLetterAloneWorksUnlessAFieldTakesIt)
{
    auto backend{oui::scripted_backend({}, {40, 12})};
    auto* const screen{backend.get()};
    const auto theme{oui::TurboCTheme::make()};
    oui::Application application{std::move(backend), theme};
    application.set_content(oui::label("behind"));
    auto dialog{oui::window("Ask")};
    auto& dialog_reference{*dialog};
    auto& content{oui::column(*dialog)};
    auto& wrap{oui::check_button("&Wrap", content)};
    auto& field{oui::line_edit(content)};
    auto& other{oui::button("Other", content)};
    auto saved{0};
    oui::button("&Save", content,
                [&saved, &dialog_reference]
                {
                    ++ saved;
                    dialog_reference.close(oui::DialogResult::accepted);
                });
    std::optional<oui::Cell> letter;
    auto checked_by_letter{false};
    auto wrap_focused{false};
    auto saved_while_typing{-1};
    // Read while the dialog is there: it goes when it closes.
    std::string typed;
    application.post(
        [&]
        {
            application.render();
            letter = screen->last_frame()->cell_at(oui::whole(wrap.bounds().x) + 4, oui::whole(wrap.bounds().y));
            application.focus(other);
            application.dispatch_event(oui::TextEvent{U'w'});
            checked_by_letter = wrap.is_checked();
            wrap_focused = wrap.is_focused();
            application.focus(field);
            application.dispatch_event(oui::TextEvent{U's'});
            saved_while_typing = saved;
            typed = field.text();
            application.dispatch_event(oui::TextEvent{U's', false, true});
            if(saved == 0)
            {
                dialog_reference.close(oui::DialogResult::cancelled);
            }
        });
    static_cast<void>(application.show_modal(std::move(dialog)));

    ASSERT_TRUE(letter.has_value());
    EXPECT_EQ(letter->glyph, U'W');
    EXPECT_EQ(letter->foreground, theme.cluster_shortcut_foreground);
    EXPECT_TRUE(checked_by_letter) << "W alone turns the check box, from a button";
    EXPECT_TRUE(wrap_focused);
    EXPECT_EQ(saved_while_typing, 0) << "in a field, a letter is typed";
    EXPECT_EQ(typed, "s");
    EXPECT_EQ(saved, 1) << "Alt+S presses the button from the field";
}

TEST(OuiWidgetTests, AWidgetRefLetsGoOfAWidgetThatIsGoneThoughAnotherIsMadeWhereItWas)
{
    std::optional<oui::TextEdit> slot;
    slot.emplace();
    const oui::WidgetRef<oui::TextEdit> held{&*slot};
    EXPECT_EQ(held.get(), &*slot);
    const auto* const where{&*slot};
    slot.reset();
    EXPECT_EQ(held.get(), nullptr);
    slot.emplace();
    ASSERT_EQ(&*slot, where) << "the new one is where the old one was";
    EXPECT_EQ(held.get(), nullptr) << "and is not taken for it";
    EXPECT_FALSE(held);
    EXPECT_FALSE(held.refers_to(&*slot));
}

TEST(OuiWidgetTests, AltAndAShortcutLetterWorkInTheMainWindowTooWhenNothingElseTakesThem)
{
    oui::Application application{oui::scripted_backend({}, {40, 12})};
    auto content{oui::column()};
    auto& name_label{oui::label("&Name:", *content)};
    auto& name{oui::line_edit(*content)};
    name_label.set_buddy(name);
    auto& other{oui::line_edit(*content)};
    auto saved{0};
    static_cast<void>(oui::button("&Save", *content, [&saved] { ++ saved; }));
    application.set_content(std::move(content));
    application.focus(other);

    application.dispatch_event(oui::TextEvent{U'n', false, true});
    EXPECT_TRUE(name.is_focused()) << "Alt+N goes to the field the label names";
    application.dispatch_event(oui::TextEvent{U's', false, true});
    EXPECT_EQ(saved, 1) << "Alt+S presses the button";
    // Outside a dialog a letter alone is typed, not a shortcut.
    application.focus(other);
    application.dispatch_event(oui::TextEvent{U's'});
    EXPECT_EQ(saved, 1);
    EXPECT_EQ(other.text(), "s");
}

TEST(OuiWidgetTests, ThePointerSaysALineCanBeDraggedWhileOverItOrDraggingIt)
{
    auto backend{oui::scripted_backend({}, {40, 12})};
    auto* const screen{backend.get()};
    oui::Application application{std::move(backend)};
    auto rows{oui::split_view(oui::SplitView::Orientation::vertical)};
    rows->add(oui::label("top"));
    rows->add(oui::label("bottom"));
    auto columns{oui::split_view()};
    columns->add(oui::label("left"));
    columns->add(std::move(rows));
    auto& columns_reference{*columns};
    application.set_content(std::move(columns));
    application.render();
    // The line is the column after the first side.
    const auto& first{columns_reference.child_at(0).bounds()};
    const auto across{first.x + first.width};
    const auto hover{[&application](double x, double y)
                     { application.dispatch_event(oui::MouseEvent{x, y, false, 0, false, false, true}); }};
    using oui::Pointer;

    hover(across, 3);
    EXPECT_EQ(screen->pointer(), Pointer::resize_columns) << "over the line between side and side";
    hover(across - 2, 3);
    EXPECT_EQ(screen->pointer(), Pointer::arrow);
    hover(across + 3, 6);
    EXPECT_EQ(screen->pointer(), Pointer::resize_rows) << "over the line between top and bottom";
    // Dragged, the line is under the pointer only now and then; the pointer says it is being dragged all the while.
    application.dispatch_event(oui::MouseEvent{across, 3, true});
    application.dispatch_event(oui::MouseEvent{across - 5, 3, false, 0, true});
    EXPECT_EQ(screen->pointer(), Pointer::resize_columns);
    application.dispatch_event(oui::MouseEvent{across - 5, 3, false, 0, false, true});
    hover(1, 1);
    EXPECT_EQ(screen->pointer(), Pointer::arrow);
}

TEST(OuiWidgetTests, ThePointerSaysADockedPanelsEdgeCanBeDragged)
{
    auto backend{oui::scripted_backend({}, {40, 12})};
    auto* const screen{backend.get()};
    oui::Application application{std::move(backend)};
    auto workspace{oui::dock_layout()};
    auto& panel{workspace->add(oui::Dock::left, oui::label("panel"), 10)};
    workspace->set_resizable(panel);
    workspace->add(oui::Dock::fill, oui::label("documents"));
    application.set_content(std::move(workspace));
    application.render();
    // The edge is the panel's last column.
    application.dispatch_event(oui::MouseEvent{9, 4, false, 0, false, false, true});
    EXPECT_EQ(screen->pointer(), oui::Pointer::resize_columns);
    application.dispatch_event(oui::MouseEvent{20, 4, false, 0, false, false, true});
    EXPECT_EQ(screen->pointer(), oui::Pointer::arrow);
}

namespace
{

// A widget whose pointer changes when it is told, as a program on the desktop answers after the pointer moved.
class ChangingPointer final : public oui::Widget
{
  public:
    [[nodiscard]] oui::Size measure(const oui::Constraints& constraints) const override
    {
        return constraints.constrain({constraints.max_width, constraints.max_height});
    }
    [[nodiscard]] oui::Pointer pointer_at(oui::Point) const override { return wanted; }

    oui::Pointer wanted{oui::Pointer::arrow};
};

} // namespace

TEST(OuiWidgetTests, ThePointerIsAskedAgainWhenWhatIsUnderItChanges)
{
    auto backend{oui::scripted_backend({}, {20, 5})};
    auto* const screen{backend.get()};
    oui::Application application{std::move(backend)};
    auto widget{std::make_unique<ChangingPointer>()};
    auto& changing{*widget};
    application.set_content(std::move(widget));
    application.render();
    application.dispatch_event(oui::MouseEvent{4, 2, false, 0, false, false, true});
    changing.wanted = oui::Pointer::resize_rows;
    EXPECT_EQ(screen->pointer(), oui::Pointer::arrow);
    application.update_pointer();
    EXPECT_EQ(screen->pointer(), oui::Pointer::resize_rows);
}

namespace
{

// A widget that tells its parts by where it heard the pointer last, as one drawing its own pixels does: the left half
// pulls up to the right, the right half down to the right.
class HeardPointer final : public oui::Widget
{
  public:
    [[nodiscard]] oui::Size measure(const oui::Constraints& constraints) const override
    {
        return constraints.constrain({constraints.max_width, constraints.max_height});
    }
    bool handle_event(const oui::Event& event) override
    {
        if(const auto* const mouse{std::get_if<oui::MouseEvent>(&event)})
        {
            m_wanted = mouse->x >= 10 ? oui::Pointer::resize_down_right : oui::Pointer::resize_up_right;
            return true;
        }
        return false;
    }
    [[nodiscard]] oui::Pointer pointer_at(oui::Point) const override { return m_wanted; }

  private:
    oui::Pointer m_wanted{oui::Pointer::arrow};
};

} // namespace

TEST(OuiWidgetTests, ThePointerIsAskedOnceTheWidgetUnderItHeardWhereItIs)
{
    auto backend{oui::scripted_backend({}, {20, 5})};
    auto* const screen{backend.get()};
    oui::Application application{std::move(backend)};
    application.set_content(std::make_unique<HeardPointer>());
    application.render();
    application.dispatch_event(oui::MouseEvent{12, 2, false, 0, false, false, true});
    EXPECT_EQ(screen->pointer(), oui::Pointer::resize_down_right);
    application.dispatch_event(oui::MouseEvent{3, 2, false, 0, false, false, true});
    EXPECT_EQ(screen->pointer(), oui::Pointer::resize_up_right);
}

TEST(OuiApplicationTests, ItTellsWhetherThePointerIsOverAWidget)
{
    oui::Application application{oui::scripted_backend({}, {20, 5})};
    auto grid{oui::grid()};
    grid->set_columns({oui::Length::fill(), oui::Length::fill()});
    auto& left{grid->add(std::make_unique<ChangingPointer>())};
    auto& right{grid->add(std::make_unique<ChangingPointer>())};
    application.set_content(std::move(grid));
    application.render();
    EXPECT_FALSE(application.is_under_pointer(left)) << "before the pointer was heard";
    application.dispatch_event(oui::MouseEvent{2, 2, false, 0, false, false, true});
    EXPECT_TRUE(application.is_under_pointer(left));
    EXPECT_FALSE(application.is_under_pointer(right));
    application.dispatch_event(oui::MouseEvent{17, 2, false, 0, false, false, true});
    EXPECT_FALSE(application.is_under_pointer(left));
    EXPECT_TRUE(application.is_under_pointer(right));
}

TEST(OuiApplicationTests, TheFocusGoesToWhatIsNearestOnTheSideAnArrowSays)
{
    oui::Application application{oui::scripted_backend({}, {40, 10})};
    // Four fields, two a row: from each, an arrow goes to the one beside it on that side.
    auto grid{oui::grid()};
    grid->set_columns({oui::Length::fill(), oui::Length::fill()});
    grid->set_gap(oui::Spacing::tight);
    auto& top_left{grid->add(oui::line_edit())};
    auto& top_right{grid->add(oui::line_edit())};
    auto& bottom_left{grid->add(oui::line_edit())};
    auto& bottom_right{grid->add(oui::line_edit())};
    application.set_content(std::move(grid));
    application.render();
    application.focus(top_left);
    EXPECT_TRUE(application.focus_toward(1, 0));
    EXPECT_TRUE(top_right.is_focused());
    EXPECT_TRUE(application.focus_toward(0, 1));
    EXPECT_TRUE(bottom_right.is_focused());
    EXPECT_TRUE(application.focus_toward(-1, 0));
    EXPECT_TRUE(bottom_left.is_focused());
    EXPECT_TRUE(application.focus_toward(0, -1));
    EXPECT_TRUE(top_left.is_focused());
    // Nothing further on that side: the focus stays.
    EXPECT_FALSE(application.focus_toward(-1, 0));
    EXPECT_TRUE(top_left.is_focused());
}

namespace
{

// Takes every key, as a terminal does for the program in it.
class TakesEveryKey final : public oui::Widget
{
  public:
    [[nodiscard]] oui::Size measure(const oui::Constraints& constraints) const override
    {
        return constraints.constrain({constraints.max_width, constraints.max_height});
    }
    [[nodiscard]] bool is_focusable() const override { return true; }
    [[nodiscard]] bool claims_key(const oui::Event&) const override { return true; }
    bool handle_event(const oui::Event&) override
    {
        ++ taken;
        return true;
    }

    int taken{};
};

} // namespace

TEST(OuiApplicationTests, AReservedCommandsKeysAreItsOwnOverAWidgetThatTakesEveryKey)
{
    oui::Application application{oui::scripted_backend({}, {20, 5})};
    auto widget{std::make_unique<TakesEveryKey>()};
    auto& taking{*widget};
    application.set_content(std::move(widget));
    application.focus(taking);
    auto moved{0};
    auto other{0};
    application.commands().register_command("move", "Move", [&moved] { ++ moved; });
    application.commands().bind("move", oui::Shortcut{U'k', false, true});
    application.commands().reserve("move");
    application.commands().register_command("other", "Other", [&other] { ++ other; });
    application.commands().bind("other", oui::Shortcut{U'j', false, true});

    application.dispatch_event(oui::TextEvent{U'k', false, true});
    EXPECT_EQ(moved, 1) << "reserved: the application's, wherever the focus is";
    application.dispatch_event(oui::TextEvent{U'j', false, true});
    EXPECT_EQ(other, 0) << "not reserved: the widget's, which takes every key";
    EXPECT_EQ(taking.taken, 1);
}
