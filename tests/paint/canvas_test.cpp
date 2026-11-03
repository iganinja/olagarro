#include <paint/blend.h>
#include <paint/canvas.h>
#include <paint/color.h>
#include <paint/tile_change.h>

#include <gtest/gtest.h>

#include <array>
#include <string>

using namespace olagarro;
using namespace olagarro::paint;

namespace
{

// Whether two canvases hold the same tiles at the same places with the same pixels.
[[nodiscard]] bool same_tiles(const Canvas& one, const Canvas& other)
{
    if(one.tiles().size() != other.tiles().size())
    {
        return false;
    }
    for(const auto& [index, tile] : one.tiles())
    {
        const auto matching{other.tile(index)};
        if(matching == nullptr || *matching != *tile)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] Image filled(int width, int height, const Rgba8& pixel)
{
    Image image{PixelFormat::rgba8, width, height};
    for(int y{}; y < height; ++ y)
    {
        for(int x{}; x < width; ++ x)
        {
            image.set_pixel(x, y, pixel);
        }
    }
    return image;
}

} // namespace

TEST(Image, EightBitsGoToSixteenAndBackUnchanged)
{
    Image image{PixelFormat::gray_alpha8, 256, 1};
    for(int x{}; x < 256; ++ x)
    {
        image.set_pixel(x, 0, GrayAlpha8{{static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(255 - x)}});
    }
    const auto wide{converted(image, PixelFormat::gray_alpha16)};
    EXPECT_EQ(wide.pixel<GrayAlpha16>(0xab, 0).value[0], 0xabab);
    EXPECT_EQ(converted(wide, PixelFormat::gray_alpha8).bytes, image.bytes);
}

TEST(Image, GreyBecomesColourAndColourGreyByItsLuma)
{
    Image gray{PixelFormat::gray_alpha8, 1, 1};
    gray.set_pixel(0, 0, GrayAlpha8{{100, 255}});
    const auto color{converted(gray, PixelFormat::rgba8).pixel<Rgba8>(0, 0)};
    EXPECT_EQ(color.value, (std::array<std::uint8_t, 4>{100, 100, 100, 255}));
    Image red{PixelFormat::rgba8, 1, 1};
    red.set_pixel(0, 0, Rgba8{{255, 0, 0, 255}});
    EXPECT_EQ(converted(red, PixelFormat::gray_alpha8).pixel<GrayAlpha8>(0, 0).value[0], 76);
}

TEST(Canvas, WhatIsWrittenAcrossTilesReadsBackAndTheRestIsSeeThrough)
{
    Canvas canvas{PixelFormat::rgba8, 600, 600};
    Image square{PixelFormat::rgba8, 300, 20};
    for(int y{}; y < 20; ++ y)
    {
        for(int x{}; x < 300; ++ x)
        {
            square.set_pixel(x, y, Rgba8{{static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y), 7, 255}});
        }
    }
    canvas.write(square, 200, 250);
    EXPECT_EQ(canvas.tiles().size(), 4u) << "it reaches two rows of two tiles";
    EXPECT_EQ(canvas.read(200, 250, 300, 20).bytes, square.bytes);
    const auto around{canvas.read(190, 245, 10, 5)};
    for(const auto byte : around.bytes)
    {
        EXPECT_EQ(byte, 0);
    }
}

TEST(Canvas, ACopySharesItsTilesUntilOneIsWrittenTo)
{
    Canvas canvas{PixelFormat::gray_alpha16, 10, 10};
    Image dot{PixelFormat::gray_alpha16, 1, 1};
    dot.set_pixel(0, 0, GrayAlpha16{{1000, 65535}});
    canvas.write(dot, 5, 5);
    const auto kept{canvas};
    EXPECT_EQ(kept.tile({0, 0}), canvas.tile({0, 0})) << "copying costs nothing";
    dot.set_pixel(0, 0, GrayAlpha16{{2000, 65535}});
    canvas.write(dot, 5, 5);
    EXPECT_NE(kept.tile({0, 0}), canvas.tile({0, 0}));
    EXPECT_EQ(kept.read(5, 5, 1, 1).pixel<GrayAlpha16>(0, 0).value[0], 1000);
    EXPECT_EQ(canvas.read(5, 5, 1, 1).pixel<GrayAlpha16>(0, 0).value[0], 2000);
}

