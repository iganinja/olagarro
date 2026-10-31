#include <oui/application.h>
#include <oui/button.h>
#include <oui/check_button.h>
#include <oui/combo_box.h>
#include <oui/event.h>
#include <oui/file_system.h>
#include <oui/graphics/canvas.h>
#include <oui/graphics/font.h>
#include <oui/graphics/graphical_painter.h>
#include <oui/graphics/icon.h>
#include <oui/graphics/pixel_backend.h>
#include <oui/graphics/svg.h>
#include <oui/grid.h>
#include <oui/label.h>
#include <oui/layout.h>
#include <oui/popup_list.h>
#include <oui/split_view.h>
#include <oui/text_layout.h>
#include <oui/text_metrics.h>
#include <oui/scripted_backend.h>
#include <oui/window.h>

#include "oui/src/graphics/wheel.h"

#include <vector>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>
#include <tuple>

namespace
{

using namespace olagarro;
using namespace olagarro::oui;

[[nodiscard]] bool same(Color first, Color second)
{
    return first == second;
}

TEST(OuiGraphicsTests, FillsOnlyInsideTheClip)
{
    Canvas canvas{10, 10};
    canvas.set_clip({2, 2, 4, 4});
    canvas.fill({0, 0, 10, 10}, {255, 0, 0});
    EXPECT_TRUE(same(canvas.pixel_at(2, 2), {255, 0, 0}));
    EXPECT_TRUE(same(canvas.pixel_at(5, 5), {255, 0, 0}));
    EXPECT_TRUE(same(canvas.pixel_at(1, 2), {0, 0, 0}));
    EXPECT_TRUE(same(canvas.pixel_at(6, 6), {0, 0, 0}));
}

TEST(OuiGraphicsTests, ShadesThePixelsAPolygonEdgeCrosses)
{
    Canvas canvas{10, 10};
    // A square whose left edge runs through the middle of column 2.
    canvas.fill_polygon({{2.5f, 0}, {8, 0}, {8, 10}, {2.5f, 10}}, {255, 255, 255});
    EXPECT_EQ(canvas.pixel_at(1, 5).red, 0);
    EXPECT_GT(canvas.pixel_at(2, 5).red, 64);
    EXPECT_LT(canvas.pixel_at(2, 5).red, 192);
    EXPECT_EQ(canvas.pixel_at(4, 5).red, 255);
}

TEST(OuiGraphicsTests, BuildsInAFixedCellFontAndAProportionalOne)
{
    auto cell{default_cell_font()};
    EXPECT_EQ(cell->line_height(), 24);
    EXPECT_EQ(cell->measure("iii"), cell->measure("MMM"));
    EXPECT_TRUE(cell->has_glyph(U'ñ'));
    EXPECT_TRUE(cell->has_glyph(U'┼'));

    auto interface{default_interface_font()};
    EXPECT_LT(interface->measure("iii"), interface->measure("MMM"));
    EXPECT_TRUE(interface->has_glyph(U'ñ'));
    EXPECT_FALSE(interface->has_glyph(U'↑'));
}

TEST(OuiGraphicsTests, DrawsATabInInterfaceTextAsASpace)
{
    auto cell{default_cell_font()};
    auto interface{default_interface_font()};
    auto symbols{symbol_font(12, 24, cell->ascent())};
    const FontSet fonts{*cell, *interface, *symbols};
    EXPECT_EQ(fonts.interface_width("a\tb"), fonts.interface_width("a b"));
}

TEST(OuiGraphicsTests, DrawsTheArrowsTheFontsLack)
{
    auto symbols{symbol_font(12, 24, 19)};
    EXPECT_TRUE(symbols->has_glyph(U'↑'));
    EXPECT_FALSE(symbols->has_glyph(U'a'));
    const auto& arrow{symbols->glyph(U'↑')};
    int inked{};
    for(const auto value : arrow.coverage)
    {
        inked += value > 128 ? 1 : 0;
    }
    EXPECT_GT(inked, 10);
}

TEST(OuiGraphicsTests, PaintsCellsInPixelsOfTheCellFont)
{
    auto cell{default_cell_font()};
    auto interface{default_interface_font()};
    auto symbols{symbol_font(12, 24, cell->ascent())};
    Canvas canvas{120, 72};
    const auto style{classic_style()};
    GraphicalPainter painter{canvas, style, *cell, *interface, *symbols};
    painter.draw_text({2, 1}, "  ", {{255, 255, 255}, {10, 20, 30}});
    // Two cells of 12 by 24 from column 2 of row 1.
    EXPECT_TRUE(same(canvas.pixel_at(24, 24), {10, 20, 30}));
    EXPECT_TRUE(same(canvas.pixel_at(47, 47), {10, 20, 30}));
    EXPECT_FALSE(same(canvas.pixel_at(48, 24), {10, 20, 30}));
    EXPECT_FALSE(same(canvas.pixel_at(23, 24), {10, 20, 30}));
}

TEST(OuiGraphicsTests, RowsAreAllOneHeightAndWhatEndsAtTheBottomEndsAtItsLastPixel)
{
    // 80 pixels are three rows of 24 and a third of one: what reaches the bottom, as a window's status line, ends at
    // its last pixel, with no strip under it, and every row above is 24 tall.
    auto cell{default_cell_font()};
    auto interface{default_interface_font()};
    auto symbols{symbol_font(12, 24, cell->ascent())};
    Canvas canvas{120, 80};
    canvas.fill({0, 0, 120, 80}, {1, 2, 3});
    const auto style{classic_style()};
    GraphicalPainter painter{canvas, style, *cell, *interface, *symbols};
    painter.fill_rect({0, 0, 10, 2}, {200, 0, 0});
    painter.fill_rect({0, 2, 10, 80.0 / 24 - 2}, {0, 0, 200});
    for(int y{}; y < 80; ++ y)
    {
        EXPECT_FALSE(same(canvas.pixel_at(5, y), {1, 2, 3})) << "a strip nothing paints at " << y;
    }
    EXPECT_TRUE(same(canvas.pixel_at(5, 79), {0, 0, 200}));
    EXPECT_TRUE(same(canvas.pixel_at(5, 47), {200, 0, 0}));
    EXPECT_TRUE(same(canvas.pixel_at(5, 48), {0, 0, 200}));
}

TEST(OuiGraphicsTests, CellsAreInPixelsWherePainted)
{
    // Three rows of 24 in 80 pixels, so the rows are not all 24 tall: the cells' pixels are where they are painted.
    auto cell{default_cell_font()};
    auto interface{default_interface_font()};
    auto symbols{symbol_font(12, 24, cell->ascent())};
    Canvas canvas{120, 80};
    canvas.fill({0, 0, 120, 80}, {1, 2, 3});
    const auto style{classic_style()};
    GraphicalPainter painter{canvas, style, *cell, *interface, *symbols};
    const oui::Rect cells{2, 1, 3, 2};
    painter.fill_rect(cells, {200, 0, 0});
    const auto area{painter.pixel_bounds(cells)};
    ASSERT_TRUE(area.has_value());
    for(int y{}; y < 80; ++ y)
    {
        for(int x{}; x < 120; ++ x)
        {
            const auto inside{x >= area->x && x < area->x + area->width && y >= area->y && y < area->y + area->height};
            EXPECT_EQ(same(canvas.pixel_at(x, y), {200, 0, 0}), inside) << x << ", " << y;
        }
    }
}

TEST(OuiGraphicsTests, DrawsAnSvgPictureAtTheSizeAsked)
{
    // Red on the left half, nothing on the right, in a 16 by 16 view drawn at 32 by 32.
    const auto picture{draw_svg(R"(<svg height="16" viewBox="0 0 16 16" width="16" xmlns="http://www.w3.org/2000/svg">)"
                                R"(<path d="M0 0h8v16H0z" fill="#ff0000"/></svg>)",
                                32, 32)};
    ASSERT_TRUE(picture.has_value());
    ASSERT_EQ(picture->width, 32);
    ASSERT_EQ(picture->height, 32);
    EXPECT_EQ(picture->pixels.at(5 * 32 + 3), 0xffff0000u);
    EXPECT_EQ(picture->pixels.at(5 * 32 + 28) >> 24, 0u) << "the right half is see-through";
    EXPECT_FALSE(draw_svg("not a picture", 16, 16).has_value());
}

TEST(OuiGraphicsTests, AnImageWithAlphaIsMixedOverWhatIsUnderIt)
{
    auto cell{default_cell_font()};
    auto interface{default_interface_font()};
    auto symbols{symbol_font(12, 24, cell->ascent())};
    Canvas canvas{24, 24};
    canvas.fill({0, 0, 24, 24}, {0, 0, 255});
    const auto style{classic_style()};
    GraphicalPainter painter{canvas, style, *cell, *interface, *symbols};
    // Solid red, half red, and nothing.
    const std::vector<std::uint32_t> pixels{0xffff0000u, 0x80ff0000u, 0x00ff0000u};
    PixelImage image{pixels.data(), 3, 1, 3};
    image.alpha = true;
    painter.draw_image({0, 0, 2, 1}, image, Theme{});
    EXPECT_TRUE(same(canvas.pixel_at(0, 0), {255, 0, 0}));
    const auto half{canvas.pixel_at(1, 0)};
    EXPECT_NEAR(half.red, 128, 2);
    EXPECT_NEAR(half.blue, 127, 2);
    EXPECT_TRUE(same(canvas.pixel_at(2, 0), {0, 0, 255}));
}

TEST(OuiGraphicsTests, APixelIsInTheCellThatIsPaintedThere)
{
    auto cell{default_cell_font()};
    auto interface{default_interface_font()};
    auto symbols{symbol_font(12, 24, cell->ascent())};
    const auto style{classic_style()};
    for(const auto height : {72, 80, 95, 1000})
    {
        auto backend{image_backend({36, static_cast<double>(height)})};
        Canvas canvas{36, height};
        GraphicalPainter painter{canvas, style, *cell, *interface, *symbols};
        for(int y{}; y < height; ++ y)
        {
            for(int x{}; x < 36; ++ x)
            {
                const auto at{backend->cell_at({static_cast<double>(x), static_cast<double>(y)})};
                const auto area{
                    painter.pixel_bounds({static_cast<double>(whole(at.x)), static_cast<double>(whole(at.y)), 1, 1})};
                ASSERT_TRUE(area.has_value());
                EXPECT_TRUE(x >= area->x && x < area->x + area->width && y >= area->y && y < area->y + area->height)
                    << x << ", " << y << " of " << height;
            }
        }
    }
}

TEST(OuiGraphicsTests, APointerIsWhereItsPixelIsBetweenCells)
{
    // Cells of 12 by 24: pixel 125 across is 125 / 12 columns in, not the column it falls in.
    auto backend{image_backend({480, 240})};
    ASSERT_EQ(backend->cell_size().width, 12);
    const auto at{backend->cell_at({125, 30})};
    EXPECT_DOUBLE_EQ(at.x, 125.0 / 12);
    EXPECT_DOUBLE_EQ(at.y, 30.0 / 24);
}

// The cells' pixels as GraphicalPainter paints them, each edge rounded on its own.
[[nodiscard]] long pixel_of(double cells, double cell_pixels)
{
    return std::lround(cells * cell_pixels);
}

TEST(OuiGraphicsTests, AWindowIsLaidOutInWholeCellsOfItsPixels)
{
    auto backend{image_backend({1000, 700})};
    auto& image{*backend};
    Application application{std::move(backend)};
    // Its size is every pixel it has, in cells.
    EXPECT_DOUBLE_EQ(image.size().width, 1000.0 / 12);
    EXPECT_DOUBLE_EQ(image.size().height, 700.0 / 24);
    EXPECT_DOUBLE_EQ(text_metrics().column_step(), 1.0 / 12);
    EXPECT_DOUBLE_EQ(text_metrics().row_step(), 1.0 / 24);
    ASSERT_TRUE(text_metrics().cell_pixels().has_value());
    EXPECT_DOUBLE_EQ(text_metrics().cell_pixels()->width, 12);
}

TEST(OuiGraphicsTests, ASplitViewPutsItsDividerWithinAPixelOfItsRatio)
{
    auto backend{image_backend({1000, 700})};
    Application application{std::move(backend)};
    auto split{split_view(SplitView::Orientation::horizontal)};
    auto& left{split->add(label("Left"))};
    auto& right{split->add(label("Right"))};
    split->set_ratio(0.37);
    application.set_content(std::move(split));
    application.run_frame();
    // The two sides share the width but for the divider's column, 12 pixels, which starts where the first ends.
    const auto first{pixel_of(left.bounds().x + left.bounds().width, 12)};
    EXPECT_NEAR(static_cast<double>(first), 0.37 * (1000 - 12), 1.0);
    EXPECT_EQ(pixel_of(right.bounds().x, 12), first + 12);
    EXPECT_EQ(pixel_of(right.bounds().x + right.bounds().width, 12), 1000);
}

TEST(OuiGraphicsTests, FlexibleColumnsShareAWindowByThePixel)
{
    auto backend{image_backend({1000, 700})};
    Application application{std::move(backend)};
    auto columns{grid()};
    columns->set_columns({Length::flex(), Length::flex(), Length::flex()});
    std::vector<Label*> children;
    for(const auto* const text : {"One", "Two", "Three"})
    {
        children.push_back(&columns->add(label(text)));
    }
    application.set_content(std::move(columns));
    application.run_frame();
    std::vector<long> widths;
    for(std::size_t index{}; index < children.size(); ++ index)
    {
        const auto& bounds{children[index]->bounds()};
        const auto left{pixel_of(bounds.x, 12)};
        const auto end{pixel_of(bounds.x + bounds.width, 12)};
        widths.push_back(end - left);
        if(index + 1 < children.size())
        {
            EXPECT_EQ(end, pixel_of(children[index + 1]->bounds().x, 12)) << "no gap and no overlap after " << index;
        }
    }
    EXPECT_LE(*std::max_element(widths.begin(), widths.end()) - *std::min_element(widths.begin(), widths.end()), 1);
    // Less than a pixel each is left over, where whole cells would leave a cell or more.
    EXPECT_GT(widths[0] + widths[1] + widths[2], 1000 - 3);
}

TEST(OuiGraphicsTests, APopupAtAnAnchorBetweenCellsStartsAtTheAnchorsPixel)
{
    auto backend{image_backend({1000, 700})};
    Application application{std::move(backend)};
    application.set_content(label("Under"));
    auto list{std::make_unique<PopupList>(Point{10.25, 5.5})};
    list->set_placement(PopupPlacement::at_anchor);
    list->set_items({PopupItem{"First"}, PopupItem{"Second"}});
    const auto& popup{*list};
    application.show_popup(std::move(list));
    application.run_frame();
    auto cell{default_cell_font()};
    auto interface{default_interface_font()};
    auto symbols{symbol_font(12, 24, cell->ascent())};
    Canvas canvas{1000, 700};
    const auto style{classic_style()};
    const GraphicalPainter painter{canvas, style, *cell, *interface, *symbols};
    const auto area{painter.pixel_bounds(popup.bounds())};
    ASSERT_TRUE(area.has_value());
    EXPECT_EQ(area->x, 123);
    EXPECT_EQ(area->y, 132);
}

TEST(OuiGraphicsTests, TheWindowsLastRowEndsAtItsLastPixel)
{
    auto backend{image_backend({1000, 700})};
    Application application{std::move(backend)};
    auto rows{grid()};
    rows->set_columns({Length::fill()});
    rows->set_rows({Length::fill(), Length::cells(1)});
    rows->add(label("Content"));
    const auto& status{rows->add(label("Status"))};
    application.set_content(std::move(rows));
    application.run_frame();
    EXPECT_EQ(pixel_of(status.bounds().y + status.bounds().height, 24), 700);
    EXPECT_EQ(pixel_of(status.bounds().height, 24), 24);
}

TEST(OuiGraphicsTests, TheSameLayoutsInATerminalAreInWholeCells)
{
    const auto is_whole{[](double value) { return value == std::floor(value); }};
    auto backend{scripted_backend({}, {40, 12})};
    Application application{std::move(backend)};
    auto split{split_view(SplitView::Orientation::horizontal)};
    auto columns{grid()};
    columns->set_columns({Length::flex(), Length::flex(), Length::flex()});
    std::vector<Label*> children;
    for(const auto* const text : {"One", "Two", "Three"})
    {
        children.push_back(&columns->add(label(text)));
    }
    split->add(std::move(columns));
    auto& right{split->add(label("Right"))};
    split->set_ratio(0.37);
    application.set_content(std::move(split));
    application.run_frame();
    // The divider in column 14, the nearest to 37% of the 39 the sides share.
    EXPECT_EQ(right.bounds().x, 15);
    for(const auto* const child : children)
    {
        EXPECT_TRUE(is_whole(child->bounds().x)) << child->bounds().x;
        EXPECT_EQ(child->bounds().width, 4);
    }
}

TEST(OuiGraphicsTests, ShowsAnApplicationInTheClassicStyle)
{
    auto backend{image_backend({480, 240})};
    auto& image{*backend};
    Application application{std::move(backend)};
    application.set_theme(classic_theme());
    auto window{oui::window("Hello")};
    auto content{column()};
    button("Press", *content, [] {});
    window->add(std::move(content));
    application.set_window(std::move(window));
    application.run_frame();
    ASSERT_GE(image.frame_count(), 1u);
    EXPECT_EQ(image.size().width, 40);
    // The green title bar shows, and the window's face round what it holds.
    const auto& canvas{image.canvas()};
    const auto style{classic_style()};
    bool title{false};
    bool face{false};
    for(int x{}; x < canvas.width(); ++ x)
    {
        for(int y{}; y < canvas.height(); ++ y)
        {
            title = title || same(canvas.pixel_at(x, y), style.title);
            face = face || same(canvas.pixel_at(x, y), style.face);
        }
    }
    EXPECT_TRUE(title);
    EXPECT_TRUE(face);
}

TEST(OuiGraphicsTests, MeasuresTextInColumnsUntilAGraphicalBackendLives)
{
    EXPECT_EQ(text_metrics().cell_width(), 1);
    EXPECT_EQ(text_metrics().width("abc"), 3);
    // A position inside a character gives its start, or its end for a drag.
    EXPECT_EQ(text_metrics().byte_at("abc", 1, false), 1u);
    EXPECT_EQ(text_metrics().byte_at("abc", 1, true), 2u);
    {
        auto backend{image_backend({240, 48})};
        EXPECT_EQ(text_metrics().cell_width(), 12);
        EXPECT_LT(text_metrics().width("iii"), text_metrics().width("MMM"));
        // In a proportional font a position takes the nearer edge of its character.
        const auto m{text_metrics().width("M")};
        EXPECT_EQ(text_metrics().byte_at("MM", m / 2 - 1, false), 0u);
        EXPECT_EQ(text_metrics().byte_at("MM", m / 2 + 1, true), 1u);
    }
    EXPECT_EQ(text_metrics().cell_width(), 1);
}

TEST(OuiGraphicsTests, ZoomsThroughEveryFontSize)
{
    auto backend{image_backend({480, 240})};
    auto& image{*backend};
    Application application{std::move(backend)};
    auto window{oui::window("Hello")};
    window->add(label("Hello"));
    application.set_window(std::move(window));
    EXPECT_EQ(image.zoom(), PixelBackend::default_zoom());
    double previous_columns{1 << 20};
    for(int level{}; level < PixelBackend::zoom_levels(); ++ level)
    {
        image.set_zoom(level);
        application.run_frame();
        // Bigger text leaves fewer cells across.
        if(level > 0)
        {
            EXPECT_LT(image.size().width, previous_columns);
        }
        previous_columns = image.size().width;
        EXPECT_EQ(text_metrics().cell_width(), image.cell_size().width);
    }
    image.set_zoom(PixelBackend::zoom_levels());
    EXPECT_EQ(image.zoom(), PixelBackend::zoom_levels() - 1);
}

TEST(OuiGraphicsTests, WritesAPngImage)
{
    Canvas canvas{3, 2};
    canvas.fill({0, 0, 3, 2}, {1, 2, 3});
    const auto path{std::filesystem::temp_directory_path() / "oui_graphics_test.png"};
    write_png(canvas, path);
    const auto bytes{oui::read_file(path).value_or(std::string{})};
    std::filesystem::remove(path);
    ASSERT_GT(bytes.size(), 33u);
    EXPECT_EQ(bytes.substr(1, 3), "PNG");
    EXPECT_EQ(bytes.substr(12, 4), "IHDR");
    // Width and height, big-endian.
    EXPECT_EQ(bytes[19], '\x03');
    EXPECT_EQ(bytes[23], '\x02');
}

TEST(OuiGraphicsTests, DrawsAnIconOfLettersWithRoundCorners)
{
    const auto icon{oui::text_icon("OC", {0, 0, 170}, {255, 255, 255}, 64)};
    ASSERT_EQ(icon.size, 64);
    ASSERT_EQ(icon.pixels.size(), 64u * 64u);
    const auto at{[&icon](int x, int y) { return icon.pixels[static_cast<std::size_t>(y * icon.size + x)]; }};
    EXPECT_EQ(at(0, 0) >> 24u, 0u) << "a corner is see-through";
    EXPECT_EQ(at(63, 63) >> 24u, 0u);
    EXPECT_EQ(at(32, 2), 0xFF0000AAu) << "an edge is the background, opaque";
    std::size_t lettered{};
    for(const auto pixel : icon.pixels)
    {
        lettered += pixel == 0xFFFFFFFFu ? 1u : 0u;
    }
    EXPECT_GT(lettered, 50u) << "the letters are there";
}

} // namespace

