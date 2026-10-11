#pragma once

#include "documents.h"
#include "languages.h"

#include <oui/application.h>
#include <oui/event.h>
#include <oui/folder_view.h>
#include <oui/menu_bar.h>
#include <oui/settings.h>
#include <oui/split_view.h>
#include <oui/tab_view.h>

#include <cstddef>
#include <filesystem>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The editor's area: its groups of tabs, how they split and join, and how files open into them.

// A row of tabs, and what each holds in tab order.
struct EditorGroup
{
    olagarro::oui::TabView& tabs;
    std::vector<Document> documents;
};

// A side of a group of tabs on screen.
enum class Side
{
    left,
    right,
    top,
    bottom
};

// The editor's groups of tabs, which splitting divides as finely as the screen allows: each split puts a
// new group beside or under one, in a tree of split views. Commands act on the active group, the one the
// focus is in or was in last.
class Editors
{
  public:
    // `area` holds the tree, and starts with one group.
    Editors(olagarro::oui::Application& application, olagarro::oui::SplitView& area);
    Editors(const Editors&) = delete;
    Editors& operator=(const Editors&) = delete;

    // Opens a menu at a point, as a right click in a tab's content asks; the menu bar sets it.
    std::function<void(std::vector<olagarro::oui::MenuItem>, olagarro::oui::Point)> open_context_menu;

    // Runs `setup` on every group's tabs, those there are and those splitting makes.
    void prepare_tabs(std::function<void(olagarro::oui::TabView&)> setup);
    [[nodiscard]] EditorGroup& active();
    // Where documents find their language; none leaves them all plain.
    void set_languages(const Languages* languages);
    // Gives each document the colours, comment and brackets of its file's language, when its file changed
    // since, or every document when `again`, as after the scripts are loaded again.
    void apply_languages(bool again = false);
    // The groups as they lie on screen, written as text to come back next time: a group is its number, a split
    // is h (side by side) or v (one above the other), the first group's share, and its two halves, as in
    // h0.5(0,v0.33(1,2)). The groups are numbered in that order, which `order` gives.
    [[nodiscard]] std::string layout(std::vector<const EditorGroup*>& order) const;
    // Splits the single group the editor starts with as `text`, a layout, says, and gives the groups by their
    // numbers. Text that is not a layout leaves the one group.
    [[nodiscard]] std::vector<EditorGroup*> restore_layout(std::string_view text);
    // Moves the line nearest `group` across `side`'s direction one cell that way, as tmux's resize-pane does: the
    // group grows when the line is on that side of it, and shrinks when it is on the other.
    void move_line(const EditorGroup& group, Side side);
    // Makes `group` the active one, which open_file_at_path opens into, before the focus is in it.
    void set_active(EditorGroup& group);
    [[nodiscard]] std::list<EditorGroup>& groups();
    [[nodiscard]] const std::list<EditorGroup>& groups() const;
    [[nodiscard]] std::size_t count() const;

    // A new, empty group on `side` of `group`, the two sharing its space half and half.
    EditorGroup& split(EditorGroup& group, Side side);
    // Takes a group off the screen, with whatever it still holds; the group beside it grows into its space.
    // The last group stays.
    void remove(EditorGroup& group);
    // The group next to `group` on `side` on screen: the nearest, and of those the one most in line with
    // it. None at the edge of the editor.
    [[nodiscard]] EditorGroup* neighbour(const EditorGroup& group, Side side);

  private:
    [[nodiscard]] std::string describe(const olagarro::oui::Widget& widget,
                                       std::vector<const EditorGroup*>& order) const;

    olagarro::oui::Application& m_application;
    olagarro::oui::SplitView& m_area;
    std::list<EditorGroup> m_groups;
    EditorGroup* m_active{};
    std::vector<std::function<void(olagarro::oui::TabView&)>> m_setups;
    const Languages* m_languages{};
};

// Gives the focus to what the selected tab shows: a document, a preview or whatever else a tab holds.
void focus_current_tab(olagarro::oui::Application& application, olagarro::oui::TabView& tabs);
// The blank "Untitled" tab OUI Code starts with makes way for the first document opened into it.
void drop_lone_empty_tab(olagarro::oui::TabView& tabs, std::vector<Document>& documents);
// Moves the tab at `index` of one group to the end of another, selected there, and shows that group.
void move_tab(EditorGroup& from, std::size_t index, EditorGroup& to);
// Moves a tab of a group from one place among its tabs to another, its document with it.
void reorder_tab(EditorGroup& group, std::size_t from, std::size_t to);
// After tabs leave a group: an empty group goes, the one beside it taking its space, unless it is the last,
// which gets a blank document, as OUI Code always shows one.
void settle_group(Editors& editors, EditorGroup& group, olagarro::oui::Settings& settings);
// The group beside `group` on any side, for things that want somewhere else to go.
[[nodiscard]] EditorGroup* any_neighbour(Editors& editors, const EditorGroup& group);

// Shows the task list kept in `file` in a tab, the one already open or a new one in `into`, the active group by
// default.
void open_task_list(olagarro::oui::Application& application, Editors& editors, const std::filesystem::path& file,
                    EditorGroup* into = nullptr);

// How a file opens: as what its name says, a task list as one and anything else as text, or as either whatever
// its name.
enum class OpenAs
{
    natural,
    text,
    task_list
};

// Whether `file` can open as a task list without losing any of it: kept as one, whatever is not a task would go.
[[nodiscard]] bool could_be_task_list(const std::filesystem::path& file);
// Shows `file` in a tab of `into`, the active group by default, or in its tab wherever it is already open, and
// makes its folder `current_folder`. Returns false when it cannot be read, after telling the user why.
bool open_file_at_path(olagarro::oui::Application& application, Editors& editors, olagarro::oui::FolderView& explorer,
                       std::filesystem::path& current_folder, const std::filesystem::path& file,
                       olagarro::oui::Settings& settings, EditorGroup* into = nullptr, OpenAs as = OpenAs::natural);

// What Split Editor was told with a key: a side, and what to do there.
struct SplitChoice
{
    enum class Kind
    {
        // A new group on that side.
        split,
        // The current tab to the group on that side.
        move,
        // The focus to the group on that side.
        focus,
        // The group closes into the one beside it.
        join
    };
    Kind kind{Kind::split};
    Side side{Side::right};
};

// Does what a key told Split Editor, to the active group: a new group on a side, the tab to the group on a
// side, the focus there, or the group closed into the one beside it.
void apply_split_choice(olagarro::oui::Application& application, Editors& editors, olagarro::oui::Settings& settings,
                        const SplitChoice& choice);
// The choice a key makes in Split Editor: an arrow a side, Shift moving the tab there and Ctrl going there, and
// Backspace closing the group when there are others; nothing for any other key.
[[nodiscard]] std::optional<SplitChoice> split_choice_of(const olagarro::oui::KeyEvent& key, bool several);
