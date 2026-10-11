#include "task_list.h"

#include "ui_descriptions.h"

#include <oui/button.h>
#include <oui/dialogs.h>
#include <oui/file_system.h>
#include <oui/grid.h>
#include <oui/label.h>
#include <oui/text_file.h>
#include <oui/spacer.h>
#include <oui/text_metrics.h>
#include <oui/translation.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <ctime>
#include <system_error>
#include <utility>

using namespace olagarro;

namespace
{

constexpr std::string_view open_mark{"- [ ] "};
constexpr std::string_view done_mark{"- [x] "};
constexpr std::string_view note_indent{"  "};

// A title without the spaces round it; a task needs one with something in it.
[[nodiscard]] std::string trimmed(std::string_view text)
{
    const auto first{text.find_first_not_of(" \t")};
    if(first == std::string_view::npos)
    {
        return {};
    }
    return std::string{text.substr(first, text.find_last_not_of(" \t") - first + 1)};
}

[[nodiscard]] std::optional<std::string> check_title(std::string_view title)
{
    if(trimmed(title).empty())
    {
        return oui::translate("Tasks", "Write what there is to do.");
    }
    return std::nullopt;
}

// The priorities as a box offers them, most urgent first.
[[nodiscard]] std::vector<std::string> priority_names()
{
    return {oui::translate("Tasks", "Urgent"), oui::translate("Tasks", "High"), oui::translate("Tasks", "Normal"),
            oui::translate("Tasks", "Low")};
}

// The tasks' rows: a mark that says whether the task is done, in a column of its own, its title and its priority.
class TaskList final : public oui::Table
{
  public:
    struct Keys
    {
        std::function<void()> add;
        std::function<void()> rename;
        std::function<void()> remove;
        std::function<void()> toggle;
        std::function<void(int)> move;
        std::function<void()> open_note;
        std::function<void(Priority)> prioritize;
        // A character typed, or Backspace, that goes to the search.
        std::function<void(const oui::Event&)> search;
        std::function<void(oui::Point)> context;
    };

    explicit TaskList(Keys keys) : m_keys{std::move(keys)} {}

    bool handle_event(const oui::Event& event) override
    {
        using oui::Key;
        if(const auto* const key{std::get_if<oui::KeyEvent>(&event)})
        {
            if(key->alt && (key->key == Key::up || key->key == Key::down))
            {
                m_keys.move(key->key == Key::up ? -1 : 1);
                return true;
            }
            if(key->control || key->alt)
            {
                return Table::handle_event(event);
            }
            switch(key->key)
            {
            case Key::backspace:
                m_keys.search(event);
                return true;
            case Key::insert_:
            case Key::enter:
                m_keys.add();
                return true;
            case Key::f2:
                m_keys.rename();
                return true;
            case Key::delete_:
                m_keys.remove();
                return true;
            case Key::space:
                m_keys.toggle();
                return true;
            case Key::tab:
                m_keys.open_note();
                return true;
            default:
                break;
            }
        }
        if(const auto* const text{std::get_if<oui::TextEvent>(&event)}; text != nullptr && !text->control)
        {
            // Alt+1 to Alt+4 make the task urgent, high, normal or low.
            if(text->alt && text->codepoint >= U'1' && text->codepoint <= U'4')
            {
                m_keys.prioritize(static_cast<Priority>(text->codepoint - U'1'));
                return true;
            }
            // Anything else typed, but the Space that marks a task, finds tasks.
            if(!text->alt && text->codepoint != U' ')
            {
                m_keys.search(event);
                return true;
            }
        }
        // A right click selects what it falls on and asks for the menu of what can be done with it.
        if(const auto* const mouse{std::get_if<oui::MouseEvent>(&event)};
           mouse != nullptr && mouse->pressed && mouse->right)
        {
            if(row_at(mouse->y))
            {
                auto left{*mouse};
                left.right = false;
                left.clicks = 1;
                static_cast<void>(Table::handle_event(left));
            }
            m_keys.context({mouse->x, mouse->y});
            return true;
        }
        // A click on a task's box marks it done or not, once the click has selected it; below the last task there
        // is no box.
        if(const auto* const mouse{std::get_if<oui::MouseEvent>(&event)};
           mouse != nullptr && mouse->pressed && !mouse->right && mouse->clicks == 1 && mouse->x >= bounds().x &&
           mouse->x < bounds().x + marker_columns && row_at(mouse->y))
        {
            const auto handled{Table::handle_event(event)};
            m_keys.toggle();
            return handled;
        }
        return Table::handle_event(event);
    }