TEST(OuiGraphicsTests, ControlAndTheWheelMakeTextBiggerOrSmaller)
{
    // As a window's every program: up bigger, down smaller; the wheel alone still scrolls.
    MouseEvent up{10, 10, false};
    up.wheel_delta = 1;
    up.control = true;
    MouseEvent down{up};
    down.wheel_delta = -1;
    MouseEvent plain{up};
    plain.control = false;
    auto backend{image_backend({480, 240})};
    auto& image{*backend};
    Application application{std::move(backend)};
    application.set_theme(classic_theme());
    application.set_window(oui::window("Zoom"));
    application.run_frame();
    const auto start{image.zoom()};
    const auto cells{image.size().width};
    const auto after{[&application, &image](const MouseEvent& event)
                     {
                         image.push(event);
                         application.run_frame();
                         return image.zoom();
                     }};
    EXPECT_EQ(after(up), start + 1);
    EXPECT_EQ(after(up), start + 2);
    EXPECT_LT(image.size().width, cells);
    EXPECT_EQ(after(down), start + 1);
    EXPECT_EQ(after(plain), start + 1);
    // A desktop leaves them to the program under the pointer.
    application.set_wheel_zooms(false);
    EXPECT_EQ(after(up), start + 1);
}

TEST(OuiGraphicsTests, ANotchOfTheWheelScrollsThreeLinesOrWithControlZoomsOneStep)
{
    const auto scrolled{wheel_events({3, 4}, -2, false, false, false)};
    ASSERT_EQ(scrolled.size(), 6u);
    EXPECT_EQ(scrolled.front().wheel_delta, -1);
    EXPECT_FALSE(scrolled.front().control);
    const auto zoomed{wheel_events({3, 4}, 2, true, false, false)};
    ASSERT_EQ(zoomed.size(), 2u);
    EXPECT_EQ(zoomed.front().wheel_delta, 1);
    EXPECT_TRUE(zoomed.front().control);
    EXPECT_EQ(zoomed.front().x, 3);
}

