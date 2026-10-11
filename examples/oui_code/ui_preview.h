#pragma once

#include "interactive_view.h"
#include "ui_skeleton.h"

#include <oui/combo_box.h>
#include <oui/container.h>
#include <oui/text_edit.h>

#include <signalslot/signalslot.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

// What a new description in Tools > Interactive UI starts with: a dialog to change, which says where to read on.
extern const std::string_view interactive_ui_template;

// A user interface description and what it builds, in one tab, as Tools > Interactive UI makes it: the description
// on the left, the interface on the right, to try as it would be used, and why it does not build along the bottom,
// which a click takes to. The description is a document as any other; after an error what built last stays, until it
// builds again. Its texts are translated when it names a context, as a program's are, and show as written when not,
// as a script's do; a Slot shows its id, where the code puts what goes there.
class UiPreviewView final : public olagarro::oui::Container
{
  public:
    explicit UiPreviewView(std::unique_ptr<olagarro::oui::TextEdit> code);

    [[nodiscard]] olagarro::oui::TextEdit& code();
    // Builds the description and shows what it makes, or says why it does not build.
    void show(std::string_view description);
    // What shows: none until a description builds.
    [[nodiscard]] olagarro::oui::Widget* shown();
    // Where the description is wrong, its line from 1, and what the line along the bottom says; none and "" while it
    // builds.
    [[nodiscard]] const std::optional<std::size_t>& error_line() const;
    [[nodiscard]] const std::string& error_text() const;

    // The error was clicked: its line of the description, from 1.
    template <typename Callback> olagarro::signal_slot::Connection on_error_chosen(Callback&& callback)
    {
        return m_error_chosen.connect(std::forward<Callback>(callback));
    }
    // Generate was pressed: code for the description is wanted, in the language beside it.
    template <typename Callback> olagarro::signal_slot::Connection on_generate(Callback&& callback)
    {
        return m_generate.connect(std::forward<Callback>(callback));
    }
    // The language Generate writes in, chosen beside it.
    [[nodiscard]] SkeletonLanguage language() const;
    // As remembered from before: not told as a choice.
    void set_language(SkeletonLanguage language);
    // Another language was chosen beside Generate.
    template <typename Callback> olagarro::signal_slot::Connection on_language_chosen(Callback&& callback)
    {
        return m_language_chosen.connect(std::forward<Callback>(callback));
    }

    // The script that runs after Generate, for the language chosen: its name, or none. Not told as a choice.
    void set_script(const std::optional<std::string>& name);
    // A script to run after Generate was asked for, in place of the one there is, if any; set_script says what
    // there is after.
    template <typename Callback> olagarro::signal_slot::Connection on_script_wanted(Callback&& callback)
    {
        return m_script_wanted.connect(std::forward<Callback>(callback));
    }
    // A new script to run after Generate was asked for, to write.
    template <typename Callback> olagarro::signal_slot::Connection on_script_created(Callback&& callback)
    {
        return m_script_created.connect(std::forward<Callback>(callback));
    }
    // No script, after Generate, was chosen.
    template <typename Callback> olagarro::signal_slot::Connection on_script_dropped(Callback&& callback)
    {
        return m_script_dropped.connect(std::forward<Callback>(callback));
    }

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override;
    void arrange(const olagarro::oui::Rect& bounds) override;

  private:
    olagarro::oui::TextEdit* m_code{};
    olagarro::oui::Container* m_area{};
    ErrorLine* m_error_view{};
    std::optional<std::size_t> m_error_line;
    std::string m_error_text;
    olagarro::signal_slot::Signal<std::size_t> m_error_chosen;
    olagarro::signal_slot::Signal<> m_generate;
    olagarro::oui::ComboBox* m_language{};
    // Set from the code: the box's change is no one's choice.
    bool m_setting_language{false};
    olagarro::signal_slot::Signal<SkeletonLanguage> m_language_chosen;
    olagarro::oui::ComboBox* m_script{};
    bool m_has_script{false};
    olagarro::signal_slot::Signal<> m_script_wanted;
    olagarro::signal_slot::Signal<> m_script_dropped;
    olagarro::signal_slot::Signal<> m_script_created;
};
