#include <paint/palette.h>

#include <gtest/gtest.h>

#include <string>

using namespace olagarro;

namespace
{

[[nodiscard]] paint::Color bytes(int red, int green, int blue, int alpha = 255)
{
    return {red / 255.0, green / 255.0, blue / 255.0, alpha / 255.0};
}

} // namespace

TEST(PaintPaletteTests, APaletteWrittenAsGplAndReadBackIsTheSameSeeThroughIncluded)
{
    const paint::Palette palette{"Sunset",
                                 {bytes(255, 128, 0), bytes(0, 0, 0, 0), bytes(12, 34, 56), bytes(1, 2, 3, 128)}};
    const auto text{paint::to_gpl(palette)};
    EXPECT_EQ(text.rfind("GIMP Palette\nName: Sunset\n", 0), 0u) << text;
    EXPECT_NE(text.find("  0   0   0\ttransparent\n"), std::string::npos) << "black named transparent: " << text;
    const auto back{paint::palette_from_gpl(text)};
    ASSERT_TRUE(back);
    EXPECT_EQ(*back, palette);
}

TEST(PaintPaletteTests, GimpsOwnPalettesAreReadAndOtherTextIsNot)
{
    const auto gimp{paint::palette_from_gpl("GIMP Palette\r\nName: Bears\r\nColumns: 4\r\n#\r\n"
                                            "# a comment\r\n"
                                            "  8  16  24\tBrown\r\n"
                                            "255 255 255\r\n"
                                            "\r\n")};
    ASSERT_TRUE(gimp);
    EXPECT_EQ(gimp->name, "Bears");
    EXPECT_EQ(gimp->colors, (std::vector<paint::Color>{bytes(8, 16, 24), bytes(255, 255, 255)}));
    EXPECT_FALSE(paint::palette_from_gpl("JASC-PAL\n0100\n1\n1 2 3\n"));
    EXPECT_FALSE(paint::palette_from_gpl(""));
}

TEST(PaintPaletteTests, TheStandardPaletteIsTheGreysAndColoursNamedDefault)
{
    const auto standard{paint::Palette::standard()};
    EXPECT_EQ(standard.name, "Default");
    ASSERT_EQ(standard.colors.size(), 26u);
    EXPECT_EQ(standard.colors.front(), (paint::Color{0, 0, 0, 1}));
    EXPECT_EQ(standard.colors[8], (paint::Color{1, 1, 1, 1}));
    EXPECT_EQ(standard.colors[9].alpha, 0.0) << "see-through after the greys";
}