TEST(OuiGraphicsTests, ControlAndPlusOrMinusMakeTextBiggerOrSmallerAndZeroAsItWas)
{
    auto backend{image_backend({480, 240})};
    auto& image{*backend};
    Application application{std::move(backend)};
    application.set_theme(classic_theme());
    application.set_window(oui::window("Zoom"));
    auto told{0};
    application.on_zoomed([&told] { ++ told; });
    application.run_frame();
    const auto start{image.zoom()};
    const auto after{[&application, &image](char32_t character)
                     {
                         image.push(TextEvent{character, true});
                         application.run_frame();
                         return image.zoom();
                     }};
    EXPECT_EQ(after(U'+'), start + 1);
    EXPECT_EQ(after(U'='), start + 2);
    EXPECT_EQ(after(U'-'), start + 1);
    EXPECT_EQ(after(U'0'), PixelBackend::default_zoom());
    EXPECT_EQ(told, 4);
}

TEST(OuiGraphicsTests, InATerminalThereIsNoZoomToOffer)
{
    Application application{scripted_backend({})};
    EXPECT_FALSE(application.commands().title("zoom.in"));
}

TEST(OuiGraphicsTests, DrawsTheSquareRootTheFontsLack)
{
    // The calculator writes √; the cell font at 24 and the interface font have none, and it showed as a box.
    auto cell{default_cell_font()};
    auto interface{default_interface_font()};
    auto symbols{symbol_font(12, 24, cell->ascent())};
    ASSERT_FALSE(cell->has_glyph(U'√'));
    ASSERT_FALSE(interface->has_glyph(U'√'));
    const FontSet fonts{*cell, *interface, *symbols};
    const auto& root{fonts.cell_glyph(U'√')};
    EXPECT_EQ(&fonts.interface_glyph(U'√'), &root);
    // A tick down on the left to the bottom, and a long stroke up to the top on the right: ink low on the left half and
    // high on the right half, and not the box that stands for what no font has.
    const auto inked{[&root](int x0, int y0, int x1, int y1)
                     {
                         auto count{0};
                         for(int y{y0}; y < y1; ++ y)
                         {
                             for(int x{x0}; x < x1; ++ x)
                             {
                                 count += root.coverage[static_cast<std::size_t>(y * root.width + x)] > 128 ? 1 : 0;
                             }
                         }
                         return count;
                     }};
    const auto half{root.width / 2};
    EXPECT_GT(inked(0, root.height * 2 / 3, half, root.height), 0) << "the bottom of the tick";
    EXPECT_GT(inked(half, 0, root.width, root.height / 3), 0) << "the top of the long stroke";
    EXPECT_EQ(inked(half, root.height * 2 / 3, root.width, root.height), 0) << "nothing low on the right";
}

