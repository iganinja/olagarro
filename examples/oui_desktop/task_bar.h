#pragma once

#include "desktop_area.h"
#include "launcher.h"

#include <oui/desktop.h>
#include <oui/widget.h>

#include <string>
#include <vector>

// The bar along the bottom: a button for each program it starts, then a tab for each window, lit for the one with
// the keys; a click on it brings it to the top, or leaves it only in the bar when it was on top already.
class TaskBar final : public olagarro::oui::Widget
{
  public:
    TaskBar(DesktopArea& desktop, olagarro::oui::DesktopServer& server, std::vector<Launcher> launchers);

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override;
    void paint(olagarro::oui::Painter& painter, const olagarro::oui::Theme& theme) const override;
    bool handle_event(const olagarro::oui::Event& event) override;

  private:
    struct Entry
    {
        olagarro::oui::Rect bounds;
        int launcher{-1};
        int window{};
        std::string title;
    };

    // Where each button and tab is now: windows come and go, and change their titles.
    [[nodiscard]] std::vector<Entry> entries() const;

    DesktopArea& m_desktop;
    olagarro::oui::DesktopServer& m_server;
    std::vector<Launcher> m_launchers;
};