TEST(Canvas, APixelSetReadsBackInEveryFormat)
{
    for(const auto format :
        {PixelFormat::gray_alpha8, PixelFormat::rgba8, PixelFormat::gray_alpha16, PixelFormat::rgba16})
    {
        visit_format(format,
                     [&](auto tag)
                     {
                         using PixelType = typename decltype(tag)::Type;
                         using Channel = typename PixelType::ChannelType;
                         PixelType pixel;
                         for(int index{}; index < PixelType::channels; ++ index)
                         {
                             // Every byte of every channel different, so a byte out of its place shows.
                             pixel.value[index] =
                                 static_cast<Channel>(sizeof(Channel) == 1 ? 40 + index * 50 : 0x1234 + index * 0x2101);
                         }
                         Canvas canvas{format, 600, 600};
                         Image dot{format, 1, 1};
                         dot.set_pixel(0, 0, pixel);
                         canvas.write(dot, 300, 513);
                         const auto back{canvas.read(299, 513, 2, 1)};
                         EXPECT_EQ(back.pixel<PixelType>(1, 0).value, pixel.value) << describe(format);
                         EXPECT_EQ(back.pixel<PixelType>(0, 0).value, PixelType{}.value) << describe(format);
                         ASSERT_EQ(canvas.tiles().size(), 1u) << describe(format);
                         EXPECT_EQ(canvas.tiles().begin()->first, (TileIndex{1, 2})) << describe(format);
                         EXPECT_EQ(canvas.tile({1, 2})->pixel<PixelType>(300 - 256, 1).value, pixel.value)
                             << describe(format);
                     });
    }
}

TEST(Canvas, ATileLeftEmptyIsNotStored)
{
    Canvas canvas{PixelFormat::rgba8, 1000, 1000};
    canvas.write(Image{PixelFormat::rgba8, 600, 300}, 0, 0);
    EXPECT_TRUE(canvas.tiles().empty()) << "nothing written over nothing makes no tile";
    canvas.write(filled(2, 2, Rgba8{{9, 8, 7, 255}}), 255, 10);
    EXPECT_EQ(canvas.tiles().size(), 2u);
    canvas.write(Image{PixelFormat::rgba8, 1, 2}, 256, 10);
    EXPECT_EQ(canvas.tiles().size(), 1u) << "a tile written back to see-through is forgotten";
    EXPECT_NE(canvas.tile({0, 0}), nullptr);
    // A tool writing tile by tile leaves its empty tiles for drop_empty_tiles.
    canvas.writable_tile({3, 3}).set_pixel(0, 0, Rgba8{});
    EXPECT_EQ(canvas.tiles().size(), 2u);
    canvas.drop_empty_tiles();
    EXPECT_EQ(canvas.tiles().size(), 1u);
    EXPECT_EQ(canvas.tile({3, 3}), nullptr);
}

TEST(Canvas, EightBitRgbaGoesInAndComesOutAsItWas)
{
    auto image{filled(700, 300, Rgba8{})};
    image.set_pixel(10, 20, Rgba8{{1, 2, 3, 4}});
    image.set_pixel(699, 299, Rgba8{{250, 128, 0, 255}});
    const auto canvas{from_rgba8(700, 300, image.bytes.data())};
    EXPECT_EQ(canvas.width(), 700);
    EXPECT_EQ(canvas.height(), 300);
    EXPECT_EQ(canvas.tiles().size(), 2u) << "only the tiles with something in them, of six";
    const auto back{to_rgba8(canvas)};
    EXPECT_EQ(back.format, PixelFormat::rgba8);
    EXPECT_EQ(back.width, 700);
    EXPECT_EQ(back.height, 300);
    EXPECT_EQ(back.bytes, image.bytes);
    const auto deep{from_rgba8(700, 300, image.bytes.data(), PixelFormat::rgba16)};
    EXPECT_EQ(deep.read(699, 299, 1, 1).pixel<Rgba16>(0, 0).value,
              (std::array<std::uint16_t, 4>{250 * 257, 128 * 257, 0, 65535}));
    EXPECT_EQ(to_rgba8(deep).bytes, image.bytes) << "16 bits made from 8 go back to the same 8";
}