    // The columns of a task's box, before its title.
    static constexpr int marker_columns{4};

  private:
    Keys m_keys;
};

// Whether `text` is a day as YYYY-MM-DD.
[[nodiscard]] bool is_date(std::string_view text)
{
    if(text.size() != 10 || text[4] != '-' || text[7] != '-')
    {
        return false;
    }
    return std::all_of(text.begin(), text.end(),
                       [](char c) { return c == '-' || std::isdigit(static_cast<unsigned char>(c)) != 0; });
}

} // namespace

std::string today()
{
    const auto now{std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())};
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::array<char, 11> text{};
    static_cast<void>(std::strftime(text.data(), text.size(), "%Y-%m-%d", &local));
    return text.data();
}

std::vector<Task> parse_tasks(std::string_view text)
{
    std::vector<Task> tasks;
    std::size_t start{};
    while(start <= text.size())
    {
        const auto end{std::min(text.find('\n', start), text.size())};
        auto line{text.substr(start, end - start)};
        if(!line.empty() && line.back() == '\r')
        {
            line.remove_suffix(1);
        }
        start = end + 1;
        if(line.substr(0, open_mark.size()) == open_mark || line.substr(0, done_mark.size()) == done_mark ||
           line.substr(0, 6) == "- [X] ")
        {
            auto title{line.substr(open_mark.size())};
            Task task{{}, {}, line[3] != ' ', Priority::normal, {}};
            if(task.done && is_date(title.substr(0, 10)) && title.size() > 10 && title[10] == ' ')
            {
                task.done_on = std::string{title.substr(0, 10)};
                title.remove_prefix(11);
            }
            if(title.size() >= 4 && title[0] == '(' && title[2] == ')' && title[3] == ' ' && title[1] >= 'A' &&
               title[1] <= 'D')
            {
                task.priority = static_cast<Priority>(title[1] - 'A');
                title.remove_prefix(4);
            }
            task.title = std::string{title};
            tasks.push_back(std::move(task));
            continue;
        }
        if(tasks.empty())
        {
            continue;
        }
        // A note's lines, indented under their task; blank lines belong to it while more of it follows.
        auto& note{tasks.back().note};
        if(line.substr(0, note_indent.size()) == note_indent)
        {
            note += std::string{line.substr(note_indent.size())} + "\n";
        }
        else if(line.empty() && !note.empty())
        {
            note += "\n";
        }
        if(end == text.size())
        {
            break;
        }
    }
    for(auto& task : tasks)
    {
        while(!task.note.empty() && task.note.back() == '\n')
        {
            task.note.pop_back();
        }
    }
    return tasks;
}

bool holds_only_tasks(std::string_view text)
{
    auto in_task{false};
    for(std::size_t start{}; start < text.size();)
    {
        const auto end{std::min(text.find('\n', start), text.size())};
        auto line{text.substr(start, end - start)};
        start = end + 1;
        if(!line.empty() && line.back() == '\r')
        {
            line.remove_suffix(1);
        }
        const auto task{line.substr(0, open_mark.size()) == open_mark ||
                        line.substr(0, done_mark.size()) == done_mark || line.substr(0, 6) == "- [X] "};
        if(task)
        {
            in_task = true;
        }
        else if(!line.empty() && line.front() != '#' && !(in_task && line.substr(0, note_indent.size()) == note_indent))
        {
            return false;
        }
    }
    return true;
}

