#include <oui/file_system.h>
#include <oui/folder_watcher.h>

#include <gtest/gtest.h>

#ifdef __linux__
#include <sys/inotify.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#endif

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

using namespace olagarro;

namespace
{

using Kind = oui::FolderChange::Kind;

// A folder of its own for each test and each way of watching, removed afterwards.
class FolderWatcherTests : public ::testing::TestWithParam<oui::FolderWatcher::Way>
{
  protected:
    void SetUp() override
    {
#ifdef __linux__
        // The system's way is only there when the system has an inotify instance to give, and each user has a few.
        if(GetParam() == oui::FolderWatcher::Way::system)
        {
            const auto descriptor{inotify_init1(IN_CLOEXEC)};
            if(descriptor < 0)
            {
                GTEST_SKIP() << "the system gives no inotify instance: " << std::strerror(errno);
            }
            ::close(descriptor);
        }
#endif
        auto name{std::string{"oui_folder_watcher_test_"} +
                  ::testing::UnitTest::GetInstance()->current_test_info()->name()};
        for(auto& character : name)
        {
            if(character == '/')
            {
                character = '_';
            }
        }
        folder = std::filesystem::temp_directory_path() / name;
        std::error_code ignored;
        std::filesystem::remove_all(folder, ignored);
        std::filesystem::create_directories(folder / "sub");
        write("kept.txt", "kept");
        write("sub/old.txt", "old");
    }

    void TearDown() override
    {
        std::error_code ignored;
        std::filesystem::remove_all(folder, ignored);
    }

    void write(const std::string& name, const std::string& text) const
    {
        auto* const file{oui::open_file(folder / oui::path_from_utf8(name), "wb")};
        ASSERT_NE(file, nullptr) << name;
        std::fwrite(text.data(), 1, text.size(), file);
        std::fclose(file);
    }

    void rename(const std::string& from, const std::string& to) const
    {
        std::filesystem::rename(folder / oui::path_from_utf8(from), folder / oui::path_from_utf8(to));
    }

    void remove(const std::string& name) const { std::filesystem::remove(folder / oui::path_from_utf8(name)); }

    std::filesystem::path folder;
};

oui::FolderChange change(Kind kind, std::string path, bool is_folder = false)
{
    return oui::FolderChange{kind, std::move(path), {}, is_folder};
}

oui::FolderChange renamed(std::string from, std::string to)
{
    return oui::FolderChange{Kind::renamed, std::move(to), std::move(from)};
}

std::string printed(const std::vector<oui::FolderChange>& changes)
{
    std::string text;
    for(const auto& each : changes)
    {
        const char* const kinds[]{"added", "removed", "changed", "renamed"};
        text += std::string{kinds[static_cast<int>(each.kind)]} + " " + each.path +
                (each.old_path.empty() ? "" : " from " + each.old_path) + (each.is_folder ? " (folder)" : "") + "\n";
    }
    return text;
}

} // namespace

TEST_P(FolderWatcherTests, WatchesTheWayItIsAsked)
{
    oui::FolderWatcher watcher{folder, GetParam()};
    EXPECT_EQ(watcher.folder(), folder);
#ifdef __linux__
    EXPECT_EQ(watcher.is_polling(), GetParam() == oui::FolderWatcher::Way::polling);
#else
    EXPECT_TRUE(watcher.is_polling());
#endif
    EXPECT_TRUE(watcher.changes().empty());
}

TEST_P(FolderWatcherTests, AFileAddedIsHeardOnce)
{
    oui::FolderWatcher watcher{folder, GetParam()};
    write("new.txt", "a new file, written after it was made");
    const auto heard{watcher.changes()};
    EXPECT_EQ(heard, (std::vector{change(Kind::added, "new.txt")})) << printed(heard);
    EXPECT_TRUE(watcher.changes().empty());
}

TEST_P(FolderWatcherTests, AFileWrittenToIsChangedOnce)
{
    oui::FolderWatcher watcher{folder, GetParam()};
    write("kept.txt", "written again, longer than before");
    const auto heard{watcher.changes()};
    EXPECT_EQ(heard, (std::vector{change(Kind::changed, "kept.txt")})) << printed(heard);
    EXPECT_TRUE(watcher.changes().empty());
}

TEST_P(FolderWatcherTests, AFileRemovedIsHeardOnce)
{
    oui::FolderWatcher watcher{folder, GetParam()};
    remove("kept.txt");
    const auto heard{watcher.changes()};
    EXPECT_EQ(heard, (std::vector{change(Kind::removed, "kept.txt")})) << printed(heard);
    EXPECT_TRUE(watcher.changes().empty());
}

TEST_P(FolderWatcherTests, AFileRenamedIsHeardOnce)
{
    oui::FolderWatcher watcher{folder, GetParam()};
    rename("kept.txt", "moved.txt");
    const auto heard{watcher.changes()};
    EXPECT_EQ(heard, (std::vector{renamed("kept.txt", "moved.txt")})) << printed(heard);
    EXPECT_TRUE(watcher.changes().empty());
}

TEST_P(FolderWatcherTests, AFileAddedAndRemovedBeforeItIsAskedIsNothing)
{
    oui::FolderWatcher watcher{folder, GetParam()};
    write("brief.txt", "here and gone");
    remove("brief.txt");
    const auto heard{watcher.changes()};
    EXPECT_TRUE(heard.empty()) << printed(heard);
}

