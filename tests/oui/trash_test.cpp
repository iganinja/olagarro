#include <oui/file_system.h>

#include <gtest/gtest.h>

#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

using namespace olagarro;

namespace
{

// A folder for the files a test trashes, and a data home of its own whose Trash is the one they go to: the person's
// own trash is never touched.
class TrashTests : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        const std::string name{::testing::UnitTest::GetInstance()->current_test_info()->name()};
        folder = std::filesystem::temp_directory_path() / ("oui_trash_test_" + name);
        data = std::filesystem::temp_directory_path() / ("oui_trash_test_data_" + name);
        std::error_code ignored;
        std::filesystem::remove_all(folder, ignored);
        std::filesystem::remove_all(data, ignored);
        std::filesystem::create_directories(folder);
        const auto* const previous{std::getenv("XDG_DATA_HOME")};
        if(previous != nullptr)
        {
            previous_data_home = previous;
        }
        set_data_home(data);
    }

    void TearDown() override
    {
        if(previous_data_home)
        {
            setenv("XDG_DATA_HOME", previous_data_home->c_str(), 1);
        }
        else
        {
            unsetenv("XDG_DATA_HOME");
        }
        std::error_code ignored;
        std::filesystem::remove_all(folder, ignored);
        std::filesystem::remove_all(data, ignored);
    }

    static void set_data_home(const std::filesystem::path& path) { setenv("XDG_DATA_HOME", path.c_str(), 1); }

    static void write(const std::filesystem::path& path, const std::string& text)
    {
        auto* const file{oui::open_file(path, "wb")};
        ASSERT_NE(file, nullptr) << path;
        std::fwrite(text.data(), 1, text.size(), file);
        std::fclose(file);
    }

    std::filesystem::path folder;
    std::filesystem::path data;
    std::optional<std::string> previous_data_home;
};

// The lines of a .trashinfo, checked as the freedesktop.org trash specification has them.
void expect_trash_info(const std::filesystem::path& info, const std::string& path)
{
    const auto text{oui::read_file(info)};
    ASSERT_TRUE(text.has_value()) << info;
    const std::string start{"[Trash Info]\nPath=" + path + "\nDeletionDate="};
    ASSERT_EQ(text->compare(0, start.size(), start), 0) << *text;
    const auto date{text->substr(start.size())};
    // YYYY-MM-DDThh:mm:ss, and the end of the line.
    ASSERT_EQ(date.size(), 20u) << date;
    for(std::size_t index{}; index < 19; ++ index)
    {
        const auto character{date[index]};
        if(index == 4 || index == 7)
        {
            EXPECT_EQ(character, '-') << date;
        }
        else if(index == 10)
        {
            EXPECT_EQ(character, 'T') << date;
        }
        else if(index == 13 || index == 16)
        {
            EXPECT_EQ(character, ':') << date;
        }
        else
        {
            EXPECT_TRUE(character >= '0' && character <= '9') << date;
        }
    }
    EXPECT_EQ(date.back(), '\n');
}

} // namespace

TEST_F(TrashTests, AFileMovedToTheTrashIsGoneAndListedThereWithItsOriginalPath)
{
    const auto file{folder / "a b%é.txt"};
    write(file, "kept for later");
    std::error_code error;
    oui::move_to_trash(file, error);
    ASSERT_FALSE(error) << error.message();

    EXPECT_FALSE(std::filesystem::exists(file));
    const auto trashed{data / "Trash" / "files" / "a b%é.txt"};
    EXPECT_EQ(oui::read_file(trashed).value_or(""), "kept for later");
    // The path URL-escaped: the space, the percent sign and each byte of the é.
    const auto escaped{oui::utf8_of(folder) + "/a%20b%25%C3%A9.txt"};
    expect_trash_info(data / "Trash" / "info" / "a b%é.txt.trashinfo", escaped);
}

