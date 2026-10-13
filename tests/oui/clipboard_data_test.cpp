#include <oui/clipboard.h>

#include <gtest/gtest.h>

#include <algorithm>
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
