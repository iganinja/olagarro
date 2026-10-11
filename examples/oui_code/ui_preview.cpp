#include "ui_preview.h"

#include "ui_descriptions.h"

#include <oui/button.h>
#include <oui/declarative.h>
#include <oui/label.h>
#include <oui/painter.h>
#include <oui/theme.h>
#include <oui/translation.h>
#include <oui/ui.h>

#include <utility>
#include <vector>

using namespace olagarro;

namespace
{

// Where the interface shows: on the desktop's colour, as a window would. A window centres itself in it, as on the
// screen; anything else fills it.
class PreviewArea final : public oui::Container
{
  public:
    void set(std::unique_ptr<oui::Widget> shown)
    {
        if(child_count() != 0)
        {
            static_cast<void>(remove_at(0));
        }
        add(std::move(shown));
    }

    [[nodiscard]] oui::Size measure(const oui::Constraints& constraints) const override
    {
        return constraints.constrain({1, 1});
    }

    void arrange(const oui::Rect& bounds) override
    {
        Widget::arrange(bounds);
        if(child_count() != 0)
        {
            child_at(0).arrange(bounds);
        }
    }

    void paint(oui::Painter& painter, const oui::Theme& theme) const override
    {
        painter.fill_rect(bounds(), theme.background);
        Container::paint(painter, theme);
    }
};

} // namespace

UiPreviewView::UiPreviewView(std::unique_ptr<oui::TextEdit> code)
{
    auto ui{build_oui_code_ui(OuiCodeUi::interactive_ui)};
    m_code = &ui.put("code", std::move(code));
    m_area = &ui.put("preview", std::make_unique<PreviewArea>());
    m_error_view = &ui.put("error_line", std::make_unique<ErrorLine>(
                                             [this]
                                             {
                                                 if(m_error_line)
                                                 {
                                                     m_error_chosen.fire(*m_error_line);
                                                 }
                                             }));
    m_error_view->set_visible(false);
    static_cast<void>(ui.get<oui::Button>("generate").on_clicked([this] { m_generate.fire(); }));
    // The languages by their names, in the order SkeletonLanguage has them.
    m_language = &ui.get<oui::ComboBox>("language");
    m_language->set_options({"JavaScript", "C++"});
    static_cast<void>(m_language->on_selected(
        [this](std::size_t)
        {
            if(!m_setting_language)
            {
                m_language_chosen.fire(language());
            }
        }));
    // No script, the one there is, another to choose, and a new one to write; picking the one there is changes
    // nothing.
    m_script = &ui.get<oui::ComboBox>("script");
    set_script(std::nullopt);
    static_cast<void>(m_script->on_selected(
        [this](std::size_t index)
        {
            if(index + 1 == m_script->options().size())
            {
                m_script_created.fire();
            }
            else if(index + 2 == m_script->options().size())
            {
                m_script_wanted.fire();
            }
            else if(index == 0 && m_has_script)
            {
                m_script_dropped.fire();
            }
        }));
    add(ui.take());
}

void UiPreviewView::set_script(const std::optional<std::string>& name)
{
    m_has_script = name.has_value();
    std::vector<std::string> options{oui::translate("InteractiveUi", "No Script")};
    if(name)
    {
        options.push_back(*name);
    }
    options.push_back(oui::translate("InteractiveUi", "Choose..."));
    options.push_back(oui::translate("InteractiveUi", "New..."));
    m_script->set_options(std::move(options));
    m_script->set_selected(name ? 1 : 0);
}

SkeletonLanguage UiPreviewView::language() const
{
    return static_cast<SkeletonLanguage>(m_language->selected());
}

void UiPreviewView::set_language(SkeletonLanguage language)
{
    m_setting_language = true;
    m_language->set_selected(static_cast<std::size_t>(language));
    m_setting_language = false;
}

oui::TextEdit& UiPreviewView::code()
{
    return *m_code;
}

void UiPreviewView::show(std::string_view description)
{
    try
    {
        // A program's description names the context its texts are translated under; a script's shows them as
        // they are written.
        const auto parsed{oui::parse_oui(description)};
        const auto translated{parsed.root && parsed.root->has("context")};
        auto ui{oui::build_ui(description, translated ? oui::UiTexts::translated : oui::UiTexts::as_written)};
        std::vector<std::string> slots;
        for(const auto& [id, widget] : ui.widgets())
        {
            if(ui.is_slot(id))
            {
                slots.push_back(id);
            }
        }
        for(const auto& id : slots)
        {
            static_cast<void>(ui.put(id, oui::label("[" + id + "]")));
        }
        static_cast<PreviewArea*>(m_area)->set(ui.take());
        m_error_line.reset();
        m_error_text.clear();
        m_error_view->set_visible(false);
    }
    catch(const oui::UiError& error)
    {
        m_error_line = error.line();
        m_error_text =
            oui::format_text(oui::translate("InteractiveUi", "Line {0}, column {1}: {2}"),
                             {std::to_string(error.line()), std::to_string(error.column()), error.message()});
        m_error_view->set_text(m_error_text);
        m_error_view->set_visible(true);
    }
}

oui::Widget* UiPreviewView::shown()
{
    return m_area->child_count() == 0 ? nullptr : &m_area->child_at(0);
}

const std::optional<std::size_t>& UiPreviewView::error_line() const
{
    return m_error_line;
}

const std::string& UiPreviewView::error_text() const
{
    return m_error_text;
}

oui::Size UiPreviewView::measure(const oui::Constraints& constraints) const
{
    return constraints.constrain({1, 1});
}

void UiPreviewView::arrange(const oui::Rect& bounds)
{
    Widget::arrange(bounds);
    if(!children().empty())
    {
        children().front()->arrange(bounds);
    }
}

const std::string_view interactive_ui_template{
    R"(# A user interface, built on the right a moment after each change, to try as it would be used. A widget a line,
# its text in quotes and its properties after it; what goes in it indented under it. F1 says what there is.
Window "Rename Files" width=50
  Grid columns=content,1,fill
    Label "&Pattern:" buddy=pattern
    HSpacer
    LineEdit id=pattern placeholder="A regular expression"
    Label "Replace &with:" buddy=replacement
    HSpacer
    LineEdit id=replacement
  CheckButton "&Show what changes first" checked
  VSpacer
  Row centered gap=tight
    Button "&Rename" default
    Button "Cancel"
)"};
