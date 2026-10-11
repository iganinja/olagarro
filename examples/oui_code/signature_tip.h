#pragma once

#include "completion.h"

#include <oui/widget.h>

#include <signalslot/signalslot.h>

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

// How the call the cursor is in is called, shown over its line with Application::show_popup while one writes its
// arguments: the signature, the parameter the cursor's argument goes to marked, and what it does. Typing goes on in
// the document; it takes only Escape, to close.
class SignatureTip final : public olagarro::oui::Widget
{
  public:
    // Over `anchor`, the cell of the name called, or under it when there is no room above.
    explicit SignatureTip(olagarro::oui::Point anchor);

    void show(Signature signature, std::size_t argument);
    void set_anchor(olagarro::oui::Point anchor);
    [[nodiscard]] const Signature& signature() const;
    // The parameter marked, when the argument goes to one.
    [[nodiscard]] std::optional<std::size_t> parameter() const;

    // Escape was pressed.
    template <typename Callback> olagarro::signal_slot::Connection on_cancelled(Callback&& callback)
    {
        return m_cancelled.connect(std::forward<Callback>(callback));
    }

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override;
    void arrange(const olagarro::oui::Rect& screen) override;
    void paint(olagarro::oui::Painter& painter, const olagarro::oui::Theme& theme) const override;
    bool handle_event(const olagarro::oui::Event& event) override;

  private:
    // A row as shown: a stretch of the signature, or of a line of what it does.
    struct Row
    {
        bool signature;
        std::size_t line;
        std::size_t start;
        std::size_t end;
    };

    // The rows, no wider than `width` cells.
    [[nodiscard]] std::vector<Row> rows(int width) const;

    olagarro::oui::Point m_anchor;
    Signature m_signature;
    std::size_t m_argument{};
    std::vector<Row> m_rows;
    olagarro::signal_slot::Signal<> m_cancelled;
};