TEST(OuiGraphicsTests, AnEditorsCursorInPixelsIsABarBeforeItsCharacterNotABlock)
{
    // In a terminal the character under the cursor shows reversed; in a window the cursor is a thin bar at the cell's
    // left, as in a text field, and the character keeps its colours.
    auto cell{default_cell_font()};
    auto interface{default_interface_font()};
    auto symbols{symbol_font(12, 24, cell->ascent())};
    Canvas canvas{48, 24};
    const Color text{200, 200, 200};
    const Color background{10, 20, 30};
    canvas.fill({0, 0, 48, 24}, background);
    const auto style{classic_style()};
    GraphicalPainter painter{canvas, style, *cell, *interface, *symbols};
    painter.draw_text_cursor({1, 0}, " ", {text, background});
    EXPECT_TRUE(same(canvas.pixel_at(12, 12), text));
    EXPECT_TRUE(same(canvas.pixel_at(18, 12), background));
    EXPECT_TRUE(same(canvas.pixel_at(22, 12), background));
}

TEST(OuiGraphicsTests, AProgressBarInPixelsFillsItsTrackAsFarAsIsDone)
{
    auto cell{default_cell_font()};
    auto interface{default_interface_font()};
    auto symbols{symbol_font(12, 24, cell->ascent())};
    Canvas canvas{240, 24};
    canvas.fill({0, 0, 240, 24}, {1, 2, 3});
    const auto style{classic_style()};
    const auto theme{theme_for(style)};
    GraphicalPainter painter{canvas, style, *cell, *interface, *symbols};
    painter.draw_progress({0, 0, 20, 1}, {0.25, ""}, theme);
    // A quarter of 240 pixels: what is done is in the title's colour, what is not is the well's.
    EXPECT_TRUE(same(canvas.pixel_at(30, 12), style.title));
    EXPECT_TRUE(same(canvas.pixel_at(150, 12), style.well));
}

