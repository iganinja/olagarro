#pragma once

#include <oui/application.h>
#include <oui/translation.h>

#include <signalslot/signalslot.h>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace olagarro::oui
{

class List;
class Window;

// A pop-up list for picking one of several titles, such as the open tabs, from the keyboard. It must
// outlive the pop-up it opens.
class TabSwitcher final
{
  public:
    // "Tab List" by default.
    void set_title(std::string title);
    void open(Application& application, const std::vector<std::string>& titles, std::size_t selected);
    // Goes through `titles`, the tabs from the most recently used, as editors do on Ctrl+Tab: the first call opens
    // the list on the next title (`step` 1) or the last one (-1), and each call after moves on by `step`. Letting
    // Ctrl go picks the one marked, where the backend can tell; otherwise a moment without another call does.
    // Enter picks it as well, and Escape gives up.
    void cycle(Application& application, const std::vector<std::string>& titles, int step);
    [[nodiscard]] bool is_open() const;

    template <typename Callback> olagarro::signal_slot::Connection on_confirmed(Callback&& callback)
    {
        return m_confirmed.connect(std::forward<Callback>(callback));
    }
    template <typename Callback> olagarro::signal_slot::Connection on_cancelled(Callback&& callback)
    {
        return m_cancelled.connect(std::forward<Callback>(callback));
    }

  private:
    void move(int step);
    void pick();
    // Picks the title marked once the moment for another step is over, where Ctrl cannot be heard letting go.
    void wait_for_pick();

    std::string m_title{translate("TabSwitcher", "Tab List")};
    bool m_open{false};
    Application* m_application{};
    Window* m_dialog{};
    List* m_list{};
    bool m_cycling{false};
    bool m_listens_to_modifiers{false};
    Timer m_pick_timer;
    olagarro::signal_slot::Signal<std::size_t> m_confirmed;
    olagarro::signal_slot::Signal<> m_cancelled;
};

[[nodiscard]] std::unique_ptr<TabSwitcher> tab_switcher();

} // namespace olagarro::oui
