#include "task_list.h"

#include <gtest/gtest.h>

namespace
{

TEST(OuiCodeTaskListTests, ReadsTasksWithTheirNotesAndWhetherTheyAreDone)
{
    const auto tasks{parse_tasks("# Tasks\n"
                                 "\n"
                                 "- [ ] Write the help\n"
                                 "  In English and Spanish.\n"
                                 "\n"
                                 "  Both at once.\n"
                                 "\n"
                                 "- [x] Fix the menu\n"
                                 "- [X] Done in capitals\n")};
    ASSERT_EQ(tasks.size(), 3u);
    EXPECT_EQ(tasks[0].title, "Write the help");
    EXPECT_FALSE(tasks[0].done);
    EXPECT_EQ(tasks[0].note, "In English and Spanish.\n\nBoth at once.");
    EXPECT_EQ(tasks[1].title, "Fix the menu");
    EXPECT_TRUE(tasks[1].done);
    EXPECT_EQ(tasks[1].note, "");
    EXPECT_TRUE(tasks[2].done);
}

TEST(OuiCodeTaskListTests, WritesWhatItReadsBack)
{
    const std::vector<Task> tasks{{"One", "A note\n\nof two paragraphs", false, Priority::normal, {}},
                                  {"Two", "", true, Priority::high, "2026-10-03"}};
    const auto read{parse_tasks(format_tasks(tasks))};
    ASSERT_EQ(read.size(), tasks.size());
    for(std::size_t index{}; index < tasks.size(); ++ index)
    {
        EXPECT_EQ(read[index].title, tasks[index].title);
        EXPECT_EQ(read[index].note, tasks[index].note);
        EXPECT_EQ(read[index].done, tasks[index].done);
        EXPECT_EQ(read[index].done_on, tasks[index].done_on);
    }
}

TEST(OuiCodeTaskListTests, ReadsAndWritesPrioritiesAsTodoTxtDoes)
{
    const auto tasks{parse_tasks(
        "- [ ] (A) Urgent\n- [x] (B) Soon\n- [ ] (C) Normal\n- [ ] (D) Some day\n- [ ] (E) Not a priority\n")};
    ASSERT_EQ(tasks.size(), 5u);
    EXPECT_EQ(tasks[0].priority, Priority::urgent);
    EXPECT_EQ(tasks[0].title, "Urgent");
    EXPECT_EQ(tasks[1].priority, Priority::high);
    EXPECT_EQ(tasks[2].priority, Priority::normal);
    EXPECT_EQ(tasks[3].priority, Priority::low);
    EXPECT_EQ(tasks[4].priority, Priority::normal);
    EXPECT_EQ(tasks[4].title, "(E) Not a priority");
    // A normal task is written with no mark.
    const auto text{format_tasks(tasks)};
    EXPECT_NE(text.find("- [ ] Normal\n"), std::string::npos);
    EXPECT_NE(text.find("- [ ] (D) Some day\n"), std::string::npos);
    const auto again{parse_tasks(text)};
    ASSERT_EQ(again.size(), tasks.size());
    for(std::size_t index{}; index < tasks.size(); ++ index)
    {
        EXPECT_EQ(again[index].priority, tasks[index].priority);
        EXPECT_EQ(again[index].title, tasks[index].title);
    }
}

TEST(OuiCodeTaskListTests, KeepsTheTasksInTheProjectOrBesideTheSettings)
{
    EXPECT_EQ(tasks_file(std::filesystem::path{"/p"}, "/s"), std::filesystem::path{"/p/tasks.md"});
    EXPECT_EQ(tasks_file(std::nullopt, "/s"), std::filesystem::path{"/s/oui_code_tasks.md"});
}

TEST(OuiCodeTaskListTests, TellsTaskListsFromOtherFilesByName)
{
    EXPECT_TRUE(is_task_list_file("/p/tasks.md"));
    EXPECT_TRUE(is_task_list_file("/p/release.tasks.md"));
    EXPECT_FALSE(is_task_list_file("/p/README.md"));
    EXPECT_FALSE(is_task_list_file("/p/.tasks.md"));
}

TEST(OuiCodeTaskListTests, KeepsTheDayATaskWasDone)
{
    const auto tasks{parse_tasks("- [x] 2026-10-03 (B) Buy cables\n- [x] Fix the menus\n- [ ] 2026-10-03 Not a day\n")};
    ASSERT_EQ(tasks.size(), 3u);
    EXPECT_EQ(tasks[0].done_on, "2026-10-03");
    EXPECT_EQ(tasks[0].priority, Priority::high);
    EXPECT_EQ(tasks[0].title, "Buy cables");
    EXPECT_TRUE(tasks[1].done_on.empty());
    // Only a done task has a day: an open one keeps it as part of its title.
    EXPECT_EQ(tasks[2].title, "2026-10-03 Not a day");
    EXPECT_NE(format_tasks(tasks).find("- [x] 2026-10-03 (B) Buy cables\n"), std::string::npos);
    EXPECT_EQ(today().size(), 10u);
}

TEST(OuiCodeTaskListTests, TellsATaskListFromOtherText)
{
    EXPECT_TRUE(holds_only_tasks("# Tasks\n\n- [ ] One\n  its note\n\n- [x] 2026-10-03 Two\n"));
    EXPECT_TRUE(holds_only_tasks(""));
    EXPECT_FALSE(holds_only_tasks("# Readme\n\nSome text.\n- [ ] One\n"));
    EXPECT_FALSE(holds_only_tasks("  indented before any task\n- [ ] One\n"));
}

} // namespace