TEST(OuiGraphicsTests, AButtonTakesItsWholeWidthWhereNoShadowIsDrawn)
{
    // A terminal's button keeps a column for its shadow; drawn in pixels there is none, and the column left a stretched
    // button short of the fields over it, and a row of buttons off its centre.
    auto cell{default_cell_font()};
    auto interface{default_interface_font()};
    auto symbols{symbol_font(12, 24, cell->ascent())};
    Canvas canvas{240, 72};
    const auto style{classic_style()};
    const auto theme{theme_for(style)};
    GraphicalPainter painter{canvas, style, *cell, *interface, *symbols};
    const Color around{1, 2, 3};
    ButtonLook look;
    look.text = "Reset";
    look.around = around;
    painter.draw_button({0, 0, 10, 2}, look, theme);
    // Ten cells of 12: the right edge of the last one, halfway down the two rows, is the button's, not what is around.
    EXPECT_FALSE(same(canvas.pixel_at(10 * 12 - 2, 24), around));
    EXPECT_FALSE(same(canvas.pixel_at(1, 24), around));
}

TEST(OuiGraphicsTests, WidgetsWithTextAskForTheCellsItShowsInNotItsCharacters)
{
    // In a proportional font "Rename Files" takes about half the cells of its twelve characters: a window of such
    // widgets shrinks round what it shows, as in a terminal.
    auto backend{image_backend({480, 240})};
    const auto& image{*backend};
    Application application{std::move(backend)};
    const std::string text{"Rename Files"};
    const auto cell{whole(image.cell_size().width)};
    // Room for the text as drawn, and for the padding interface text keeps.
    const auto at_least{(text_metrics().width(text) + 2 * text_metrics().field_padding() + cell - 1) / cell};
    ASSERT_LT(at_least, text_width(text));
    const auto width_of{[](const Widget& widget) { return widget.measure({}).width; }};
    EXPECT_EQ(width_of(*label(text)), at_least);
    EXPECT_EQ(width_of(*button(text)), at_least + 5);
    EXPECT_EQ(width_of(*check_button(text)), at_least + 5);
    EXPECT_EQ(width_of(*combo_box({text, "Go"})), at_least + 4);
    // No text takes nothing, padding and all: drawn, it covers nothing beside it.
    EXPECT_EQ(shown_width(""), 0);
}