std::string format_tasks(const std::vector<Task>& tasks)
{
    std::string text{"# " + oui::translate("Tasks", "Tasks") + "\n"};
    for(const auto& task : tasks)
    {
        text += "\n";
        text += std::string{task.done ? done_mark : open_mark};
        if(task.done && !task.done_on.empty())
        {
            text += task.done_on + " ";
        }
        if(task.priority != Priority::normal)
        {
            text += std::string{"("} + static_cast<char>('A' + static_cast<int>(task.priority)) + ") ";
        }
        text += task.title + "\n";
        std::size_t start{};
        while(!task.note.empty() && start <= task.note.size())
        {
            const auto end{std::min(task.note.find('\n', start), task.note.size())};
            const auto line{std::string_view{task.note}.substr(start, end - start)};
            text += line.empty() ? std::string{} : std::string{note_indent} + std::string{line};
            text += "\n";
            start = end + 1;
        }
    }
    return text;
}

std::filesystem::path tasks_file(const std::optional<std::filesystem::path>& project,
                                 const std::filesystem::path& settings_folder)
{
    return project ? *project / oui::path_from_utf8("tasks.md")
                   : settings_folder / oui::path_from_utf8("oui_code_tasks.md");
}

bool is_task_list_file(const std::filesystem::path& file)
{
    const auto name{oui::utf8_of(file.filename())};
    constexpr std::string_view suffix{".tasks.md"};
    return name == "tasks.md" ||
           (name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0);
}

TasksView::TasksView(oui::Application& application, std::filesystem::path file)
    : m_application{application}, m_file{std::move(file)}
{
    auto ui{build_oui_code_ui(OuiCodeUi::tasks)};
    m_split = &ui.get<oui::SplitView>("split");
    auto list{std::make_unique<TaskList>(
        TaskList::Keys{[this] { add_task(); }, [this] { rename_task(); }, [this] { remove_task(); },
                       [this] { toggle_task(); }, [this](int step) { move_task(step); },
                       [this]
                       {
                           if(selected())
                           {
                               m_application.focus(*m_note);
                           }
                       },
                       [this](Priority priority) { prioritize(priority); }, [this](const oui::Event& typed)
                       { search_from_list(typed); }, [this](oui::Point at) { m_context_requested.fire(at); }})};
    m_list = list.get();
    // A column for the box, unnamed, then the title, taking what is left, and the priority.
    const auto names{priority_names()};
    // Columns are in cells, and text as wide as the backend draws it.
    const auto cells{[](std::string_view text)
                     {
                         const auto& metrics{oui::text_metrics()};
                         const auto cell{std::max(1, metrics.cell_width())};
                         return (metrics.width(text) + cell - 1) / cell;
                     }};
    // The header has room for the arrow that marks it sorted.
    auto priority_width{cells(oui::translate("Tasks", "Priority")) + 2};
    for(const auto& name : names)
    {
        priority_width = std::max(priority_width, cells(name));
    }
    m_list->set_headers({"", oui::translate("Tasks", "Name"), oui::translate("Tasks", "Priority")});
    m_list->set_column_width(0, TaskList::marker_columns);
    m_list->set_column_width(1, 10);
    // Room besides for the margins round a cell's text, and for the header's, drawn bolder.
    m_list->set_column_width(2, priority_width + 3);
    m_list->set_stretch_column(1);
    // A click on the Name or Priority header sorts by it, up, down, then back to the list's own order; the rows are
    // made from the tasks, so they are sorted here, and the header marked here too.
    m_list->set_headers_clickable(true);
    m_list->set_sorted_by_owner(true);
    m_list->set_columns_resizable(true);
    m_list->on_column_click(
        [this](std::size_t column, oui::SortOrder order)
        {
            if(column == 0)
            {
                return;
            }
            for(std::size_t each{}; each < m_list->column_count(); ++ each)
            {
                m_list->set_column_sort(each, each == column ? order : oui::SortOrder::none);
            }
            const auto sorted{order != oui::SortOrder::none};
            m_sort_column = sorted ? std::optional<std::size_t>{column} : std::nullopt;
            m_sort_order = order;
            show_tasks(selected());
        });
    m_list->set_colors(&oui::Theme::table_background, &oui::Theme::table_foreground);
    m_list->on_selection_changed([this](std::size_t) { show_note(); });
    m_list->on_row_activated(
        [this](std::size_t)
        {
            if(selected())
            {
                m_application.focus(*m_note);
            }
        });
    ui.put("list", std::move(list));
    m_tabs = &ui.get<oui::TabView>("tabs");
    m_tabs->on_selection_changed([this](std::size_t index) { show_done(index == 1); });
    m_search = &ui.get<oui::LineEdit>("search");
    m_search->on_changed([this](std::string_view) { show_tasks(selected()); });
    m_search->on_submitted([this](std::string_view) { focus_list(); });
    // The add button's clear column either side is for a terminal; where panels have gaps of their own, it goes.
    if(oui::text_metrics().panel_gap() != 0)
    {
        ui.get<oui::Grid>("add_row").set_columns({oui::Length::cells(0), oui::Length::fill(), oui::Length::cells(0)});
    }
    static_cast<void>(ui.get<oui::Button>("add").on_clicked([this] { add_task(); }));
    m_title = &ui.get<oui::LineEdit>("title");
    // Each change shows in the list and is saved at once, as the note's are; a task keeps its title while the field
    // is empty.
    m_title->on_changed(
        [this](std::string_view text)
        {
            const auto title{trimmed(text)};
            if(m_loading_note || !m_note_of || *m_note_of >= m_tasks.size() || title.empty() ||
               title == m_tasks[*m_note_of].title)
            {
                return;
            }
            m_tasks[*m_note_of].title = title;
            show_tasks(m_note_of);
            save();
        });
    m_title->on_submitted([this](std::string_view) { focus_list(); });
    m_priority = &ui.get<oui::ComboBox>("priority");
    m_priority->set_options(priority_names());
    m_priority->set_selected(static_cast<std::size_t>(Priority::normal));
    m_priority->on_selected([this](std::size_t index) { prioritize(static_cast<Priority>(index)); });
    m_note = &ui.get<oui::TextEdit>("note");
    m_note->on_changed(
        [this]
        {
            if(m_loading_note || !m_note_of || *m_note_of >= m_tasks.size())
            {
                return;
            }
            m_tasks[*m_note_of].note = std::string{m_note->text()};
            save();
        });
    add(ui.take());
    load();
    show_tasks(m_tasks.empty() ? std::nullopt : std::optional<std::size_t>{0});
}

