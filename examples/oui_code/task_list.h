#pragma once

#include <oui/application.h>
#include <oui/combo_box.h>
#include <oui/line_edit.h>
#include <oui/container.h>
#include <oui/table.h>
#include <oui/menu_bar.h>
#include <oui/split_view.h>
#include <oui/tab_view.h>
#include <oui/text_edit.h>

#include <signalslot/signalslot.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// How soon a task wants doing, most urgent first.
enum class Priority
{
    urgent,
    high,
    normal,
    low
};

// Something to do: a line that says what, a note with as much as it needs, whether it is done, and how soon.
struct Task
{
    std::string title;
    std::string note;
    bool done{false};
    Priority priority{Priority::normal};
    // When it was done, as YYYY-MM-DD; empty while it is not, or when nobody said.
    std::string done_on;
};

// Tasks are kept as Markdown, to read and edit by hand or keep in git: each one a "- [ ] title" line, "- [x]"
// once done, its note under it indented by two spaces. As todo.txt writes them, a done task has the day it was
// done first, "- [x] 2026-10-03 title", and a priority goes before the title: "(A)" urgent, "(B)" high, "(D)" low;
// a normal task has none, or "(C)". Other lines, such as a heading, are left out.
[[nodiscard]] std::vector<Task> parse_tasks(std::string_view text);
[[nodiscard]] std::string format_tasks(const std::vector<Task>& tasks);
// Whether a text has nothing but tasks, their notes, headings and blank lines, so keeping it as a task list loses
// nothing of it.
[[nodiscard]] bool holds_only_tasks(std::string_view text);

// Where a project keeps its tasks, at its top so they show and go with it; without a project, beside the settings.
[[nodiscard]] std::filesystem::path tasks_file(const std::optional<std::filesystem::path>& project,
                                               const std::filesystem::path& settings_folder);
// Whether a file holds a task list rather than text to edit: one named tasks.md, or anything.tasks.md.
[[nodiscard]] bool is_task_list_file(const std::filesystem::path& file);
// Today, as a done task keeps it.
[[nodiscard]] std::string today();

// The tasks of a file in a tab: the list on the left, the note of the selected one on the right. Every change is
// saved at once. In the list, Enter or Insert adds a task, Space or a click on its box marks it done or not, F2
// renames it, Delete removes it, Alt+1 to Alt+4 make it urgent, high, normal or low, Alt+Up and Alt+Down move it,
// and Tab or a double click go to its note; Escape there comes back. Above the note, a box shows and sets the
// priority. Over the list, two tabs show the tasks pending and those done, F6 going from one to the other; marking
// a task moves it to the other tab. Under them, a field finds tasks by their title or note, and typing in the list
// starts a search there. The list is in the user's order, or the most recently done first, until a click on the
// Name or Priority header sorts it by that.
class TasksView final : public olagarro::oui::Container
{
  public:
    TasksView(olagarro::oui::Application& application, std::filesystem::path file);

    [[nodiscard]] const std::filesystem::path& file() const;
    // The file's name and how many tasks are left, for the tab.
    [[nodiscard]] std::string title() const;
    // Every change is saved as it is made; this writes the file now, as Ctrl+S asks.
    void save();
    // Keeps the tasks in `file` from now on, written there at once.
    void save_as(std::filesystem::path file);
    // How many tasks are not done yet.
    [[nodiscard]] std::size_t open_count() const;
    // Fires after the tasks change, as their count may have.
    template <typename Callback> olagarro::signal_slot::Connection on_changed(Callback&& callback)
    {
        return m_changed.connect(std::forward<Callback>(callback));
    }
    // Fires with where a right click on the list fell, the task under it selected, to open a menu there.
    template <typename Callback> olagarro::signal_slot::Connection on_context_requested(Callback&& callback)
    {
        return m_context_requested.connect(std::forward<Callback>(callback));
    }
    // What can be done with the selected task, for a menu: what the keys of the list do.
    [[nodiscard]] std::vector<olagarro::oui::MenuItem> context_items();
    // Puts the focus in the list.
    void focus_list();
    // Puts the focus in the field that finds tasks, what it holds selected.
    void focus_search();

    // Escape in the note goes back to the list.
    bool handle_event(const olagarro::oui::Event& event) override;
    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override;
    void arrange(const olagarro::oui::Rect& bounds) override;

  private:
    void load();
    // Selects `selected` when it shows, or else the row at `row`, or the last.
    void show_tasks(std::optional<std::size_t> selected, std::size_t row = 0);
    // Shows the tasks done, or those pending.
    void show_done(bool done);
    void show_note();
    // The task of the selected row, as the rows show the tasks found, in the order chosen.
    [[nodiscard]] std::optional<std::size_t> selected() const;
    void prioritize(Priority priority);
    // Starts a search with what was typed in the list.
    void search_from_list(const olagarro::oui::Event& event);
    void add_task();
    void rename_task();
    void remove_task();
    void toggle_task();
    void move_task(int step);

    olagarro::oui::Application& m_application;
    std::filesystem::path m_file;
    std::vector<Task> m_tasks;
    olagarro::oui::SplitView* m_split{};
    // The tasks as rows: their box, title and priority.
    olagarro::oui::Table* m_list{};
    olagarro::oui::LineEdit* m_search{};
    // The column the list is sorted by, from a click on its header, and which way; none keeps the user's order for
    // what is pending, and the latest first for what is done.
    std::optional<std::size_t> m_sort_column;
    olagarro::oui::SortOrder m_sort_order{olagarro::oui::SortOrder::none};
    olagarro::oui::TabView* m_tabs{};
    bool m_showing_done{false};
    olagarro::oui::ComboBox* m_priority{};
    // Which task each row shows.
    std::vector<std::size_t> m_rows;
    olagarro::oui::TextEdit* m_note{};
    // The selected task's title, to edit beside its note.
    olagarro::oui::LineEdit* m_title{};
    // The note shown belongs to this task; set while it is loaded, so loading is not taken for an edit.
    std::optional<std::size_t> m_note_of;
    bool m_loading_note{false};
    olagarro::signal_slot::Signal<> m_changed;
    olagarro::signal_slot::Signal<olagarro::oui::Point> m_context_requested;
};

[[nodiscard]] std::unique_ptr<TasksView> tasks_view(olagarro::oui::Application& application,
                                                    std::filesystem::path file);