TEST(OuiGraphicsTests, ATooltipInPixelsHasAnEdgeAndLetsWhatIsUnderShowThrough)
{
    // Over a list's drop-down or a field, what it covers still shows faintly, and its edge sets it apart.
    auto cell{default_cell_font()};
    auto interface{default_interface_font()};
    auto symbols{symbol_font(12, 24, cell->ascent())};
    const auto style{classic_style()};
    const auto theme{theme_for(style)};
    // The same tooltip over red and over blue, as two canvases.
    const auto over{[&](Color under)
                    {
                        Canvas canvas{240, 96};
                        canvas.fill({0, 0, 240, 96}, under);
                        GraphicalPainter painter{canvas, style, *cell, *interface, *symbols};
                        painter.draw_tooltip({1, 1, 8, 2}, {"tip", "two"}, theme);
                        return canvas;
                    }};
    const Color red{200, 0, 0};
    const auto on_red{over(red)};
    const auto on_blue{over({0, 0, 200})};
    // Cells of 12 by 24 from column 1 of row 1: its left edge, and a spot inside clear of the text.
    EXPECT_TRUE(same(on_red.pixel_at(12, 40), style.text));
    const auto inside{on_red.pixel_at(12 + 8 * 12 - 6, 40)};
    EXPECT_FALSE(same(inside, red));
    EXPECT_FALSE(same(inside, on_blue.pixel_at(12 + 8 * 12 - 6, 40))) << "what is under shows through";
    // Outside it, nothing changed.
    EXPECT_TRUE(same(on_red.pixel_at(12 + 8 * 12 + 2, 40), red));
}

TEST(OuiGraphicsTests, APointerCoversTheColumnsItsArrowReaches)
{
    // An arrow reaches about three fifths of its size to the right of its tip.
    EXPECT_EQ(oui::PixelBackend::columns_covered(0, 10, 24), 2) << "15 pixels from the left of a cell of 10";
    EXPECT_EQ(oui::PixelBackend::columns_covered(8, 10, 24), 3) << "from near its right, into a third";
    EXPECT_EQ(oui::PixelBackend::columns_covered(0, 10, 48), 3) << "29 pixels";
    EXPECT_EQ(oui::PixelBackend::columns_covered(9, 10, 48), 4);
    EXPECT_EQ(oui::PixelBackend::columns_covered(0, 10, 16), 1) << "10 pixels, its own cell only";
}

