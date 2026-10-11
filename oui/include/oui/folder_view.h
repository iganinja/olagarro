#pragma once

#include <oui/file_system.h>
#include <oui/list.h>
#include <oui/translation.h>

#include <signalslot/signalslot.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace olagarro::oui
{

// What a FolderView shows besides file names: English, or their translation.
struct FolderViewTexts
{
    std::string project{translate("FolderView", "Project")};
    std::string explorer{translate("FolderView", "Explorer")};
    std::string empty{translate("FolderView", "Open a folder")};
};

// A folder's contents as a list, folders first, with ".." to go up and the location in the header. Without a
// scope it is an explorer of the whole file system; with one, a project tree that never goes above the
// scope's folder, and its header says which it is.
//
// Activating a file, by Enter or a click, announces it. The usual file-panel keys announce requests and
// leave the work to the application: F2 to rename, Delete to delete, Insert for a new file, F7 for a new
// folder and F5 to copy, as file managers with two panels copy to the other one. Typing jumps to an entry
// by its name.
class FolderView final : public List
{
  public:
    FolderView();

    // Limits navigation to `folder` and what lies inside it; nothing allows the whole file system.
    void set_scope(std::optional<std::filesystem::path> folder);
    [[nodiscard]] const std::optional<std::filesystem::path>& scope() const;
    // Shows `folder`, or the scope's folder when `folder` lies outside the scope.
    void show_folder(const std::filesystem::path& folder);
    [[nodiscard]] const std::filesystem::path& current_path() const;
    // Lists the folder again, keeping the selected entry, or selecting `select` when given.
    void refresh(const std::optional<std::filesystem::path>& select = std::nullopt);
    // The selected file or folder; nothing on "..".
    [[nodiscard]] std::optional<std::filesystem::path> selected_path() const;
    void set_texts(FolderViewTexts texts);
    // What the panel is: "Project" with a scope, "Explorer" without, as its header says.
    [[nodiscard]] const std::string& name() const;

    template <typename Callback> olagarro::signal_slot::Connection on_activated(Callback&& callback)
    {
        return m_activated.connect(std::forward<Callback>(callback));
    }
    // With the selected entry.
    template <typename Callback> olagarro::signal_slot::Connection on_rename_requested(Callback&& callback)
    {
        return m_rename_requested.connect(std::forward<Callback>(callback));
    }
    // With where a right click fell, to open a menu there.
    template <typename Callback> olagarro::signal_slot::Connection on_context_requested(Callback&& callback)
    {
        return m_context_requested.connect(std::forward<Callback>(callback));
    }
    template <typename Callback> olagarro::signal_slot::Connection on_delete_requested(Callback&& callback)
    {
        return m_delete_requested.connect(std::forward<Callback>(callback));
    }
    template <typename Callback> olagarro::signal_slot::Connection on_copy_requested(Callback&& callback)
    {
        return m_copy_requested.connect(std::forward<Callback>(callback));
    }
    // With the folder shown, and whether a folder rather than a file is wanted.
    template <typename Callback> olagarro::signal_slot::Connection on_create_requested(Callback&& callback)
    {
        return m_create_requested.connect(std::forward<Callback>(callback));
    }

    bool handle_event(const Event& event) override;
    // F2, F5, F7, Insert and Delete, without modifiers: the panel's own file operations.
    [[nodiscard]] bool claims_key(const Event& key) const override;
    void paint(Painter& painter, const Theme& theme) const override;

  private:
    [[nodiscard]] bool can_go_up() const;
    [[nodiscard]] bool has_parent_row() const;
    // "Project: " or "Explorer: ", then where the panel is.
    [[nodiscard]] std::string header_label() const;
    [[nodiscard]] std::string header_location() const;
    void go_up();
    void activate(std::size_t row);
    void populate(const std::optional<std::filesystem::path>& select);

    FolderViewTexts m_texts;
    std::optional<std::filesystem::path> m_scope;
    std::filesystem::path m_current;
    std::vector<DirectoryEntry> m_entries;
    // The row a click went down on: it opens once the button comes up on the same row.
    std::optional<std::size_t> m_pressed_row;
    olagarro::signal_slot::Signal<std::filesystem::path> m_activated;
    olagarro::signal_slot::Signal<std::filesystem::path> m_rename_requested;
    olagarro::signal_slot::Signal<Point> m_context_requested;
    olagarro::signal_slot::Signal<std::filesystem::path> m_delete_requested;
    olagarro::signal_slot::Signal<std::filesystem::path> m_copy_requested;
    olagarro::signal_slot::Signal<std::filesystem::path, bool> m_create_requested;
};

std::unique_ptr<FolderView> folder_view();
FolderView& folder_view(Container& parent);

} // namespace olagarro::oui
