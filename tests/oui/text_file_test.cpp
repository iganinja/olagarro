#include <oui/text_file.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

using namespace olagarro;

namespace
{

using oui::LineEnding;
using oui::TextEncoding;
using oui::TextFormat;

std::string file_bytes(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

void write_bytes(const std::filesystem::path& path, const std::string& bytes)
{
    std::ofstream{path, std::ios::binary} << bytes;
}

TEST(OuiTextFileTests, DecodesPlainUtf8WithLfUnchanged)
{
    const auto decoded{oui::decode_text("uno\ndós\n")};
    EXPECT_EQ(decoded.text, "uno\ndós\n");
    EXPECT_EQ(decoded.format.encoding, TextEncoding::utf8);
    EXPECT_EQ(decoded.format.line_ending, LineEnding::lf);
}

TEST(OuiTextFileTests, EditsCrlfTextWithLfAndRestoresItOnSave)
{
    const auto decoded{oui::decode_text("uno\r\ndos\r\n")};
    EXPECT_EQ(decoded.text, "uno\ndos\n");
    EXPECT_EQ(decoded.format.line_ending, LineEnding::crlf);
    EXPECT_EQ(oui::encode_text("uno\ndos\ntres\n", decoded.format), "uno\r\ndos\r\ntres\r\n");
}

TEST(OuiTextFileTests, TheMostCommonLineEndingWins)
{
    EXPECT_EQ(oui::decode_text("a\r\nb\r\nc\n").format.line_ending, LineEnding::crlf);
    const auto mostly_lf{oui::decode_text("a\nb\nc\r\n")};
    EXPECT_EQ(mostly_lf.format.line_ending, LineEnding::lf);
    // The odd CRLF of an LF file stays as it was, so saving changes nothing the user did not touch.
    EXPECT_EQ(oui::encode_text(mostly_lf.text, mostly_lf.format), "a\nb\nc\r\n");
}

TEST(OuiTextFileTests, KeepsTheUtf8ByteOrderMark)
{
    const auto decoded{oui::decode_text("\xEF\xBB\xBFtexto\n")};
    EXPECT_EQ(decoded.text, "texto\n");
    EXPECT_EQ(decoded.format.encoding, TextEncoding::utf8_bom);
    EXPECT_EQ(oui::encode_text(decoded.text, decoded.format), "\xEF\xBB\xBFtexto\n");
}

TEST(OuiTextFileTests, ReadsInvalidUtf8AsWindows1252)
{
    const auto decoded{oui::decode_text("caf\xE9 \x80 \x93x\x94\r\n")};
    EXPECT_EQ(decoded.format.encoding, TextEncoding::windows_1252);
    EXPECT_EQ(decoded.format.line_ending, LineEnding::crlf);
    EXPECT_EQ(decoded.text, "café € “x”\n");
}

TEST(OuiTextFileTests, Windows1252RoundTripsEveryByte)
{
    std::string bytes;
    for(auto value{0x80}; value <= 0xFF; ++ value)
    {
        bytes += static_cast<char>(value);
    }
    const auto decoded{oui::decode_text(bytes)};
    ASSERT_EQ(decoded.format.encoding, TextEncoding::windows_1252);
    EXPECT_EQ(oui::encode_text(decoded.text, decoded.format), bytes);
}

TEST(OuiTextFileTests, RefusesCharactersTheEncodingCannotHold)
{
    const TextFormat latin{TextEncoding::windows_1252, LineEnding::lf};
    EXPECT_FALSE(oui::encode_text("日本", latin));
    EXPECT_EQ(oui::encode_text("año €", latin), "a\xF1o \x80");
}

TEST(OuiTextFileTests, RejectsOverlongAndSurrogateUtf8)
{
    EXPECT_EQ(oui::decode_text("\xC0\xAF").format.encoding, TextEncoding::windows_1252);
    EXPECT_EQ(oui::decode_text("\xED\xA0\x80").format.encoding, TextEncoding::windows_1252);
}

class OuiTextFileDiskTests : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // One folder per test: ctest runs the tests of this fixture in parallel processes.
        m_folder =
            std::filesystem::temp_directory_path() /
            (std::string{"oui_text_file_test_"} + ::testing::UnitTest::GetInstance()->current_test_info()->name());
        std::filesystem::remove_all(m_folder);
        std::filesystem::create_directories(m_folder);
    }

    void TearDown() override
    {
        std::error_code ignored;
        std::filesystem::permissions(m_folder, std::filesystem::perms::owner_all, std::filesystem::perm_options::add,
                                     ignored);
        std::filesystem::remove_all(m_folder, ignored);
    }

    std::filesystem::path m_folder;
};