TEST(OuiGraphicsTests, ATooltipInPixelsIsAsWideAsItsTextShows)
{
    // Its text in a proportional font takes fewer pixels than its characters take cells: the box fits the text, not
    // the characters.
    auto backend{image_backend({1000, 240})};
    auto& image{*backend};
    Application application{std::move(backend)};
    const std::string tip{"Code that opens this window, each widget with an id wired, in a new tab"};
    auto content{column()};
    auto& told{content->add(label("told"))};
    told.set_tooltip(tip);
    application.set_content(std::move(content));
    application.run_frame();
    image.push(MouseEvent{told.bounds().x, told.bounds().y, false, 0, false, false, true});
    application.run_frame();
    std::this_thread::sleep_for(std::chrono::milliseconds{700});
    application.run_frame();
    application.run_frame();
    // The box's top edge: the longest run of the edge's colour along a row.
    const auto& canvas{image.canvas()};
    const auto edge{classic_style().text};
    int widest{};
    for(int y{}; y < canvas.height(); ++ y)
    {
        int run{};
        for(int x{}; x < canvas.width(); ++ x)
        {
            run = same(canvas.pixel_at(x, y), edge) ? run + 1 : 0;
            widest = std::max(widest, run);
        }
    }
    const auto text{text_metrics().width(tip)};
    ASSERT_GT(widest, text) << "a box round the text";
    // Its margins and a cell to round the text up to whole cells, no more.
    EXPECT_LE(widest, text + 3 * image.cell_size().width) << widest << " for " << text;
}

TEST(OuiGraphicsTests, AWindowInPixelsKeepsTheLeastRoomCellsAllowRoundWhatItHolds)
{
    // In a terminal the title is in the frame's own line, and what the window holds starts on the next, two columns
    // in; drawn in pixels the title bar is a row of its own, and the room round what it holds is a cell: a row under
    // the bar and at the bottom, a column at the sides.
    const auto gaps{[](std::unique_ptr<Backend> backend)
                    {
                        Application application{std::move(backend)};
                        auto window{oui::window("Hello")};
                        auto& text{window->add(label("inside"))};
                        auto& window_reference{*window};
                        application.set_window(std::move(window));
                        application.run_frame();
                        const auto& frame{window_reference.bounds()};
                        return std::tuple{text.bounds().y - frame.y,
                                          frame.y + frame.height - (text.bounds().y + text.bounds().height),
                                          text.bounds().x - frame.x};
                    }};
    // Above, below, and at the side: a terminal's frame and a cell beside it; in pixels as little as cells allow.
    EXPECT_EQ(gaps(scripted_backend({}, {40, 12})), (std::tuple{1, 1, 2})) << "in a terminal, the frame's lines";
    EXPECT_EQ(gaps(image_backend({480, 288})), (std::tuple{2, 1, 1})) << "in pixels, the title bar and a row";
}

// WCAG's contrast between two colours, from 1 to 21.
[[nodiscard]] double contrast(Color first, Color second)
{
    const auto luminance{
        [](Color color)
        {
            const auto linear{[](std::uint8_t channel)
                              {
                                  const auto value{channel / 255.0};
                                  return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
                              }};
            return 0.2126 * linear(color.red) + 0.7152 * linear(color.green) + 0.0722 * linear(color.blue);
        }};
    const auto a{luminance(first)};
    const auto b{luminance(second)};
    return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
}

TEST(OuiGraphicsTests, TheDarkLookIsFlatInNotoSansWithOneAccentAndItsTextReads)
{
    const auto style{dark_style()};
    EXPECT_TRUE(style.flat);
    EXPECT_EQ(style.interface_font, InterfaceFont::noto_sans);
    EXPECT_FALSE(classic_style().flat);
    EXPECT_EQ(classic_style().interface_font, InterfaceFont::jersey);
    // One accent: the title, the focus and what is selected.
    EXPECT_EQ(style.focus, style.title);
    EXPECT_EQ(style.selection, style.title);
    // Dark: its surfaces darker than its text.
    EXPECT_LT(contrast(style.face, {0, 0, 0}), 2.0);
    EXPECT_LT(contrast(style.well, {0, 0, 0}), 2.0);
    EXPECT_GE(contrast(style.text, style.face), 7.0);
    EXPECT_GE(contrast(style.well_text, style.well), 7.0);
    EXPECT_GE(contrast(style.muted_text, style.face), 4.5);
    EXPECT_GE(contrast(style.title_text, style.title), 4.0);
    EXPECT_GE(contrast(style.selection_text, style.selection), 4.0);

    const auto theme{dark_theme()};
    EXPECT_EQ(theme.menu_selected_background, style.title);
    EXPECT_EQ(theme.item_selected_background, style.selection);
    EXPECT_EQ(theme.editor_background, style.well);

    const auto themes{graphical_themes()};
    EXPECT_EQ(themes.front().id, "classic");
    const auto dark{std::find_if(themes.begin(), themes.end(), [](const auto& theme) { return theme.id == "dark"; })};
    ASSERT_NE(dark, themes.end());
    EXPECT_EQ(dark->name, "Dark");
    EXPECT_TRUE(dark->style.flat);
}

TEST(OuiGraphicsTests, AFlatButtonHasAThinEdgeAllRoundAndHeldDownIsFilledLighter)
{
    auto cell{default_cell_font()};
    auto interface{default_interface_font()};
    auto symbols{symbol_font(12, 24, cell->ascent())};
    const auto style{dark_style()};
    const auto theme{theme_for(style)};
    const auto drawn{[&](bool pressed)
                     {
                         Canvas canvas{120, 48};
                         GraphicalPainter painter{canvas, style, *cell, *interface, *symbols};
                         ButtonLook look;
                         look.pressed = pressed;
                         look.around = {1, 2, 3};
                         painter.draw_button({0, 0, 10, 2}, look, theme);
                         return canvas;
                     }};
    const auto resting{drawn(false)};
    // Ten cells of 12 by two rows of 24: the shape is 34 pixels high in the middle of the 48, from 7 to 40.
    EXPECT_TRUE(same(resting.pixel_at(60, 7), style.shadow));
    EXPECT_TRUE(same(resting.pixel_at(60, 40), style.shadow));
    EXPECT_TRUE(same(resting.pixel_at(0, 24), style.shadow));
    EXPECT_TRUE(same(resting.pixel_at(119, 24), style.shadow));
    EXPECT_TRUE(same(resting.pixel_at(2, 10), theme.button_background));
    EXPECT_TRUE(same(resting.pixel_at(60, 6), {1, 2, 3}));

    const auto pressed{drawn(true)};
    EXPECT_TRUE(same(pressed.pixel_at(60, 7), style.shadow));
    EXPECT_TRUE(same(pressed.pixel_at(0, 24), style.shadow));
    EXPECT_TRUE(same(pressed.pixel_at(2, 10), style.light));
}

