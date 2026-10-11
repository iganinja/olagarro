#pragma once

#include <oui/list.h>

#include <signalslot/signalslot.h>

#include <cstddef>
#include <memory>
#include <string_view>
#include <utility>

namespace olagarro::oui
{

// A list whose rows each have a box before their text, to tick, as a list of things to do: a click on a row's box,
// or Space on the row chosen, ticks it or unticks it, and on_toggled says so. A row's box is the mark of its label,
// drawn as a box in a window and as "[x]" or "[ ]" in a terminal.
class CheckList final : public List
{
  public:
    // How many columns the box takes, before the text.
    static constexpr int box_columns{4};

    void add_item(std::string_view text, bool checked);
    [[nodiscard]] bool is_checked(std::size_t row) const;
    // As the code sets it, which on_toggled does not hear.
    void set_checked(std::size_t row, bool checked);
    // A box was ticked or unticked by the person: its row, and whether it is ticked now.
    template <typename Callback> olagarro::signal_slot::Connection on_toggled(Callback&& callback)
    {
        return m_toggled.connect(std::forward<Callback>(callback));
    }

    bool handle_event(const Event& event) override;

  private:
    void toggle(std::size_t row);

    olagarro::signal_slot::Signal<std::size_t, bool> m_toggled;
};

[[nodiscard]] std::unique_ptr<CheckList> check_list();

} // namespace olagarro::oui