const std::filesystem::path& TasksView::file() const
{
    return m_file;
}

std::string TasksView::title() const
{
    return oui::utf8_of(m_file.filename()) + " (" + std::to_string(open_count()) + ")";
}

void TasksView::save_as(std::filesystem::path file)
{
    m_file = std::move(file);
    save();
}

std::size_t TasksView::open_count() const
{
    return static_cast<std::size_t>(
        std::count_if(m_tasks.begin(), m_tasks.end(), [](const Task& task) { return !task.done; }));
}

void TasksView::focus_list()
{
    m_application.focus(*m_list);
}

bool TasksView::handle_event(const oui::Event& event)
{
    const auto* const key{std::get_if<oui::KeyEvent>(&event)};
    if(key != nullptr && key->key == oui::Key::escape &&
       (m_note->is_focused() || m_search->is_focused() || m_priority->is_focused() || m_title->is_focused()))
    {
        focus_list();
        return true;
    }
    // F6 goes between the tasks pending and those done, as the tabs over the list do.
    if(key != nullptr && key->key == oui::Key::f6 && !key->control && !key->alt && !key->shift)
    {
        show_done(!m_showing_done);
        return true;
    }
    return Container::handle_event(event);
}

oui::Size TasksView::measure(const oui::Constraints& constraints) const
{
    return constraints.constrain({constraints.max_width, constraints.max_height});
}

void TasksView::arrange(const oui::Rect& bounds)
{
    Widget::arrange(bounds);
    // Where cells allow it, a gap on the left keeps the list apart from a panel beside the tab.
    const auto gap{oui::text_metrics().panel_gap()};
    m_split->arrange({bounds.x + gap, bounds.y, std::max(0, bounds.width - gap), bounds.height});
}

