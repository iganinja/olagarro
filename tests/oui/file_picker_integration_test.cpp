#include <oui/file_system.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>

using namespace olagarro;

namespace
{

// Replicate the exact logic from file_picker.cpp to test it in isolation

std::string extension_lower(const std::string& name)
{
    auto ext{std::filesystem::path{name}.extension().string()};
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

bool matches_filter(const std::string& name, const std::string& filter)
{
    if(filter.empty() || filter == "*" || filter == "*.*")
    {
        return true;
    }
    const auto extension{extension_lower(name)};
    std::size_t start{};
    while(start <= filter.size())
    {
        auto end{filter.find(';', start)};
        if(end == std::string::npos)
        {
            end = filter.size();
        }
        std::string token{filter.substr(start, end - start)};
        std::transform(token.begin(), token.end(), token.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if(!token.empty())
        {
            if(token == "*")
            {
                return true;
            }
            std::string token_extension{token};
            if(token_extension.compare(0, 2, "*.") == 0)
            {
                token_extension.erase(0, 1);
            }
            else if(token_extension.empty() || token_extension.front() != '.')
            {
                token_extension.insert(0, ".");
            }
            if(extension == token_extension)
            {
                return true;
            }
        }
        if(end == filter.size())
        {
            break;
        }
        start = end + 1;
    }
    return false;
}

struct FileEntry
{
    std::string name;
    std::uintmax_t size{};
    std::string type;
    bool is_directory{false};
    std::filesystem::path full_path;
};

// Exact replication of the file listing logic from refresh_listing
std::vector<FileEntry> build_file_listing(const std::filesystem::path& directory, const std::string& filter_extension,
                                          bool directories_only)
{
    std::error_code ec;
    if(!std::filesystem::is_directory(directory, ec))
    {
        return {}; // Not a directory - returns EMPTY
    }

    std::vector<FileEntry> entries;

    // Add ".." entry
    if(directory.has_parent_path())
    {
        entries.push_back({"..", 0, "", true, directory.parent_path()});
    }

    const std::string& filter{directories_only ? "" : filter_extension};

    for(const auto& de : oui::directory_entries(directory))
    {
        // This is the exact condition from refresh_listing:
        if(de.is_directory || matches_filter(de.path.filename().string(), filter))
        {
            FileEntry entry;
            entry.name = de.path.filename().string();
            entry.is_directory = de.is_directory;
            entry.full_path = de.path;
            if(!de.is_directory)
            {
                std::error_code ec2;
                entry.size = std::filesystem::file_size(de.path, ec2);
                entry.type = extension_lower(de.path.filename().string());
                if(entry.type.size() > 1)
                {
                    entry.type = entry.type.substr(1);
                }
            }
            entries.push_back(std::move(entry));
        }
    }

    std::sort(entries.begin(), entries.end(),
              [](const FileEntry& a, const FileEntry& b)
              {
                  if(a.name == "..")
                      return true;
                  if(b.name == "..")
                      return false;
                  if(a.is_directory != b.is_directory)
                      return a.is_directory;
                  return a.name < b.name;
              });

    if(directories_only)
    {
        std::vector<FileEntry> dirs;
        for(auto& entry : entries)
        {
            if(entry.is_directory || entry.name == "..")
            {
                dirs.push_back(std::move(entry));
            }
        }
        entries = std::move(dirs);
    }

    return entries;
}

class FilePickerIntegrationTests : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Use a known path that we control
        test_dir =
            std::filesystem::temp_directory_path() / (std::string{"oui_picker_integration_test_"} +
                                                      ::testing::UnitTest::GetInstance()->current_test_info()->name());

        std::filesystem::create_directories(test_dir);

        // Create the exact same structure as before
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

// CORE TEST: Replicates the exact refresh_listing flow
TEST_F(FilePickerIntegrationTests, BuildFileListingReturnsAllEntries)
{
    auto entries{build_file_listing(test_dir, "*", false)};

    // Should have ".." + 5 files + 2 directories = 8
    EXPECT_EQ(entries.size(), 8u) << "Expected 8 entries (.. + 5 files + 2 dirs), got " << entries.size();

    // First entry should be ".."
    ASSERT_FALSE(entries.empty());
    EXPECT_EQ(entries[0].name, "..") << "First entry should be '..'";
}

TEST_F(FilePickerIntegrationTests, BuildFileListingFiltersByExtension)
{
    auto entries{build_file_listing(test_dir, "*.txt", false)};

    // Directories remain visible so the user can navigate while filtering files.
    EXPECT_EQ(entries.size(), 5u) << "Expected .., two directories, and two .txt files for *.txt, got "
                                  << entries.size();

    for(const auto& e : entries)
    {
        if(e.name != "..")
        {
            EXPECT_TRUE(e.is_directory || (e.name.size() >= 4 && e.name.compare(e.name.size() - 4, 4, ".txt") == 0))
                << "Only directories and .txt files should appear, not: " << e.name;
        }
    }
}

TEST_F(FilePickerIntegrationTests, BuildFileListingFiltersCppFiles)
{
    auto entries{build_file_listing(test_dir, "*.cpp;*.hpp", false)};

    // Directories remain visible so the user can navigate while filtering files.
    EXPECT_EQ(entries.size(), 5u) << "Expected .., two directories, and two C++ files, got " << entries.size();
}

TEST_F(FilePickerIntegrationTests, BuildFileListingDirectoriesOnly)
{
    auto entries{build_file_listing(test_dir, "*", true)};

    // Should have ".." + 2 directories = 3
    EXPECT_EQ(entries.size(), 3u) << "Expected 3 entries for directories_only mode (.. + subdir1 + subdir2), got "
                                  << entries.size();

    for(const auto& e : entries)
    {
        EXPECT_TRUE(e.is_directory) << "All entries should be directories: " << e.name;
    }
}

// Test: navigating into a subdirectory
TEST_F(FilePickerIntegrationTests, NavigateIntoSubdirectory)
{
    auto entries{build_file_listing(test_dir, "*", false)};
    EXPECT_EQ(entries.size(), 8u);

    // Find subdir1 entry
    std::filesystem::path subdir_path;
    for(const auto& e : entries)
    {
        if(e.name == "subdir1")
        {
            subdir_path = e.full_path;
            break;
        }
    }

    ASSERT_FALSE(subdir_path.empty()) << "subdir1 should be found in entries";

    // Navigate into subdir1
    auto subdir_entries{build_file_listing(subdir_path, "*", false)};
    // subdir1 is empty, so only ".."
    EXPECT_EQ(subdir_entries.size(), 1u) << "Empty subdir should only show '..'";
    EXPECT_EQ(subdir_entries[0].name, "..");
}

TEST_F(FilePickerIntegrationTests, ParentNavigationFromSubdirectory)
{
    auto subdir{test_dir / "subdir1"};
    auto entries{build_file_listing(subdir, "*", false)};

    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].name, "..");
    EXPECT_EQ(entries[0].full_path, test_dir) << "Parent path of subdir1 should be the test_dir";
}

// Test: matches_filter edge cases
TEST_F(FilePickerIntegrationTests, MatchesFilterEmptyReturnsTrue)
{
    EXPECT_TRUE(matches_filter("anything.txt", ""));
    EXPECT_TRUE(matches_filter("anything.cpp", ""));
}

TEST_F(FilePickerIntegrationTests, MatchesFilterWildcardReturnsTrue)
{
    EXPECT_TRUE(matches_filter("anything.txt", "*"));
    EXPECT_TRUE(matches_filter("anything.cpp", "*"));
    EXPECT_TRUE(matches_filter("noext", "*"));
}

TEST_F(FilePickerIntegrationTests, MatchesFilterExactExtension)
{
    EXPECT_TRUE(matches_filter("file.txt", "*.txt"));
    EXPECT_TRUE(matches_filter("file.TXT", "*.txt")); // Case insensitive
    EXPECT_FALSE(matches_filter("file.cpp", "*.txt"));
    EXPECT_FALSE(matches_filter("file", "*.txt"));
}

TEST_F(FilePickerIntegrationTests, MatchesFilterMultipleExtensions)
{
    EXPECT_TRUE(matches_filter("file.cpp", "*.cpp;*.hpp"));
    EXPECT_TRUE(matches_filter("file.hpp", "*.cpp;*.hpp"));
    EXPECT_FALSE(matches_filter("file.txt", "*.cpp;*.hpp"));
}

TEST_F(FilePickerIntegrationTests, MatchesFilterDotPrefix)
{
    EXPECT_TRUE(matches_filter("file.txt", ".txt"));
    EXPECT_TRUE(matches_filter("file.txt", "*.*"));
}

// Test: non-existent directory returns empty
TEST_F(FilePickerIntegrationTests, NonExistentDirectoryReturnsEmpty)
{
    auto entries{build_file_listing(test_dir / "does_not_exist", "*", false)};
    EXPECT_TRUE(entries.empty()) << "Listing a non-existent directory should return empty, not crash";
}

// Test: current_path() on Windows — check that it's a valid directory
TEST_F(FilePickerIntegrationTests, CurrentPathIsValidDirectory)
{
    auto current{std::filesystem::current_path()};
    std::error_code ec;
    EXPECT_TRUE(std::filesystem::is_directory(current, ec))
        << "current_path() should be a valid directory, got: " << current.string();

    auto entries{build_file_listing(current, "*", false)};
    EXPECT_FALSE(entries.empty()) << "current_path() should have at least '..' entry, got " << entries.size();
}

// Test: the exact flow that choose_path uses
TEST_F(FilePickerIntegrationTests, ChoosePathFlowFromCurrentPath)
{
    // This mimics: auto current{directory.empty() ? std::filesystem::current_path() : directory};
    std::filesystem::path current{};
    if(current.empty())
    {
        current = std::filesystem::current_path();
    }

    std::error_code ec;
    ASSERT_TRUE(std::filesystem::is_directory(current, ec))
        << "The path used by choose_path must be a directory: " << current.string();

    std::vector<FileEntry> file_entries;
    // Simulate refresh_listing flow
    if(current.has_parent_path())
    {
        file_entries.push_back({"..", 0, "", true, current.parent_path()});
    }

    const std::string& filter{"*"};
    for(const auto& de : oui::directory_entries(current))
    {
        if(de.is_directory || matches_filter(de.path.filename().string(), filter))
        {
            FileEntry entry;
            entry.name = de.path.filename().string();
            entry.is_directory = de.is_directory;
            entry.full_path = de.path;
            file_entries.push_back(std::move(entry));
        }
    }

    EXPECT_FALSE(file_entries.empty()) << "choose_path should find files in current_path(), found "
                                       << file_entries.size() << " entries from: " << current.string();

    // First entry should be ".."
    EXPECT_EQ(file_entries[0].name, "..") << "First entry should be parent directory";
}

} // namespace