TEST_P(FolderWatcherTests, ChangesInAFolderUnderItAreHeard)
{
    oui::FolderWatcher watcher{folder, GetParam()};
    write("sub/new.txt", "new");
    auto heard{watcher.changes()};
    EXPECT_EQ(heard, (std::vector{change(Kind::added, "sub/new.txt")})) << printed(heard);

    write("sub/old.txt", "old, written again");
    heard = watcher.changes();
    EXPECT_EQ(heard, (std::vector{change(Kind::changed, "sub/old.txt")})) << printed(heard);

    rename("sub/new.txt", "sub/renamed.txt");
    heard = watcher.changes();
    EXPECT_EQ(heard, (std::vector{renamed("sub/new.txt", "sub/renamed.txt")})) << printed(heard);

    remove("sub/renamed.txt");
    heard = watcher.changes();
    EXPECT_EQ(heard, (std::vector{change(Kind::removed, "sub/renamed.txt")})) << printed(heard);
}

TEST_P(FolderWatcherTests, AFolderMadeWhileWatchingIsWatchedToo)
{
    oui::FolderWatcher watcher{folder, GetParam()};
    std::filesystem::create_directories(folder / "made" / "deeper");
    write("made/deeper/first.txt", "first");
    auto heard{watcher.changes()};
    EXPECT_EQ(heard, (std::vector{change(Kind::added, "made", true), change(Kind::added, "made/deeper", true),
                                  change(Kind::added, "made/deeper/first.txt")}))
        << printed(heard);

    write("made/deeper/first.txt", "first, written again");
    write("made/deeper/second.txt", "second");
    heard = watcher.changes();
    EXPECT_EQ(heard, (std::vector{change(Kind::changed, "made/deeper/first.txt"),
                                  change(Kind::added, "made/deeper/second.txt")}))
        << printed(heard);
}

TEST_P(FolderWatcherTests, AFolderRemovedIsHeardWithWhatItHeld)
{
    oui::FolderWatcher watcher{folder, GetParam()};
    std::filesystem::remove_all(folder / "sub");
    const auto heard{watcher.changes()};
    EXPECT_EQ(heard, (std::vector{change(Kind::removed, "sub/old.txt"), change(Kind::removed, "sub", true)}))
        << printed(heard);
}

TEST_P(FolderWatcherTests, AFileWrittenUnderAnotherNameAndRenamedIntoPlaceIsAdded)
{
    oui::FolderWatcher watcher{folder, GetParam()};
    write("film.mp4.part", "the whole film");
    rename("film.mp4.part", "film.mp4");
    const auto heard{watcher.changes()};
    EXPECT_EQ(heard, (std::vector{change(Kind::added, "film.mp4")})) << printed(heard);
}

TEST_P(FolderWatcherTests, AFilePutInAnothersPlaceIsAChange)
{
    oui::FolderWatcher watcher{folder, GetParam()};
    write("kept.txt.new", "saved again, as editors save");
    rename("kept.txt.new", "kept.txt");
    const auto heard{watcher.changes()};
    EXPECT_EQ(heard, (std::vector{change(Kind::changed, "kept.txt")})) << printed(heard);
}

TEST_P(FolderWatcherTests, AFolderMovedOutIsRemovedWithWhatItHeld)
{
    oui::FolderWatcher watcher{folder, GetParam()};
    const auto outside{folder.parent_path() / (folder.filename().string() + "_outside")};
    std::error_code ignored;
    std::filesystem::remove_all(outside, ignored);
    std::filesystem::rename(folder / "sub", outside);
    const auto heard{watcher.changes()};
    std::filesystem::remove_all(outside, ignored);
    EXPECT_EQ(heard, (std::vector{change(Kind::removed, "sub/old.txt"), change(Kind::removed, "sub", true)}))
        << printed(heard);
    // Nothing more is heard from it where it went.
    EXPECT_TRUE(watcher.changes().empty());
}

TEST_P(FolderWatcherTests, AFolderRenamedTakesWhatItHoldsAlong)
{
    oui::FolderWatcher watcher{folder, GetParam()};
    rename("sub", "renamed");
    auto heard{watcher.changes()};
    if(watcher.is_polling())
    {
        EXPECT_EQ(heard, (std::vector{change(Kind::removed, "sub", true), renamed("sub/old.txt", "renamed/old.txt"),
                                      change(Kind::added, "renamed", true)}))
            << printed(heard);
    }
    else
    {
        EXPECT_EQ(heard, (std::vector{oui::FolderChange{Kind::renamed, "renamed", "sub", true}})) << printed(heard);
    }
    write("renamed/old.txt", "written where it is now");
    heard = watcher.changes();
    EXPECT_EQ(heard, (std::vector{change(Kind::changed, "renamed/old.txt")})) << printed(heard);
}

INSTANTIATE_TEST_SUITE_P(Ways, FolderWatcherTests,
                         ::testing::Values(oui::FolderWatcher::Way::system, oui::FolderWatcher::Way::polling),
                         [](const auto& info)
                         { return info.param == oui::FolderWatcher::Way::system ? "System" : "Polling"; });
