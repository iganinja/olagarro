#include <oui/file_system.h>
#include <oui/text_file.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <string>

using namespace olagarro;

namespace
{

class FilePickerTests : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        test_dir =
            std::filesystem::temp_directory_path() /
            (std::string{"oui_filepicker_test_"} + ::testing::UnitTest::GetInstance()->current_test_info()->name());
        std::filesystem::create_directories(test_dir);

        std::filesystem::create_directories(test_dir / "subdir1");
        std::filesystem::create_directories(test_dir / "subdir2");

        std::ofstream{test_dir / "file1.txt"} << "hello";
        std::ofstream{test_dir / "file2.txt"} << "world";
        std::ofstream{test_dir / "file3.cpp"} << "#include";
        std::ofstream{test_dir / "file4.hpp"} << "// header";
        std::ofstream{test_dir / "file5.log"} << "log data";
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(test_dir, ec);
    }

    std::filesystem::path test_dir;
};

TEST_F(FilePickerTests, DirectoryEntriesReturnsFiles)
{
    auto entries{oui::directory_entries(test_dir)};
    ASSERT_GE(entries.size(), 5u);

    bool has_subdir1{false};
    bool has_subdir2{false};
    for(const auto& e : entries)
    {
        if(e.path.filename() == "subdir1")
            has_subdir1 = true;
        if(e.path.filename() == "subdir2")
            has_subdir2 = true;
    }
    ASSERT_TRUE(has_subdir1);
    ASSERT_TRUE(has_subdir2);
}

TEST_F(FilePickerTests, EntriesSortedDirectoriesFirst)
{
    auto entries{oui::directory_entries(test_dir)};
    ASSERT_FALSE(entries.empty());
    ASSERT_TRUE(entries[0].is_directory);
}

TEST_F(FilePickerTests, EntriesIncludesAllFiles)
{
    auto entries{oui::directory_entries(test_dir)};
    int file_count{0};
    int dir_count{0};
    for(const auto& e : entries)
    {
        if(e.is_directory)
            dir_count++;
        else
            file_count++;
    }
    EXPECT_EQ(file_count, 5);
    EXPECT_EQ(dir_count, 2);
}

TEST_F(FilePickerTests, EmptyDirectoryReturnsEmpty)
{
    auto empty_dir{test_dir / "empty"};
    std::filesystem::create_directories(empty_dir);
    auto entries{oui::directory_entries(empty_dir)};
    EXPECT_TRUE(entries.empty());
}

TEST_F(FilePickerTests, RootDirectoriesReturnsDrives)
{
    auto roots{oui::root_directories()};
    ASSERT_FALSE(roots.empty());
    for(const auto& r : roots)
    {
        auto entries{oui::directory_entries(r)};
        (void)entries.size();
    }
}

TEST_F(FilePickerTests, ReadTextFile)
{
    oui::FileError error;
    auto result{oui::load_text_file(test_dir / "file1.txt", error)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->content.text, "hello");
}

TEST_F(FilePickerTests, ReadNonExistentFile)
{
    oui::FileError error;
    auto result{oui::load_text_file(test_dir / "does_not_exist.txt", error)};
    EXPECT_FALSE(result.has_value());
}

TEST_F(FilePickerTests, WriteReadTextFile)
{
    const auto file_path{test_dir / "write_test.txt"};
    const std::string content{"test content"};
    oui::FileError error;
    EXPECT_TRUE(oui::save_text_file(file_path, content, {}, error));
    auto result{oui::load_text_file(file_path, error)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->content.text, content);
}

TEST_F(FilePickerTests, ParentPathNavigation)
{
    auto subdir{test_dir / "subdir1"};
    auto parent{subdir.parent_path()};
    EXPECT_EQ(parent, test_dir);
}

TEST_F(FilePickerTests, FilterTxtFiles)
{
    auto entries{oui::directory_entries(test_dir)};
    int txt_count{0};
    for(const auto& e : entries)
    {
        if(!e.is_directory && e.path.extension().string() == ".txt")
        {
            txt_count++;
        }
    }
    EXPECT_EQ(txt_count, 2);
}

