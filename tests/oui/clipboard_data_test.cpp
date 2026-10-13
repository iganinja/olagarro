#include <oui/clipboard.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <string>

using namespace olagarro;

TEST(OuiClipboardDataTests, BytesOfATypeGoInAndComeBackWhole)
{
    oui::Clipboard clipboard;
    const std::string bytes{"\x89PNG\r\n\x1a\n\0\0\0\rIHDR binary", 25};
    ASSERT_TRUE(clipboard.set_data("image/png", bytes));
    const auto types{clipboard.types()};
    EXPECT_NE(std::find(types.begin(), types.end(), "image/png"), types.end());
    const auto back{clipboard.data("image/png")};
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(*back, bytes);
    EXPECT_FALSE(clipboard.data("image/x-no-such-type").has_value());
}

TEST(OuiClipboardDataTests, ATypeThatIsNotOneIsRefused)
{
    oui::Clipboard clipboard;
    EXPECT_FALSE(clipboard.set_data("image/png; touch /tmp/oui_clipboard_injection", "x"));
    EXPECT_FALSE(clipboard.data("$(true)").has_value());
}

TEST(OuiClipboardDataTests, SeveralTypesOfTheSameDataGoTogetherAndTheFirstIsAlwaysThere)
{
    oui::Clipboard clipboard;
    const std::string png{"\x89PNG\r\n\x1a\n\0\0\0\rIHDR png", 21};
    // A BMP file of one pixel: its 14 bytes of file header, its 40 of information, and the pixel.
    std::string bmp{"BM", 2};
    const auto put32{[&bmp](std::uint32_t value)
                     {
                         for(int shift{}; shift < 32; shift += 8)
                         {
                             bmp += static_cast<char>((value >> shift) & 0xffu);
                         }
                     }};
    const auto put16{[&bmp](std::uint16_t value)
                     {
                         bmp += static_cast<char>(value & 0xffu);
                         bmp += static_cast<char>(value >> 8u);
                     }};
    put32(14 + 40 + 4);
    put32(0);
    put32(14 + 40);
    put32(40);
    put32(1);
    put32(1);
    put16(1);
    put16(24);
    put32(0);
    put32(4);
    put32(2835);
    put32(2835);
    put32(0);
    put32(0);
    bmp += std::string{"\x10\x20\x30\x00", 4};
    ASSERT_TRUE(clipboard.set_data({{"image/png", png}, {"image/bmp", bmp}}));
    const auto back{clipboard.data("image/png")};
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(*back, png);
#ifdef _WIN32
    // Windows keeps both, the BMP as a device-independent bitmap, which comes back a BMP file as it went.
    const auto bitmap{clipboard.data("image/bmp")};
    ASSERT_TRUE(bitmap.has_value());
    EXPECT_EQ(*bitmap, bmp);
#endif
}