void TasksView::load()
{
    oui::FileError error;
    if(const auto loaded{oui::load_text_file(m_file, error)})
    {
        m_tasks = parse_tasks(loaded->content.text);
        return;
    }
    // No file yet is no tasks yet; anything else is worth saying.
    if(error.kind != oui::FileError::Kind::not_found)
    {
        static_cast<void>(oui::message_box(m_application, oui::translate("Tasks", "Tasks"),
                                           oui::format_text(oui::translate("Tasks", "Cannot read {0}: {1}"),
                                                            {oui::utf8_of(m_file), oui::describe(error)})));
    }
}

void TasksView::save()
{
    std::error_code ignored;
    std::filesystem::create_directories(m_file.parent_path(), ignored);
    oui::FileError error;
    if(!oui::save_text_file(m_file, format_tasks(m_tasks), {}, error))
    {
        static_cast<void>(oui::message_box(m_application, oui::translate("Tasks", "Tasks"),
                                           oui::format_text(oui::translate("Tasks", "Cannot save {0}: {1}"),
                                                            {oui::utf8_of(m_file), oui::describe(error)})));
    }
    m_changed.fire();
}

std::optional<std::size_t> TasksView::selected() const
{
    const auto row{m_list->selected_row()};
    return row && *row < m_rows.size() ? std::optional<std::size_t>{m_rows[*row]} : std::nullopt;
}

void TasksView::prioritize(Priority priority)
{
    const auto task{selected()};
    if(!task || m_tasks[*task].priority == priority)
    {
        return;
    }
    m_tasks[*task].priority = priority;
    show_tasks(task);
    save();
}

std::vector<oui::MenuItem> TasksView::context_items()
{
    using oui::Key;
    using oui::menu_action;
    using oui::translate;
    const auto key{[](Key key, bool alt = false)
                   { return oui::format_shortcut(oui::Shortcut{{}, false, alt, false, key}); }};
    std::vector<oui::MenuItem> items{
        menu_action(translate("TasksMenu", "&New Task"), [this] { add_task(); }).with_shortcut(key(Key::enter))};
    const auto task{selected()};
    if(!task)
    {
        return items;
    }
    const auto& chosen{m_tasks[*task]};
    items.push_back(oui::menu_separator());
    items.push_back(
        menu_action(chosen.done ? translate("TasksMenu", "Mark as &Pending") : translate("TasksMenu", "Mark as &Done"),
                    [this] { toggle_task(); })
            .with_shortcut(key(Key::space)));
    // The priorities, the task's own checked, as Alt and their number set them.
    std::vector<oui::MenuItem> priorities;
    const auto names{priority_names()};
    for(std::size_t index{}; index < names.size(); ++ index)
    {
        const auto priority{static_cast<Priority>(index)};
        priorities.push_back(
            menu_action(
                names[index], [this, priority] { prioritize(priority); }, true, chosen.priority == priority)
                .with_shortcut(oui::format_shortcut(
                    oui::Shortcut{static_cast<char32_t>(U'1' + index), false, true, false, std::nullopt})));
    }
    items.push_back(oui::menu_submenu(translate("TasksMenu", "Pr&iority"), std::move(priorities)));
    items.push_back(oui::menu_separator());
    items.push_back(
        menu_action(translate("TasksMenu", "&Rename"), [this] { rename_task(); }).with_shortcut(key(Key::f2)));
    items.push_back(
        menu_action(translate("TasksMenu", "De&lete"), [this] { remove_task(); }).with_shortcut(key(Key::delete_)));
    return items;
}

void TasksView::focus_search()
{
    m_application.focus(*m_search);
    // What was looked for before shows selected, so typing replaces it.
    m_search->select_all();
}

void TasksView::search_from_list(const oui::Event& event)
{
    // Typing in the list goes on with the search there, rather than starting over.
    m_application.focus(*m_search);
    static_cast<void>(m_search->handle_event(event));
}

void TasksView::show_done(bool done)
{
    if(m_showing_done == done)
    {
        return;
    }
    m_showing_done = done;
    if(m_tabs->selected_index() != (done ? 1u : 0u))
    {
        m_tabs->select(done ? 1 : 0);
    }
    m_note_of.reset();
    show_tasks(std::nullopt);
}