TEST_F(FilePickerTests, AllFilesExistAndReadable)
{
    const std::vector<std::string> expected{"file1.txt", "file2.txt", "file3.cpp", "file4.hpp", "file5.log"};
    for(const auto& name : expected)
    {
        const auto path{test_dir / name};
        ASSERT_TRUE(std::filesystem::exists(path));
        oui::FileError error;
        auto content{oui::load_text_file(path, error)};
        ASSERT_TRUE(content.has_value());
    }
}

#ifndef _WIN32
TEST(OuiKnownFolderTests, FollowTheUsersLocalisedFolders)
{
    const auto home{std::filesystem::temp_directory_path() / "oui_known_folders_home"};
    std::filesystem::remove_all(home);
    std::filesystem::create_directories(home / ".config");
    std::filesystem::create_directories(home / "Escritorio");
    std::filesystem::create_directories(home / "Downloads");
    std::ofstream{home / ".config" / "user-dirs.dirs"} << "# written by xdg-user-dirs-update\n"
                                                          "XDG_DESKTOP_DIR=\"$HOME/Escritorio\"\n"
                                                          "XDG_DOCUMENTS_DIR=\"$HOME/Documentos\"\n";
    setenv("HOME", home.c_str(), 1);
    unsetenv("XDG_CONFIG_HOME");
    unsetenv("XDG_DESKTOP_DIR");
    unsetenv("XDG_DOCUMENTS_DIR");
    unsetenv("XDG_DOWNLOAD_DIR");

    EXPECT_EQ(oui::known_folder("home"), home);
    EXPECT_EQ(oui::known_folder("desktop"), home / "Escritorio");
    EXPECT_EQ(oui::known_folder("documents"), std::filesystem::path{}) << "listed but missing on disk";
    EXPECT_EQ(oui::known_folder("downloads"), home / "Downloads") << "the English default";
    std::filesystem::remove_all(home);
}
#endif

TEST(OuiPathTextTests, UtfEightTextComesBackAsItWent)
{
    const std::string text{"ñandú/€ uno/𝄞"};
    EXPECT_EQ(oui::utf8_of(oui::path_from_utf8(text)), text);
}

TEST(OuiPathTextTests, TextSaysSlashOnEverySystemAndThePathTheSystemsOwn)
{
    // As text a path is the same everywhere, for what is shown, kept and given to scripts; as a path, the system's.
    EXPECT_EQ(oui::utf8_of(oui::path_from_utf8("a/b")), "a/b");
    EXPECT_EQ(oui::utf8_of(std::filesystem::path{"a"} / "b"), "a/b");
#ifdef _WIN32
    EXPECT_EQ(oui::utf8_of(std::filesystem::path{L"C:\\uno\\dos"}), "C:/uno/dos");
    EXPECT_EQ(oui::path_from_utf8("C:/uno/dos").native(), L"C:\\uno\\dos");
    EXPECT_EQ(oui::path_from_utf8("C:\\uno/dos").native(), L"C:\\uno\\dos") << "written either way";
#endif
}

TEST(OuiPathTextTests, NormalPathDropsWhatGoesNowhere)
{
    using oui::normal_path;
    using oui::path_from_utf8;
    EXPECT_EQ(normal_path(path_from_utf8("/a/./b/../c/")), path_from_utf8("/a/c").make_preferred());
    EXPECT_EQ(normal_path(path_from_utf8("a/../..")), path_from_utf8(".."));
    EXPECT_EQ(normal_path(path_from_utf8("/../a")), path_from_utf8("/a").make_preferred());
    EXPECT_EQ(normal_path(path_from_utf8("a/..")), path_from_utf8("."));
    EXPECT_EQ(normal_path(path_from_utf8("/")), path_from_utf8("/").make_preferred());
    EXPECT_TRUE(normal_path({}).empty());
}

