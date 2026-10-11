#pragma once

#include "editors.h"

#include <oui/application.h>
#include <oui/text_edit.h>

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <vector>

// Go Back and Go Forward, as in browsers and code editors: the places the cursor jumped from, to another document or
// ten lines or more away, however it went there. Where the cursor is is looked at before each event, so a jump made
// by any command, click or file opened counts.
class NavigationHistory
{
  public:
    // `open` opens a file again, to go back to a place in one that was closed.
    NavigationHistory(olagarro::oui::Application& application, Editors& editors,
                      std::function<void(const std::filesystem::path&)> open);

    // Before each event: where the cursor is now, which is a jump when it is far from where it was.
    void remember();
    void back();
    void forward();

  private:
    struct Visited
    {
        olagarro::oui::WidgetRef<const olagarro::oui::TextEdit> editor;
        std::optional<std::filesystem::path> path;
        std::size_t offset;
        std::size_t line;
    };

    // The cursor of the document on show; none in a tab that holds no document, as a terminal.
    [[nodiscard]] std::optional<Visited> here() const;
    // Back at a place: its tab, or its file opened again when it was closed; false when it is gone for good, as an
    // unsaved document closed.
    bool visit(const Visited& place);
    // From one list to the other: where the cursor is goes on the other, and the last place still there is visited.
    void travel(std::vector<Visited>& from, std::vector<Visited>& to);

    olagarro::oui::Application& m_application;
    Editors& m_editors;
    std::function<void(const std::filesystem::path&)> m_open;
    std::vector<Visited> m_back;
    std::vector<Visited> m_forward;
    std::optional<Visited> m_last;
};