TEST(OuiGraphicsTests, AButtonWithAnIconDrawsItInPlaceOfItsText)
{
    auto cell{default_cell_font()};
    auto interface{default_interface_font()};
    auto symbols{symbol_font(12, 24, cell->ascent())};
    const auto style{dark_style()};
    const auto theme{theme_for(style)};
    const auto drawn{[&](std::string_view text, std::optional<Icon> icon)
                     {
                         Canvas canvas{120, 48};
                         GraphicalPainter painter{canvas, style, *cell, *interface, *symbols};
                         ButtonLook look;
                         look.text = text;
                         look.icon = icon;
                         painter.draw_button({0, 0, 10, 2}, look, theme);
                         return canvas.pixels();
                     }};
    EXPECT_EQ(drawn("Play", Icon::play), drawn("Something else", Icon::play));
    EXPECT_NE(drawn("Play", Icon::play), drawn("Play", std::nullopt));
    EXPECT_NE(drawn("Play", Icon::play), drawn("Play", Icon::pause));

    auto made{button("Play")};
    EXPECT_EQ(made->icon(), std::nullopt);
    made->set_icon(Icon::play);
    EXPECT_EQ(made->icon(), Icon::play);
    EXPECT_EQ(made->text(), "Play");
}

TEST(OuiGraphicsTests, EachIconOfAPlayerAndATrackIsDrawnInsideItsAreaAndUnlikeTheOthers)
{
    const Color ink{255, 255, 255};
    const Icon icons[]{Icon::play,    Icon::pause,      Icon::step_back, Icon::step_forwards,
                       Icon::speaker, Icon::headphones, Icon::eye,       Icon::lock};
    std::vector<std::vector<std::uint32_t>> seen;
    for(const auto icon : icons)
    {
        Canvas canvas{48, 48};
        draw_icon(canvas, icon, {12, 12, 24, 24}, ink);
        int inked{};
        for(int y{}; y < 48; ++ y)
        {
            for(int x{}; x < 48; ++ x)
            {
                const auto inside{x >= 12 && x < 36 && y >= 12 && y < 36};
                const auto lit{canvas.pixel_at(x, y).red > 0};
                EXPECT_TRUE(inside || !lit) << static_cast<int>(icon) << " at " << x << ", " << y;
                inked += lit ? 1 : 0;
            }
        }
        // Each covers a good part of its square, and none is another's.
        EXPECT_GT(inked, 24 * 24 / 8) << static_cast<int>(icon);
        EXPECT_EQ(std::find(seen.begin(), seen.end(), canvas.pixels()), seen.end()) << static_cast<int>(icon);
        seen.push_back(canvas.pixels());
    }
}

TEST(OuiGraphicsTests, ABackendInTheDarkStyleWritesInterfaceTextInNotoSans)
{
    const std::string text{"Rename Files"};
    const auto jersey{built_in_interface_font(24.0f)->measure(text)};
    const auto noto{built_in_interface_font(24.0f, InterfaceFont::noto_sans)->measure(text)};
    ASSERT_NE(jersey, noto);
    auto backend{image_backend({480, 240})};
    auto& image{*backend};
    Application application{std::move(backend)};
    EXPECT_EQ(text_metrics().width(text), jersey);
    image.set_style(dark_style());
    EXPECT_EQ(text_metrics().width(text), noto);
    EXPECT_TRUE(built_in_interface_font(24.0f, InterfaceFont::noto_sans)->has_glyph(U'ñ'));
}

TEST(OuiGraphicsTests, ASpacedFontHasItsLeadingHalfAboveAndHalfBelowTheSameGlyphs)
{
    auto plain{built_in_cell_font(24)};
    const auto ascent{plain->ascent()};
    const auto height{plain->line_height()};
    const auto width{plain->glyph(U'g').width};
    const auto spaced{spaced_font(built_in_cell_font(24), 4)};
    EXPECT_EQ(spaced->line_height(), height + 4);
    EXPECT_EQ(spaced->ascent(), ascent + 2);
    EXPECT_EQ(spaced->glyph(U'g').width, width);
    EXPECT_TRUE(spaced->has_glyph(U'g'));
}

// Noto Sans fills its line with room for accents above and tails below, so at a row's height its text sat on the row's
// lower edge; in the dark style the rows are taller than the interface text, which keeps its size.
TEST(OuiGraphicsTests, InTheDarkStyleARowLeavesRoomAboveAndBelowInterfaceText)
{
    auto backend{image_backend({480, 240})};
    auto& image{*backend};
    Application application{std::move(backend)};
    EXPECT_EQ(image.cell_size().height, built_in_interface_font(24.0f)->line_height());
    image.set_style(dark_style());
    const auto interface{built_in_interface_font(24.0f, InterfaceFont::noto_sans)->line_height()};
    EXPECT_EQ(interface, 24);
    EXPECT_GE(image.cell_size().height, interface + 4);
}