void TasksView::show_tasks(std::optional<std::size_t> selected_task, std::size_t keep_row)
{
    // The tasks the search finds, in their title or their note, whatever the case.
    const auto lower{[](std::string text)
                     {
                         std::transform(text.begin(), text.end(), text.begin(),
                                        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                         return text;
                     }};
    const auto wanted{lower(std::string{m_search->text()})};
    m_rows.clear();
    std::size_t pending{};
    for(std::size_t index{}; index < m_tasks.size(); ++ index)
    {
        pending += m_tasks[index].done ? 0 : 1;
        if(m_tasks[index].done == m_showing_done &&
           (wanted.empty() || lower(m_tasks[index].title).find(wanted) != std::string::npos ||
            lower(m_tasks[index].note).find(wanted) != std::string::npos))
        {
            m_rows.push_back(index);
        }
    }
    m_tabs->set_title(0, oui::format_text(oui::translate("Tasks", "Pending ({0})"), {std::to_string(pending)}));
    m_tabs->set_title(
        1, oui::format_text(oui::translate("Tasks", "Done ({0})"), {std::to_string(m_tasks.size() - pending)}));
    // In the user's order, or sorted by a column, either way; tasks that tie keep the user's order. What is done
    // comes latest first unless sorted, and without a day after those that have one.
    const auto descending{m_sort_order == oui::SortOrder::descending};
    if(m_sort_column == std::optional<std::size_t>{1})
    {
        std::stable_sort(m_rows.begin(), m_rows.end(),
                         [this, &lower, descending](std::size_t a, std::size_t b)
                         {
                             const auto first{lower(m_tasks[a].title)};
                             const auto second{lower(m_tasks[b].title)};
                             return descending ? second < first : first < second;
                         });
    }
    else if(m_sort_column == std::optional<std::size_t>{2})
    {
        // Up is the most urgent first.
        std::stable_sort(m_rows.begin(), m_rows.end(),
                         [this, descending](std::size_t a, std::size_t b) {
                             return descending ? m_tasks[b].priority < m_tasks[a].priority
                                               : m_tasks[a].priority < m_tasks[b].priority;
                         });
    }
    else if(m_showing_done)
    {
        std::stable_sort(m_rows.begin(), m_rows.end(),
                         [this](std::size_t a, std::size_t b) { return m_tasks[a].done_on > m_tasks[b].done_on; });
    }
    m_list->clear();
    const auto names{priority_names()};
    for(const auto index : m_rows)
    {
        const auto& task{m_tasks[index]};
        m_list->add(oui::label(""))
            .set_marker(task.done ? "[x] " : "[ ] ", TaskList::marker_columns,
                        task.done ? oui::Icon::task_done : oui::Icon::task_open);
        m_list->add(oui::label(task.title));
        // A normal priority is the usual, so it reads quieter than the others.
        auto& priority{m_list->add(oui::label(names[static_cast<std::size_t>(task.priority)]))};
        if(task.priority == Priority::normal)
        {
            priority.set_color(&oui::Theme::disabled_foreground);
        }
    }
    if(m_rows.empty())
    {
        m_list->add_line({"",
                          !wanted.empty()   ? oui::translate("Tasks", "No task has that.")
                          : m_tasks.empty() ? oui::translate("Tasks", "No tasks yet: Enter adds one.")
                          : m_showing_done  ? oui::translate("Tasks", "Nothing done yet.")
                                            : oui::translate("Tasks", "Nothing left to do."),
                          ""});
    }
    const auto row{selected_task ? std::find(m_rows.begin(), m_rows.end(), *selected_task) : m_rows.end()};
    if(row != m_rows.end())
    {
        m_list->select_row(static_cast<std::size_t>(row - m_rows.begin()));
    }
    else if(!m_rows.empty())
    {
        m_list->select_row(std::min(keep_row, m_rows.size() - 1));
    }
    show_note();
}

void TasksView::show_note()
{
    const auto task{selected()};
    m_priority->set_selected(static_cast<std::size_t>(task ? m_tasks[*task].priority : Priority::normal));
    if(task == m_note_of && task)
    {
        return;
    }
    m_note_of = task;
    m_loading_note = true;
    m_note->set_text(task ? m_tasks[*task].note : std::string{});
    m_title->set_text(task ? m_tasks[*task].title : std::string{});
    m_note->set_read_only(!task);
    m_loading_note = false;
}

void TasksView::add_task()
{
    oui::PromptOptions options;
    options.title = oui::translate("Tasks", "New Task");
    options.label = oui::translate("Tasks", "What is there to do?");
    options.width = 64;
    options.validate = check_title;
    // And how soon, normal unless chosen otherwise.
    auto priority{Priority::normal};
    options.extra = [&priority](oui::Container& under_field)
    {
        auto& priority_row{oui::grid(under_field)};
        priority_row.set_columns({oui::Length::content(), oui::Length::content(), oui::Length::fill()});
        priority_row.set_gap(oui::Spacing::tight);
        priority_row.add(oui::label(oui::translate("Tasks", "Priority:")));
        oui::combo_box(priority_names(), static_cast<std::size_t>(priority), priority_row)
            .on_selected([&priority](std::size_t index) { priority = static_cast<Priority>(index); });
    };
    const auto title{oui::prompt(m_application, options)};
    focus_list();
    if(!title)
    {
        return;
    }
    // After the selected task, or at the end; a new task is pending, so it shows among those.
    const auto at{selected() && !m_showing_done ? *selected() + 1 : m_tasks.size()};
    m_tasks.insert(m_tasks.begin() + static_cast<std::ptrdiff_t>(at), Task{trimmed(*title), {}, false, priority, {}});
    m_note_of.reset();
    show_done(false);
    show_tasks(at);
    save();
}

void TasksView::rename_task()
{
    const auto task{selected()};
    if(!task)
    {
        return;
    }
    oui::PromptOptions options;
    options.title = oui::translate("Tasks", "Rename Task");
    options.label = oui::translate("Tasks", "What is there to do?");
    options.width = 64;
    options.validate = check_title;
    options.text = m_tasks[*task].title;
    const auto title{oui::prompt(m_application, options)};
    focus_list();
    if(!title)
    {
        return;
    }
    m_tasks[*task].title = trimmed(*title);
    // The title field over the note shows the new one.
    m_note_of.reset();
    show_tasks(task);
    save();
}

void TasksView::remove_task()
{
    const auto task{selected()};
    if(!task)
    {
        return;
    }
    const auto answer{oui::message_box(
        m_application, oui::translate("Tasks", "Delete Task"),
        oui::format_text(oui::translate("Tasks", "Delete \"{0}\" and its note?"), {m_tasks[*task].title}),
        {oui::translate("Tasks", "Delete"), oui::translate("Tasks", "Cancel")})};
    focus_list();
    if(answer != std::optional<std::size_t>{0})
    {
        return;
    }
    const auto row{m_list->selected_row().value_or(0)};
    m_tasks.erase(m_tasks.begin() + static_cast<std::ptrdiff_t>(*task));
    m_note_of.reset();
    show_tasks(std::nullopt, row);
    save();
}

void TasksView::toggle_task()
{
    const auto task{selected()};
    if(!task)
    {
        return;
    }
    // The task goes to the other tab, and the next one takes its place in this list.
    auto& toggled{m_tasks[*task]};
    toggled.done = !toggled.done;
    toggled.done_on = toggled.done ? today() : std::string{};
    m_note_of.reset();
    show_tasks(std::nullopt, m_list->selected_row().value_or(0));
    save();
}

void TasksView::move_task(int step)
{
    const auto task{selected()};
    const auto row{m_list->selected_row()};
    // Moving a task is changing the user's order of what there is to do, which only shows whole and unsorted. It
    // trades places with the pending task beside it, past any done between them.
    if(!task || !row || m_showing_done || m_sort_column || !m_search->text().empty())
    {
        return;
    }
    const auto target{static_cast<int>(*row) + step};
    if(target < 0 || target >= static_cast<int>(m_rows.size()))
    {
        return;
    }
    const auto other{m_rows[static_cast<std::size_t>(target)]};
    std::swap(m_tasks[*task], m_tasks[other]);
    m_note_of.reset();
    show_tasks(other);
    save();
}

std::unique_ptr<TasksView> tasks_view(oui::Application& application, std::filesystem::path file)
{
    return std::make_unique<TasksView>(application, std::move(file));
}
