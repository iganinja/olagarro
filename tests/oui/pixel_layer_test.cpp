#include <oui/application.h>
#include <oui/graphics/canvas.h>
#include <oui/graphics/font.h>
#include <oui/graphics/graphical_painter.h>
#include <oui/graphics/pixel_backend.h>
#include <oui/grid.h>
#include <oui/label.h>
#include <oui/menu_bar.h>
#include <oui/widget.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace
{

using namespace olagarro;
using namespace olagarro::oui;

// A layer whose pixels are only in memory, as a test's.
class MemoryLayer final : public PixelLayer
{
  public:
    MemoryLayer(int width, int height, std::vector<std::uint32_t> pixels)
        : m_width{width}, m_height{height}, m_pixels{std::move(pixels)}
    {
    }

    int width() const override { return m_width; }
    int height() const override { return m_height; }
    SDL_GPUTexture* texture() const override { return nullptr; }
    std::vector<std::uint32_t> pixels() const override { return m_pixels; }

  private:
    int m_width;
    int m_height;
    std::vector<std::uint32_t> m_pixels;
};

// Shows a layer over all of itself, as a video's viewer.
class LayerView final : public Widget
{
  public:
    explicit LayerView(std::shared_ptr<const PixelLayer> layer) : m_layer{std::move(layer)} {}

    Size measure(const Constraints& constraints) const override { return constraints.constrain({1, 1}); }
    void paint(Painter& painter, const Theme&) const override { painter.draw_layer(bounds(), m_layer); }

  private:
    std::shared_ptr<const PixelLayer> m_layer;
};

[[nodiscard]] std::shared_ptr<const PixelLayer> solid_layer(std::uint32_t pixel)
{
    return std::make_shared<MemoryLayer>(4, 2, std::vector<std::uint32_t>(8, pixel));
}

const Color red{255, 0, 0};

struct Fonts
{
    std::unique_ptr<Font> cell{default_cell_font()};
    std::unique_ptr<Font> interface{default_interface_font()};
    std::unique_ptr<Font> symbols{symbol_font(12, 24, cell->ascent())};
};

TEST(OuiPixelLayerTests, ACanvasIsSolidButWhereItIsCleared)
{
    Canvas canvas{8, 4};
    canvas.fill({0, 0, 8, 4}, {10, 20, 30});
    canvas.clear({2, 1, 3, 2});
    EXPECT_EQ(canvas.alpha_at(0, 0), 255);
    EXPECT_EQ(canvas.alpha_at(2, 1), 0);
    EXPECT_EQ(canvas.alpha_at(4, 2), 0);
    EXPECT_EQ(canvas.alpha_at(5, 2), 255);
    // What is copied is solid, whatever its top byte says.
    const std::uint32_t pixel{0x00ffffffu};
    canvas.copy(2, 1, &pixel, 1, 1, 1);
    EXPECT_EQ(canvas.alpha_at(2, 1), 255);
    EXPECT_EQ(canvas.pixels()[static_cast<std::size_t>(1 * 8 + 2)], 0xffffffffu);
}

TEST(OuiPixelLayerTests, WhatIsMixedOverASeeThroughPixelKeepsItsColourAndItsAlpha)
{
    Canvas canvas{2, 1};
    canvas.clear({0, 0, 2, 1});
    canvas.blend(0, 0, {200, 100, 50}, 128);
    EXPECT_EQ(canvas.alpha_at(0, 0), 128);
    EXPECT_EQ(canvas.pixel_at(0, 0), (Color{200, 100, 50}));
    // Twice: more solid, the same colour.
    canvas.blend(0, 0, {200, 100, 50}, 128);
    EXPECT_NEAR(canvas.alpha_at(0, 0), 192, 1);
    EXPECT_EQ(canvas.pixel_at(0, 0), (Color{200, 100, 50}));
    // Half white over half black: grey, three quarters solid.
    canvas.blend(1, 0, {0, 0, 0}, 128);
    canvas.blend(1, 0, {255, 255, 255}, 128);
    EXPECT_NEAR(canvas.alpha_at(1, 0), 192, 1);
    EXPECT_NEAR(canvas.pixel_at(1, 0).red, 170, 2);
    canvas.blend(1, 0, {0, 0, 0}, 255);
    EXPECT_EQ(canvas.alpha_at(1, 0), 255);
}

TEST(OuiPixelLayerTests, APainterThatShowsLayersBeneathLeavesTheirPlaceSeeThroughTillPaintedOver)
{
    Fonts fonts;
    Canvas canvas{240, 96};
    const auto style{classic_style()};
    canvas.fill({0, 0, 240, 96}, style.desktop);
    std::vector<PlacedLayer> layers;
    GraphicalPainter painter{canvas, style, *fonts.cell, *fonts.interface, *fonts.symbols, std::nullopt, &layers};
    const auto layer{solid_layer(0xffff0000u)};
    // Cells of 12 by 24: columns 2 to 10 and rows 1 to 3, cut by a clip to the first two rows.
    painter.push_clip({0, 0, 20, 2});
    painter.draw_layer({2, 1, 8, 2}, layer);
    painter.pop_clip();
    ASSERT_EQ(layers.size(), 1u);
    EXPECT_EQ(layers[0].layer, layer);
    EXPECT_EQ(layers[0].area.x, 24);
    EXPECT_EQ(layers[0].area.y, 24);
    EXPECT_EQ(layers[0].area.width, 96);
    EXPECT_EQ(layers[0].area.height, 48);
    EXPECT_EQ(layers[0].visible.height, 24);
    EXPECT_EQ(canvas.alpha_at(30, 30), 0);
    EXPECT_EQ(canvas.alpha_at(30, 50), 255) << "cut by the clip";
    EXPECT_EQ(canvas.alpha_at(10, 30), 255);
    // What is painted over it afterwards covers it.
    painter.fill_rect({3, 1, 1, 1}, {0, 0, 255});
    EXPECT_EQ(canvas.alpha_at(40, 30), 255);
    EXPECT_EQ(canvas.alpha_at(30, 30), 0);
}

TEST(OuiPixelLayerTests, AWidgetsLayerShowsInAScreenshot)
{
    auto backend{image_backend({480, 240})};
    auto& image{*backend};
    Application application{std::move(backend)};
    // Red on the left half, blue on the right, and a see-through pixel at the bottom right.
    application.set_content(std::make_unique<LayerView>(std::make_shared<MemoryLayer>(
        2, 2, std::vector<std::uint32_t>{0xffff0000u, 0xff0000ffu, 0xffff0000u, 0x000000ffu})));
    application.run_frame();
    const auto& canvas{image.canvas()};
    EXPECT_EQ(canvas.pixel_at(10, 10), red);
    EXPECT_EQ(canvas.pixel_at(470, 10), (Color{0, 0, 255}));
    EXPECT_EQ(canvas.pixel_at(470, 230), image.style().desktop);
    // Drawn in place: nothing is left to show beneath.
    for(const auto pixel : canvas.pixels())
    {
        ASSERT_EQ(pixel >> 24u, 0xffu);
    }
}

// A backend that shows its layers beneath the canvas, as a window with a device does, keeping them as one.
class LayeredBackend final : public PixelBackend
{
  public:
    explicit LayeredBackend(Size pixels) { resize(whole(pixels.width), whole(pixels.height)); }

    std::optional<Event> poll_event() override { return std::nullopt; }
    std::size_t kept_layers() const { return layers().size(); }
    void drop() { drop_layers(); }

  protected:
    void show(const Canvas&) override {}
    bool shows_layers() const override { return true; }
};

// The last frame's layers may be the last to hold a texture, which a window must let go of while its device is there.
TEST(OuiPixelLayerTests, ABackendKeepsTheLastFramesLayersTillItDropsThem)
{
    auto backend{std::make_unique<LayeredBackend>(Size{480, 240})};
    auto& layered{*backend};
    Application application{std::move(backend)};
    auto layer{solid_layer(0xffff0000u)};
    const std::weak_ptr<const PixelLayer> watched{layer};
    application.set_content(std::make_unique<LayerView>(std::move(layer)));
    application.run_frame();
    ASSERT_EQ(layered.kept_layers(), 1u);
    application.set_content(nullptr);
    EXPECT_FALSE(watched.expired()) << "kept by the last frame";
    layered.drop();
    EXPECT_EQ(layered.kept_layers(), 0u);
    EXPECT_TRUE(watched.expired());
}

TEST(OuiPixelLayerTests, AMenuOpenedOverALayerCoversIt)
{
    auto backend{image_backend({480, 240})};
    auto& image{*backend};
    Application application{std::move(backend)};
    auto layout{grid()};
    layout->set_columns({Length::fill()});
    layout->set_rows({Length::cells(1), Length::fill()});
    auto& bar{layout->add(menu_bar(application.commands()))};
    layout->add(std::make_unique<LayerView>(solid_layer(0xffff0000u)));
    bar.add_menu("&File", {menu_action("Open", [] {}), menu_action("Save everything", [] {})});
    application.set_content(std::move(layout));
    application.run_frame();
    const auto& canvas{image.canvas()};
    // The menu's first entry is on the second row, over the layer.
    EXPECT_EQ(canvas.pixel_at(30, 36), red);
    bar.open_menu(0);
    application.render();
    EXPECT_NE(canvas.pixel_at(30, 36), red);
    EXPECT_EQ(canvas.pixel_at(470, 230), red);
}

TEST(OuiPixelLayerTests, WithoutADeviceAFrameIsSolidAsBefore)
{
    auto backend{image_backend({480, 240})};
    auto& image{*backend};
    EXPECT_EQ(image.gpu_device(), nullptr);
    Application application{std::move(backend)};
    application.set_content(label("Nothing beneath"));
    application.run_frame();
    for(const auto pixel : image.canvas().pixels())
    {
        ASSERT_EQ(pixel >> 24u, 0xffu);
    }
}

} // namespace