TEST(OuiPathTextTests, RelativeToAgreesWithTheStandardLibrary)
{
    const std::vector<std::pair<std::string, std::string>> cases{
        {"/a/b/c", "/a/b"}, {"/a/b", "/a/b/c"}, {"/a/b", "/a/b"},    {"/a/d", "/a/b/c"}, {"a/b", "/a"},
        {"a", "a/b/.."},    {"/a/b/", "/a"},    {"/a/b", "/a/./b/"}, {"a/b", "c/d"},     {"/", "/a"}};
    for(const auto& [path, base] : cases)
    {
        const auto given{oui::path_from_utf8(path)};
        const auto from{oui::path_from_utf8(base)};
        EXPECT_EQ(oui::relative_to(given, from), given.lexically_relative(from)) << path << " from " << base;
    }
}

TEST_F(FilePickerTests, ListDirectoryTellsFoldersFromFiles)
{
    std::error_code error;
    const auto entries{oui::list_directory(test_dir, error)};
    EXPECT_FALSE(error);
    EXPECT_EQ(entries.size(), 7u);
    for(const auto& entry : entries)
    {
        EXPECT_EQ(entry.is_directory, std::filesystem::is_directory(entry.path)) << entry.path;
        EXPECT_FALSE(entry.is_link) << entry.path;
    }
    static_cast<void>(oui::list_directory(test_dir / "missing", error));
    EXPECT_TRUE(error);
}

TEST_F(FilePickerTests, CopyEntryCopiesAFolderOverWhatIsThere)
{
    std::ofstream{test_dir / "subdir1" / "inner.txt"} << "inside";
    std::filesystem::create_directories(test_dir / "copy");
    std::ofstream{test_dir / "copy" / "inner.txt"} << "old";
    std::error_code error;
    oui::copy_entry(test_dir / "subdir1", test_dir / "copy", error);
    EXPECT_FALSE(error);
    EXPECT_EQ(oui::read_file(test_dir / "copy" / "inner.txt"), std::optional<std::string>{"inside"});
    oui::copy_entry(test_dir / "file1.txt", test_dir / "file2.txt", error);
    EXPECT_FALSE(error);
    EXPECT_EQ(oui::read_file(test_dir / "file2.txt"), std::optional<std::string>{"hello"});
}

TEST_F(FilePickerTests, RemoveEntryTakesAFolderWithEverythingInIt)
{
    std::filesystem::create_directories(test_dir / "subdir1" / "deeper");
    std::ofstream{test_dir / "subdir1" / "deeper" / "inner.txt"} << "inside";
    std::error_code error;
    oui::remove_entry(test_dir / "subdir1", error);
    EXPECT_FALSE(error);
    EXPECT_FALSE(std::filesystem::exists(test_dir / "subdir1"));
    oui::remove_entry(test_dir / "subdir1", error);
    EXPECT_FALSE(error) << "nothing there is no error";
}

#ifndef _WIN32
TEST_F(FilePickerTests, RemoveEntryTakesALinkAndNotWhatItLeadsTo)
{
    std::ofstream{test_dir / "subdir1" / "kept.txt"} << "kept";
    std::filesystem::create_directory_symlink(test_dir / "subdir1", test_dir / "link");
    std::error_code error;
    const auto entries{oui::list_directory(test_dir, error)};
    const auto link{std::find_if(entries.begin(), entries.end(),
                                 [this](const auto& entry) { return entry.path == test_dir / "link"; })};
    ASSERT_NE(link, entries.end());
    EXPECT_TRUE(link->is_directory);
    EXPECT_TRUE(link->is_link);
    oui::remove_entry(test_dir / "link", error);
    EXPECT_FALSE(error);
    EXPECT_FALSE(std::filesystem::exists(test_dir / "link"));
    EXPECT_TRUE(std::filesystem::exists(test_dir / "subdir1" / "kept.txt"));
}
#endif

} // namespace