TEST_F(OuiTextFileDiskTests, SavesInTheFileFormatAndLoadsItBack)
{
    const auto path{m_folder / "windows.txt"};
    oui::FileError error;
    const TextFormat format{TextEncoding::windows_1252, LineEnding::crlf};
    ASSERT_TRUE(oui::save_text_file(path, "año\nuno\n", format, error)) << oui::describe(error);
    EXPECT_EQ(file_bytes(path), "a\xF1o\r\nuno\r\n");

    const auto loaded{oui::load_text_file(path, error)};
    ASSERT_TRUE(loaded) << oui::describe(error);
    EXPECT_EQ(loaded->content.text, "año\nuno\n");
    EXPECT_EQ(loaded->content.format.encoding, TextEncoding::windows_1252);
    EXPECT_EQ(loaded->content.format.line_ending, LineEnding::crlf);
}

TEST_F(OuiTextFileDiskTests, ReplacesTheFileWithoutLeavingTemporaries)
{
    const auto path{m_folder / "notes.txt"};
    write_bytes(path, "old contents\n");
    oui::FileError error;
    ASSERT_TRUE(oui::save_text_file(path, "new\n", {}, error)) << oui::describe(error);
    EXPECT_EQ(file_bytes(path), "new\n");
    auto files{0};
    for([[maybe_unused]] const auto& entry : std::filesystem::directory_iterator{m_folder})
    {
        ++ files;
    }
    EXPECT_EQ(files, 1);
}

TEST_F(OuiTextFileDiskTests, ReportsWhyItCouldNotLoadOrSave)
{
    oui::FileError error;
    EXPECT_FALSE(oui::load_text_file(m_folder / "missing.txt", error));
    EXPECT_EQ(error.kind, oui::FileError::Kind::not_found);
    EXPECT_EQ(oui::describe(error), "the file does not exist");

    error = {};
    EXPECT_FALSE(oui::save_text_file(m_folder / "no_such_folder" / "a.txt", "x", {}, error));
    EXPECT_EQ(error.kind, oui::FileError::Kind::system);
    EXPECT_FALSE(error.system_message.empty());

    error = {};
    EXPECT_FALSE(oui::save_text_file(m_folder / "latin.txt", "日本", {TextEncoding::windows_1252, {}}, error));
    EXPECT_EQ(error.kind, oui::FileError::Kind::not_encodable);
    EXPECT_EQ(error.encoding, TextEncoding::windows_1252);
    EXPECT_FALSE(std::filesystem::exists(m_folder / "latin.txt"));
}

TEST_F(OuiTextFileDiskTests, AFailedSaveLeavesTheOriginalIntact)
{
    const auto path{m_folder / "keep.txt"};
    write_bytes(path, "precious\n");
    std::filesystem::permissions(m_folder, std::filesystem::perms::owner_write, std::filesystem::perm_options::remove);
    oui::FileError error;
    const auto saved{oui::save_text_file(path, "lost\n", {}, error)};
    std::filesystem::permissions(m_folder, std::filesystem::perms::owner_write, std::filesystem::perm_options::add);
    if(saved)
    {
        GTEST_SKIP() << "the folder stayed writable (running as root?)";
    }
    EXPECT_EQ(error.kind, oui::FileError::Kind::system);
    EXPECT_EQ(file_bytes(path), "precious\n");
}

#ifndef _WIN32
TEST_F(OuiTextFileDiskTests, SavingKeepsSymbolicLinksAndPermissions)
{
    const auto target{m_folder / "real.txt"};
    const auto link{m_folder / "link.txt"};
    write_bytes(target, "before\n");
    std::filesystem::permissions(target, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write |
                                             std::filesystem::perms::group_read);
    std::filesystem::create_symlink(target, link);
    oui::FileError error;
    ASSERT_TRUE(oui::save_text_file(link, "after\n", {}, error)) << oui::describe(error);
    EXPECT_TRUE(std::filesystem::is_symlink(link));
    EXPECT_EQ(file_bytes(target), "after\n");
    EXPECT_EQ(std::filesystem::status(target).permissions(), std::filesystem::perms::owner_read |
                                                                 std::filesystem::perms::owner_write |
                                                                 std::filesystem::perms::group_read);
}
#endif

TEST_F(OuiTextFileDiskTests, TheStampChangesWhenTheFileDoes)
{
    const auto path{m_folder / "stamped.txt"};
    oui::FileError error;
    const auto first{oui::save_text_file(path, "one\n", {}, error)};
    ASSERT_TRUE(first) << oui::describe(error);
    EXPECT_EQ(oui::file_stamp(path), first);
    write_bytes(path, "changed by someone else\n");
    EXPECT_NE(oui::file_stamp(path), first);
}

} // namespace
