#pragma once

#include <oui/application.h>
#include <oui/diff_view.h>
#include <oui/text_edit.h>
#include <oui/widget.h>

#include <signalslot/signalslot.h>

#include <filesystem>
#include <optional>
#include <string>

// A tab that compares two texts side by side, with Tools > Compare Files, Compare with Saved, or --diff.

// One side of a comparison: an open document, followed as it is edited, unsaved changes and all; or a file's text
// as it was read.
struct ComparedSide
{
    std::optional<std::filesystem::path> path;
    olagarro::oui::WidgetRef<olagarro::oui::TextEdit> editor;
    std::string text;
    // What the bar over the side calls it, as its whole path, and the tab, as its name.
    std::string title;
    std::string name;

    // The text the side has now.
    [[nodiscard]] std::string current_text() const;
};

// What keeps a comparison tab going: its view, its two sides, and compared again a moment after either open
// document changes.
struct Comparison
{
    olagarro::oui::DiffView* view{};
    ComparedSide left;
    ComparedSide right;
    // Compare with Saved's: the file as saved on the left, its document on the right.
    bool against_saved{false};
    olagarro::oui::Timer pending;
    olagarro::signal_slot::ScopedConnection left_changed;
    olagarro::signal_slot::ScopedConnection right_changed;
};
