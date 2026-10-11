#include "ignore_rules.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace
{

TEST(OuiCodeIgnoreRulesTests, MatchesPatternsAsGitDoes)
{
    EXPECT_TRUE(gitignore_match("*.log", "debug.log"));
    EXPECT_FALSE(gitignore_match("*.log", "logs/debug.log")) << "'*' stays within a part";
    EXPECT_TRUE(gitignore_match("debug?.log", "debug1.log"));
    EXPECT_FALSE(gitignore_match("debug?.log", "debug/.log"));
    EXPECT_TRUE(gitignore_match("debug[0-9].log", "debug7.log"));
    EXPECT_FALSE(gitignore_match("debug[!0-9].log", "debug7.log"));
    EXPECT_TRUE(gitignore_match("**/logs", "a/b/logs"));
    EXPECT_TRUE(gitignore_match("**/logs", "logs"));
    EXPECT_TRUE(gitignore_match("logs/**", "logs/a/b.txt"));
    EXPECT_TRUE(gitignore_match("a/**/b", "a/b"));
    EXPECT_TRUE(gitignore_match("a/**/b", "a/x/y/b"));
    EXPECT_FALSE(gitignore_match("a/**/b", "a/x/yb"));
    EXPECT_FALSE(gitignore_match("x**/y", "x/z/y")) << "\"**\" inside a part is two stars";
    EXPECT_TRUE(gitignore_match("\\#notes", "#notes"));
}

TEST(OuiCodeIgnoreRulesTests, FollowsTheRulesOfEachGitignoreFile)
{
    IgnoreRules rules;
    rules.add("\xEF\xBB\xBF"
              "build/\n"
              "# a comment\n"
              "*.log\n"
              "!keep.log\n"
              "/top.txt\n"
              "docs/*.html\n",
              "");
    rules.add("generated\n", "src");
    EXPECT_TRUE(rules.is_ignored("build", true)) << "the byte order mark is no part of the pattern";
    EXPECT_FALSE(rules.is_ignored("build", false)) << "a trailing slash is for folders only";
    EXPECT_TRUE(rules.is_ignored("sub/build", true)) << "a pattern without a slash matches at any depth";
    EXPECT_TRUE(rules.is_ignored("a/b/run.log", false));
    EXPECT_FALSE(rules.is_ignored("a/keep.log", false)) << "a later '!' brings a file back";
    EXPECT_TRUE(rules.is_ignored("top.txt", false));
    EXPECT_FALSE(rules.is_ignored("sub/top.txt", false)) << "a leading slash anchors to the file's folder";
    EXPECT_TRUE(rules.is_ignored("docs/index.html", false));
    EXPECT_FALSE(rules.is_ignored("docs/api/index.html", false));
    EXPECT_TRUE(rules.is_ignored("src/generated", false));
    EXPECT_TRUE(rules.is_ignored("src/deep/generated", true));
    EXPECT_FALSE(rules.is_ignored("generated", false)) << "a nested .gitignore rules only below its folder";
}

TEST(OuiCodeIgnoreRulesTests, ReadsTheGitignoreFilesFromTheTopOfTheRepositoryDown)
{
    const auto top{std::filesystem::temp_directory_path() / "olagarro_ignore_rules_test"};
    std::filesystem::remove_all(top);
    std::filesystem::create_directories(top / ".git" / "info");
    std::filesystem::create_directories(top / "app" / "src");
    std::ofstream{top / ".gitignore"} << "*.tmp\n";
    std::ofstream{top / "app" / ".gitignore"} << "/out/\n";
    std::ofstream{top / ".git" / "info" / "exclude"} << "secret\n";

    const auto rules{IgnoreRules::for_folder(top / "app" / "src")};
    ASSERT_TRUE(rules.in_repository());
    EXPECT_EQ(rules.relative(top / "app" / "src"), "app/src");
    EXPECT_TRUE(rules.is_ignored("app/src/a.tmp", false));
    EXPECT_TRUE(rules.is_ignored("app/out", true));
    EXPECT_FALSE(rules.is_ignored("out", true));
    EXPECT_TRUE(rules.is_ignored("app/src/secret", false));
    std::filesystem::remove_all(top);

    EXPECT_FALSE(IgnoreRules::for_folder(std::filesystem::temp_directory_path()).in_repository());
}

} // namespace