TEST(TileChange, AppliedAndReversedGivesTheCanvasBackTileForTile)
{
    Canvas canvas{PixelFormat::rgba8, 800, 800};
    canvas.write(filled(300, 10, Rgba8{{200, 0, 0, 255}}), 100, 100);
    canvas.write(filled(5, 5, Rgba8{{0, 200, 0, 255}}), 600, 600);
    const auto before{canvas};
    // A step that changes a tile, makes one, clears one and leaves one as it was.
    canvas.write(filled(10, 10, Rgba8{{0, 0, 200, 128}}), 10, 100);
    canvas.write(filled(3, 3, Rgba8{{1, 1, 1, 255}}), 700, 10);
    canvas.write(Image{PixelFormat::rgba8, 5, 5}, 600, 600);
    const auto after{canvas};
    const auto change{tile_change(before, after)};
    EXPECT_FALSE(change.empty());
    EXPECT_EQ(change.tiles(), (std::vector<TileIndex>{{0, 0}, {2, 0}, {2, 2}}))
        << "not tile 1, 0, which the step did not write";
    change.undo(canvas);
    EXPECT_TRUE(same_tiles(canvas, before));
    EXPECT_EQ(canvas.tile({1, 0}), before.tile({1, 0})) << "the tile not changed is still shared";
    change.redo(canvas);
    EXPECT_TRUE(same_tiles(canvas, after));
    change.undo(canvas);
    EXPECT_EQ(to_rgba8(canvas).bytes, to_rgba8(before).bytes);
}

TEST(TileChange, ATileWrittenWithWhatItHadIsNoChange)
{
    Canvas canvas{PixelFormat::gray_alpha8, 100, 100};
    Image dot{PixelFormat::gray_alpha8, 1, 1};
    dot.set_pixel(0, 0, GrayAlpha8{{50, 255}});
    canvas.write(dot, 1, 1);
    const auto before{canvas};
    canvas.write(dot, 1, 1);
    EXPECT_NE(canvas.tile({0, 0}), before.tile({0, 0})) << "the tile was copied to be written";
    EXPECT_TRUE(tile_change(before, canvas).empty());
    EXPECT_TRUE(tile_change(before, before).empty());
}

TEST(Blend, ModesMixSolidPixelsAsPaintNetDoes)
{
    const Rgba8 below{{200, 100, 50, 255}};
    const Rgba8 above{{100, 100, 200, 255}};
    EXPECT_EQ(blend(below, above, 1.0, BlendMode::normal).value, above.value);
    EXPECT_EQ(blend(below, above, 1.0, BlendMode::multiply).value, (std::array<std::uint8_t, 4>{78, 39, 39, 255}));
    EXPECT_EQ(blend(below, above, 1.0, BlendMode::screen).value, (std::array<std::uint8_t, 4>{222, 161, 211, 255}));
    EXPECT_EQ(blend(below, above, 1.0, BlendMode::difference).value, (std::array<std::uint8_t, 4>{100, 0, 150, 255}));
    EXPECT_EQ(blend(below, above, 1.0, BlendMode::lighten).value, (std::array<std::uint8_t, 4>{200, 100, 200, 255}));
    EXPECT_EQ(blend(below, above, 1.0, BlendMode::xor_).value,
              (std::array<std::uint8_t, 4>{200 ^ 100, 0, 50 ^ 200, 255}));
}

TEST(Blend, OverNothingALayerShowsAsItIsWhateverItsMode)
{
    const Rgba8 nothing{};
    const Rgba8 above{{100, 150, 200, 255}};
    for(const auto mode : all_blend_modes())
    {
        EXPECT_EQ(blend(nothing, above, 1.0, mode).value, above.value) << blend_mode_name(mode);
    }
}

TEST(Color, HexadecimalGoesBothWays)
{
    EXPECT_EQ(to_hex(Color{1, 0, 0, 1}), "#ff0000");
    EXPECT_EQ(to_hex(Color{0, 0.5, 1, 0.5}), "#0080ff80");
    const auto back{from_hex("#0080ff80")};
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(to_hex(*back), "#0080ff80");
    EXPECT_EQ(from_hex("00ff00")->green, 1.0);
    EXPECT_FALSE(from_hex("#12345").has_value());
    EXPECT_FALSE(from_hex("#zz0000").has_value());
}