TEST_F(TrashTests, AFileOfANameAlreadyInTheTrashGetsAnotherThere)
{
    for(const auto* const text : {"first", "second"})
    {
        write(folder / "notes.txt", text);
        std::error_code error;
        oui::move_to_trash(folder / "notes.txt", error);
        ASSERT_FALSE(error) << error.message();
    }
    const auto files{data / "Trash" / "files"};
    EXPECT_EQ(oui::read_file(files / "notes.txt").value_or(""), "first");
    EXPECT_EQ(oui::read_file(files / "notes.2.txt").value_or(""), "second");
    expect_trash_info(data / "Trash" / "info" / "notes.2.txt.trashinfo", oui::utf8_of(folder) + "/notes.txt");
}

TEST_F(TrashTests, AFolderGoesToTheTrashWithWhatItHolds)
{
    std::filesystem::create_directories(folder / "shots" / "day one");
    write(folder / "shots" / "day one" / "take.mp4", "pictures");
    std::error_code error;
    oui::move_to_trash(folder / "shots", error);
    ASSERT_FALSE(error) << error.message();
    EXPECT_FALSE(std::filesystem::exists(folder / "shots"));
    EXPECT_EQ(oui::read_file(data / "Trash" / "files" / "shots" / "day one" / "take.mp4").value_or(""), "pictures");
    expect_trash_info(data / "Trash" / "info" / "shots.trashinfo", oui::utf8_of(folder) + "/shots");
}

TEST_F(TrashTests, NothingThereIsAnError)
{
    std::error_code error;
    oui::move_to_trash(folder / "never there.txt", error);
    EXPECT_EQ(error, std::make_error_code(std::errc::no_such_file_or_directory)) << error.message();
    EXPECT_FALSE(std::filesystem::exists(data / "Trash" / "info" / "never there.txt.trashinfo"));
}

// A file on another drive than the home's trash goes to that drive's own trash, by its path from the drive's top.
TEST_F(TrashTests, AFileOnAnotherDriveGoesToThatDrivesTrash)
{
    // A data home on the drive the tests run from, which is the temporary folder's only where they are one drive.
    const auto other_data{std::filesystem::current_path() / "oui_trash_test_other_data"};
    std::error_code ignored;
    std::filesystem::remove_all(other_data, ignored);
    std::filesystem::create_directories(other_data);
    struct stat here
    {
    };
    struct stat there
    {
    };
    ASSERT_EQ(stat(other_data.c_str(), &here), 0);
    ASSERT_EQ(stat(folder.c_str(), &there), 0);
    if(here.st_dev == there.st_dev)
    {
        std::filesystem::remove_all(other_data, ignored);
        GTEST_SKIP() << "the temporary folder is on the same drive as the tests";
    }
    set_data_home(other_data);

    // The drive's top: the last folder up from the file that is still on its drive.
    auto top{folder};
    for(struct stat parent{}; top.has_parent_path() && top.parent_path() != top &&
                              stat(top.parent_path().c_str(), &parent) == 0 && parent.st_dev == there.st_dev;)
    {
        top = top.parent_path();
    }
    const auto trash{top / (".Trash-" + std::to_string(getuid()))};
    const auto made_trash{!std::filesystem::exists(trash)};
    const auto name{"oui_trash_test_" + std::to_string(getpid()) + ".txt"};
    write(folder / name, "far away");

    std::error_code error;
    oui::move_to_trash(folder / name, error);
    const auto in_trash{oui::read_file(trash / "files" / name)};
    const auto info{trash / "info" / (name + ".trashinfo")};
    const auto relative{oui::utf8_of(oui::relative_to(folder, top)) + "/" + name};
    const auto info_text{oui::read_file(info)};
    std::filesystem::remove(trash / "files" / name, ignored);
    std::filesystem::remove(info, ignored);
    if(made_trash)
    {
        std::filesystem::remove_all(trash, ignored);
    }
    std::filesystem::remove_all(other_data, ignored);

    ASSERT_FALSE(error) << error.message();
    EXPECT_FALSE(std::filesystem::exists(folder / name));
    EXPECT_EQ(in_trash.value_or(""), "far away");
    ASSERT_TRUE(info_text.has_value());
    EXPECT_NE(info_text->find("\nPath=" + relative + "\n"), std::string::npos) << *info_text;
    EXPECT_FALSE(std::filesystem::exists(other_data / "Trash" / "files" / name));
}
