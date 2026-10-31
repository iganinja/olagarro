#include <oui/application.h>
#include <oui/button.h>
#include <oui/cell_buffer.h>
#include <oui/check_button.h>
#include <oui/color.h>
#include <oui/graphics/graphical_painter.h>
#include <oui/label.h>
#include <oui/layout.h>
#include <oui/line_edit.h>
#include <oui/list.h>
#include <oui/tab_view.h>
#include <oui/tab_switcher.h>
#include <oui/text_edit.h>
#include <oui/theme.h>
#include <oui/tree_view.h>
#include <oui/turbo_c_theme.h>
#include <oui/window.h>

#include <gtest/gtest.h>

#include "../../oui/src/terminal_backend_helpers.h"

#include <cmath>
#include <limits>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace olagarro;

namespace
{

class MemoryBackend final : public oui::Backend
{
  public:
    [[nodiscard]] oui::Size size() const override { return {60, 12}; }

    std::optional<oui::Event> poll_event() override { return std::nullopt; }

    void present(const oui::CellBuffer& buffer) override { m_last_frame = buffer; }

    [[nodiscard]] const std::optional<oui::CellBuffer>& last_frame() const { return m_last_frame; }

  private:
    std::optional<oui::CellBuffer> m_last_frame;
};

[[nodiscard]] int luma(const oui::Color& color)
{
    return (color.red * 299 + color.green * 587 + color.blue * 114) / 1000;
}

[[nodiscard]] bool clearly_readable(const oui::Color& foreground, const oui::Color& background)
{
    return foreground != background &&
           (std::abs(luma(foreground) - luma(background)) >= 96 ||
            (std::abs(foreground.red - background.red) + std::abs(foreground.green - background.green) +
                 std::abs(foreground.blue - background.blue) >=
             384));
}

[[nodiscard]] oui::Theme application_theme()
{
    oui::Theme theme{};
    theme.background = {0, 0, 170};
    theme.panel_background = {0, 0, 85};
    theme.control_background = {0, 0, 170};
    theme.shadow = {0, 0, 0};
    theme.text = {170, 170, 170};
    theme.muted_text = {85, 85, 85};
    theme.focus_background = {255, 255, 255};
    theme.focus_text = {0, 0, 0};
    theme.dialog_background = {170, 170, 170};
    theme.dialog_foreground = {0, 0, 0};
    theme.dialog_border = {255, 255, 255};
    theme.menu_background = {170, 170, 170};
    theme.menu_foreground = {0, 0, 0};
    theme.menu_border = {255, 255, 255};
    theme.menu_selected_background = {0, 0, 170};
    theme.menu_selected_foreground = {255, 255, 255};
    theme.button_background = {85, 255, 85};
    theme.button_foreground = {0, 0, 0};
    theme.button_focused_background = {0, 0, 170};
    theme.button_focused_foreground = {255, 255, 255};
    theme.button_pressed_background = {0, 128, 0};
    theme.button_pressed_foreground = {255, 255, 255};
    theme.tab_active_background = {0, 0, 170};
    theme.tab_active_foreground = {255, 255, 255};
    theme.tab_inactive_background = {0, 0, 85};
    theme.tab_inactive_foreground = {170, 170, 170};
    theme.editor_background = {0, 0, 170};
    theme.editor_foreground = {170, 170, 170};
    theme.input_background = {0, 0, 170};
    theme.input_foreground = {255, 255, 255};
    theme.disabled_foreground = {85, 85, 85};
    theme.hotkey_foreground = {170, 0, 0};
    return theme;
}

void expect_no_unreadable_text(const oui::CellBuffer& buffer, const std::string& context)
{
    for(int y{}; y < buffer.height(); ++ y)
    {
        for(int x{}; x < buffer.width(); ++ x)
        {
            const auto& cell{buffer.cell_at(x, y)};
            if(cell.glyph == U' ' || cell.continuation)
            {
                continue;
            }
            const auto readable{cell.foreground != cell.background &&
                                std::abs(luma(cell.foreground) - luma(cell.background)) >= 24};
            EXPECT_TRUE(readable) << context << ": unreadable glyph " << static_cast<char>(cell.glyph) << " at (" << x
                                  << ", " << y << ") fg={" << static_cast<int>(cell.foreground.red) << ","
                                  << static_cast<int>(cell.foreground.green) << ","
                                  << static_cast<int>(cell.foreground.blue) << "} bg={"
                                  << static_cast<int>(cell.background.red) << ","
                                  << static_cast<int>(cell.background.green) << ","
                                  << static_cast<int>(cell.background.blue) << "}";
        }
    }
}

} // namespace

TEST(OuiContrastTests, SelectionColorsDifferFromEverySurface)
{
    const oui::Theme theme;
    EXPECT_TRUE(clearly_readable(theme.selection_foreground, theme.selection_background));
    const std::vector<std::pair<std::string, oui::Color>> surfaces{{"editor", theme.editor_background},
                                                                   {"input", theme.input_background},
                                                                   {"dialog", theme.dialog_background},
                                                                   {"panel", theme.panel_background},
                                                                   {"tab active", theme.tab_active_background},
                                                                   {"tab inactive", theme.tab_inactive_background},
                                                                   {"menu", theme.menu_background},
                                                                   {"button", theme.button_background},
                                                                   {"button pressed", theme.button_pressed_background}};

    for(const auto& [name, surface] : surfaces)
    {
        EXPECT_NE(theme.selection_background, surface) << "selection background matches the " << name << " surface";
    }
}

TEST(OuiContrastTests, ButtonKeepsItsLabelReadableInEveryState)
{
    auto button_widget{oui::button("OK")};
    button_widget->arrange({0, 0, 20, 3});
    const auto& bounds{button_widget->bounds()};

    oui::CellBuffer normal_buffer{20, 3};
    oui::CellBufferPainter normal_painter{normal_buffer};
    button_widget->paint(normal_painter, {});
    expect_no_unreadable_text(normal_buffer, "normal button");

    static_cast<void>(button_widget->handle_event(
        oui::MouseEvent{bounds.x + std::floor(bounds.width / 2), bounds.y + std::floor(bounds.height / 2), true}));
    oui::CellBuffer pressed_buffer{20, 3};
    oui::CellBufferPainter pressed_painter{pressed_buffer};
    button_widget->paint(pressed_painter, {});
    expect_no_unreadable_text(pressed_buffer, "pressed button");
}

TEST(OuiContrastTests, CheckedGlyphIsClearlyVisibleAgainstTheDialogSurface)
{
    auto check{oui::check_button("Enable grid")};
    check->set_checked(true);
    check->arrange({0, 0, 20, 1});

    oui::CellBuffer buffer{20, 1};
    oui::CellBufferPainter painter{buffer};
    check->paint(painter, {});

    const auto& glyph_cell{buffer.cell_at(1, 0)};
    EXPECT_EQ(glyph_cell.glyph, U'X');
    EXPECT_TRUE(clearly_readable(glyph_cell.foreground, oui::Theme{}.cluster_background))
        << "the checked glyph blends into the check box's background";
    expect_no_unreadable_text(buffer, "checked button");
}

TEST(OuiContrastTests, CheckButtonLeavesAGapBetweenTheBoxAndItsText)
{
    auto check{oui::check_button("Enable grid")};
    for(const bool state : {false, true})
    {
        check->set_checked(state);
        check->arrange({0, 0, 20, 1});
        oui::CellBuffer buffer{20, 1};
        oui::CellBufferPainter painter{buffer};
        check->paint(painter, {});

        EXPECT_EQ(buffer.cell_at(3, 0).glyph, U' ') << "missing gap before the label (state=" << state << ")";
        EXPECT_EQ(buffer.cell_at(4, 0).glyph, U'E') << "label does not start after the gap (state=" << state << ")";
    }
}

TEST(OuiContrastTests, ListSelectionStandsOutFromUnselectedRows)
{
    const oui::Theme theme;
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_reference{backend.get()};
    oui::Application application{std::move(backend)};
    auto list{oui::list()};
    list->add(oui::label("First"));
    list->add(oui::label("Second"));
    list->add(oui::label("Third"));
    auto& list_reference{*list};
    application.set_content(std::move(list));
    static_cast<void>(list_reference.handle_event(oui::KeyEvent{oui::Key::down}));
    application.render();

    ASSERT_TRUE(backend_reference->last_frame().has_value());
    const auto& frame{*backend_reference->last_frame()};

    const auto x{oui::whole(list_reference.bounds().x)};
    const auto y{oui::whole(list_reference.bounds().y)};
    EXPECT_EQ(frame.cell_at(x + 1, y + 1).background, theme.item_selected_background);
    EXPECT_NE(frame.cell_at(x + 1, y).background, theme.item_selected_background);
}

TEST(OuiContrastTests, TreeViewSelectionStandsOutFromBackground)
{
    const oui::Theme theme;
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_reference{backend.get()};
    oui::Application application{std::move(backend)};
    auto tree{std::make_unique<oui::TreeView>()};
    static_cast<void>(tree->add("alpha"));
    static_cast<void>(tree->add("beta"));
    auto& tree_reference{*tree};
    application.set_content(std::move(tree));
    std::size_t activated{std::numeric_limits<std::size_t>::max()};
    tree_reference.on_activated([&activated](std::size_t index) { activated = index; });
    static_cast<void>(tree_reference.handle_event(oui::KeyEvent{oui::Key::down}));
    static_cast<void>(tree_reference.handle_event(oui::KeyEvent{oui::Key::enter}));
    EXPECT_EQ(activated, 1u);
    static_cast<void>(tree_reference.handle_event(oui::KeyEvent{oui::Key::down}));
    application.render();

    ASSERT_TRUE(backend_reference->last_frame().has_value());
    const auto& frame{*backend_reference->last_frame()};

    EXPECT_TRUE(tree_reference.is_focused());
    const auto x{oui::whole(tree_reference.bounds().x)};
    const auto y{oui::whole(tree_reference.bounds().y)};
    EXPECT_NE(frame.cell_at(x + 4, y).background, theme.item_selected_background)
        << "highlight stayed on the first row; Key::down did not move the selection";
    EXPECT_EQ(frame.cell_at(x + 4, y + 1).background, theme.item_selected_background);
    EXPECT_NE(frame.cell_at(x + 4, y).background, theme.item_selected_background);
}

TEST(OuiContrastTests, TabViewActiveTabDiffersFromInactiveTabs)
{
    const oui::Theme theme;
    auto tabs{oui::tab_view()};
    static_cast<void>(&tabs->add_tab("One", oui::label("first")));
    static_cast<void>(&tabs->add_tab("Two", oui::label("second")));
    tabs->arrange({0, 0, 40, 8});

    oui::CellBuffer buffer{40, 8};
    oui::CellBufferPainter painter{buffer};
    tabs->paint(painter, theme);

    bool saw_active{};
    bool saw_inactive{};
    for(int x{}; x < buffer.width(); ++ x)
    {
        const auto& cell{buffer.cell_at(x, 0)};
        if(cell.background == theme.tab_active_background)
        {
            saw_active = true;
        }
        if(cell.background == theme.tab_inactive_background)
        {
            saw_inactive = true;
        }
    }
    EXPECT_TRUE(saw_active);
    EXPECT_TRUE(saw_inactive);
    EXPECT_NE(theme.tab_active_background, theme.tab_inactive_background);
    expect_no_unreadable_text(buffer, "tab view");
}

TEST(OuiContrastTests, TurboCTabListKeepsSelectedAndUnselectedTitlesReadable)
{
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_reference{backend.get()};
    const auto theme{oui::TurboCTheme::make()};
    oui::Application application{std::move(backend), theme};
    application.set_content(oui::button("Background"));

    oui::TabSwitcher switcher;
    switcher.open(application, {"Current file", "Other file"}, 0);
    application.render();

    ASSERT_TRUE(backend_reference->last_frame().has_value());
    const auto& frame{*backend_reference->last_frame()};
    bool saw_selected_title{};
    bool saw_unselected_title{};
    for(int y{}; y < frame.height(); ++ y)
    {
        for(int x{}; x < frame.width(); ++ x)
        {
            const auto& cell{frame.cell_at(x, y)};
            if(cell.glyph == U'C')
            {
                saw_selected_title = true;
                EXPECT_EQ(cell.foreground, theme.item_selected_foreground);
                EXPECT_EQ(cell.background, theme.item_selected_background);
            }
            if(cell.glyph == U'O')
            {
                saw_unselected_title = true;
                EXPECT_EQ(cell.foreground, theme.list_foreground);
                EXPECT_EQ(cell.background, theme.list_background);
            }
        }
    }
    EXPECT_TRUE(saw_selected_title);
    EXPECT_TRUE(saw_unselected_title);
    expect_no_unreadable_text(frame, "Turbo C tab list");
}

TEST(OuiContrastTests, LineEditSelectionAndCursorRemainVisibleWhenFocused)
{
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_reference{backend.get()};
    oui::Application application{std::move(backend)};
    auto editor{oui::line_edit()};
    editor->set_text("abcdef");
    auto& editor_reference{*editor};
    application.set_content(std::move(editor));
    application.render();

    const auto& bounds{editor_reference.bounds()};
    static_cast<void>(editor_reference.handle_event(oui::MouseEvent{bounds.x + 1, bounds.y, true}));
    static_cast<void>(editor_reference.handle_event(oui::MouseEvent{bounds.x + 4, bounds.y, false, 0, true}));
    application.render();

    ASSERT_TRUE(backend_reference->last_frame().has_value());
    const auto& frame{*backend_reference->last_frame()};
    const oui::Theme theme;

    bool saw_cursor{};
    bool saw_selection{};
    for(int x{oui::whole(bounds.x)}; x < oui::whole(bounds.x + bounds.width); ++ x)
    {
        const auto& cell{frame.cell_at(x, oui::whole(bounds.y))};
        if(cell.background == theme.focus_text)
        {
            saw_cursor = true;
        }
        if(cell.foreground == theme.focus_background && cell.background == theme.focus_text)
        {
            saw_selection = true;
        }
    }
    EXPECT_TRUE(saw_cursor) << "the line edit cursor is invisible while focused";
    EXPECT_TRUE(saw_selection) << "the focused line edit selection does not use the inverted focus colors";
}

TEST(OuiContrastTests, TextEditRendersGutterTabsAndSelectionWithoutCollisions)
{
    const oui::Theme theme;
    oui::TextEdit editor;
    editor.set_line_numbers_visible(true);
    editor.set_text("int main()\n\treturn 0;");
    editor.arrange({0, 0, 40, 3});
    static_cast<void>(editor.handle_event(oui::MouseEvent{0, 1, true}));
    static_cast<void>(editor.handle_event(oui::MouseEvent{9, 1, false, 0, true}));

    oui::CellBuffer buffer{40, 3};
    oui::CellBufferPainter painter{buffer};
    editor.paint(painter, theme);

    EXPECT_EQ(buffer.cell_at(4, 1).background, theme.selection_background);
    expect_no_unreadable_text(buffer, "text edit");
}

TEST(OuiContrastTests, CompositeDialogHasNoUnreadableText)
{
    auto dialog{oui::window("Settings")};
    dialog->set_border(oui::WindowBorder::none);
    auto& form{dialog->add(oui::column())};
    form.add(oui::label("Profile name:"));
    form.add(oui::line_edit());
    form.add(oui::check_button("Use spaces"));
    oui::button("OK", form);

    dialog->arrange({0, 0, 40, 10});
    oui::CellBuffer buffer{40, 10};
    oui::CellBufferPainter painter{buffer};
    dialog->paint(painter, {});

    expect_no_unreadable_text(buffer, "settings dialog");
}

TEST(OuiContrastTests, FocusedLineEditStandsOutFromItsRestingState)
{
    const oui::Theme theme;
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_reference{backend.get()};
    oui::Application application{std::move(backend)};
    auto edit{oui::line_edit()};
    edit->set_text("8");
    auto& edit_reference{*edit};
    application.set_content(std::move(edit));
    application.render();

    ASSERT_TRUE(backend_reference->last_frame().has_value());
    const auto& frame{*backend_reference->last_frame()};

    EXPECT_TRUE(edit_reference.is_focused());
    const auto x{oui::whole(edit_reference.bounds().x)};
    const auto y{oui::whole(edit_reference.bounds().y)};
    const auto& focused_cell{frame.cell_at(x + 3, y)};
    EXPECT_EQ(focused_cell.background, theme.focus_background);
    EXPECT_NE(focused_cell.background, theme.input_background);
    const auto& glyph_cell{frame.cell_at(x, y)};
    EXPECT_EQ(glyph_cell.glyph, U'8');
    EXPECT_TRUE(clearly_readable(glyph_cell.foreground, glyph_cell.background));
    expect_no_unreadable_text(frame, "focused line edit");

    auto resting{oui::line_edit()};
    resting->set_text("8");
    resting->arrange({0, 0, 20, 1});
    oui::CellBuffer resting_buffer{20, 1};
    oui::CellBufferPainter resting_painter{resting_buffer};
    resting->paint(resting_painter, theme);
    EXPECT_EQ(resting_buffer.cell_at(1, 0).background, theme.input_background);
}

TEST(OuiContrastTests, FocusedCheckButtonShowsItsFocusInItsText)
{
    const oui::Theme theme;
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_reference{backend.get()};
    oui::Application application{std::move(backend)};
    auto check{oui::check_button("Use spaces for Tab")};
    auto& check_reference{*check};
    application.set_content(std::move(check));
    application.render();

    ASSERT_TRUE(backend_reference->last_frame().has_value());
    const auto& frame{*backend_reference->last_frame()};

    EXPECT_TRUE(check_reference.is_focused());
    const auto& box_cell{frame.cell_at(oui::whole(check_reference.bounds().x), oui::whole(check_reference.bounds().y))};
    // As a Turbo Vision cluster: the one with the focus in its own colour, on the group's.
    EXPECT_EQ(box_cell.background, theme.cluster_background);
    EXPECT_EQ(box_cell.foreground, theme.cluster_focused_foreground);
    EXPECT_NE(theme.cluster_focused_foreground, theme.cluster_foreground);
    EXPECT_TRUE(clearly_readable(box_cell.foreground, box_cell.background));
    expect_no_unreadable_text(frame, "focused check button");
}

TEST(OuiContrastTests, FocusedButtonUsesTheFocusPalette)
{
    const oui::Theme theme;
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_reference{backend.get()};
    oui::Application application{std::move(backend)};
    auto ok{oui::button("OK")};
    auto& ok_reference{*ok};
    application.set_content(std::move(ok));
    application.render();

    ASSERT_TRUE(backend_reference->last_frame().has_value());
    const auto& frame{*backend_reference->last_frame()};

    EXPECT_TRUE(ok_reference.is_focused());
    const auto& button_bounds{ok_reference.bounds()};
    const oui::Cell* label_cell{nullptr};
    for(int x{oui::whole(button_bounds.x)};
        x < oui::whole(button_bounds.x + button_bounds.width) && label_cell == nullptr; ++ x)
    {
        const auto& cell{frame.cell_at(x, oui::whole(button_bounds.y))};
        if(cell.glyph != U' ')
        {
            label_cell = &cell;
        }
    }
    ASSERT_NE(label_cell, nullptr);
    EXPECT_EQ(label_cell->background, theme.button_focused_background);
    EXPECT_NE(label_cell->background, theme.button_background);
    EXPECT_TRUE(clearly_readable(label_cell->foreground, label_cell->background));
}

TEST(OuiContrastTests, FocusedLineEditKeepsItsSelectionReadable)
{
    const oui::Theme theme;
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_reference{backend.get()};
    oui::Application application{std::move(backend)};
    auto editor{oui::line_edit()};
    editor->set_text("abcdef");
    auto& editor_reference{*editor};
    application.set_content(std::move(editor));
    application.render();
    const auto& bounds{editor_reference.bounds()};
    static_cast<void>(editor_reference.handle_event(oui::MouseEvent{bounds.x + 1, bounds.y, true}));
    static_cast<void>(editor_reference.handle_event(oui::MouseEvent{bounds.x + 4, bounds.y, false, 0, true}));
    application.render();

    ASSERT_TRUE(backend_reference->last_frame().has_value());
    const auto& frame{*backend_reference->last_frame()};

    const auto left{oui::whole(bounds.x)};
    for(int x{left + 2}; x < left + 4; ++ x)
    {
        const auto& cell{frame.cell_at(x, oui::whole(bounds.y))};
        EXPECT_EQ(cell.foreground, theme.item_selected_foreground) << "selected glyph at offset " << x - left;
        EXPECT_EQ(cell.background, theme.item_selected_background) << "selected glyph at offset " << x - left;
        EXPECT_TRUE(clearly_readable(cell.foreground, cell.background));
    }
}

TEST(OuiContrastTests, FocusColorsStandOutOnTheApplicationPalette)
{
    oui::Theme app_theme{};
    app_theme.dialog_background = {170, 170, 170};
    app_theme.input_background = {0, 0, 170};
    app_theme.button_background = {85, 255, 85};
    app_theme.focus_background = {255, 255, 255};
    app_theme.focus_text = {0, 0, 0};

    const std::vector<std::pair<std::string, oui::Color>> resting_surfaces{{"input", app_theme.input_background},
                                                                           {"dialog", app_theme.dialog_background},
                                                                           {"button", app_theme.button_background}};
    for(const auto& [name, surface] : resting_surfaces)
    {
        EXPECT_NE(app_theme.focus_background, surface) << "focus background matches the resting " << name << " color";
    }
    EXPECT_TRUE(clearly_readable(app_theme.focus_text, app_theme.focus_background));

    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_reference{backend.get()};
    oui::Application application{std::move(backend)};
    application.set_theme(app_theme);
    auto editor{oui::line_edit()};
    editor->set_text("8");
    auto& editor_reference{*editor};
    application.set_content(std::move(editor));
    application.render();

    ASSERT_TRUE(backend_reference->last_frame().has_value());
    const auto& frame{*backend_reference->last_frame()};
    EXPECT_EQ(
        frame.cell_at(oui::whole(editor_reference.bounds().x) + 3, oui::whole(editor_reference.bounds().y)).background,
        app_theme.focus_background);
}

TEST(OuiContrastTests, LabelColorOverrideReplacesTheForeground)
{
    const oui::Theme theme;
    const oui::Color accent{0, 0, 170};

    oui::Label plain{"Editor"};
    plain.arrange({0, 0, 20, 1});
    oui::CellBuffer plain_buffer{20, 1};
    oui::CellBufferPainter plain_painter{plain_buffer};
    plain.paint(plain_painter, theme);
    EXPECT_EQ(plain_buffer.cell_at(0, 0).foreground, theme.dialog_foreground);

    oui::Label tinted{"Editor"};
    tinted.set_color(accent);
    tinted.arrange({0, 0, 20, 1});
    oui::CellBuffer tinted_buffer{20, 1};
    oui::CellBufferPainter tinted_painter{tinted_buffer};
    tinted.paint(tinted_painter, theme);
    EXPECT_EQ(tinted_buffer.cell_at(0, 0).foreground, accent);
    EXPECT_EQ(tinted_buffer.cell_at(0, 0).background, theme.dialog_background);
    EXPECT_TRUE(clearly_readable(accent, theme.dialog_background));
}

TEST(OuiContrastTests, TabTitlesRemainReadableOnTheApplicationPalette)
{
    const auto theme{application_theme()};
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_reference{backend.get()};
    oui::Application application{std::move(backend)};
    application.set_theme(theme);
    auto tabs{oui::tab_view()};
    tabs->add_tab("First", oui::label("one"));
    tabs->add_tab("Second", oui::label("two"));
    tabs->add_tab("Third", oui::label("three"));
    auto& tabs_reference{*tabs};
    application.set_content(std::move(tabs));
    application.render();

    ASSERT_TRUE(backend_reference->last_frame().has_value());
    const auto& frame{*backend_reference->last_frame()};

    ASSERT_FALSE(tabs_reference.bounds().width < 20) << "the tab header did not fit";
    auto saw_bold_title{false};
    const auto& tabs_bounds{tabs_reference.bounds()};
    for(auto x{oui::whole(tabs_bounds.x)}; x < oui::whole(tabs_bounds.x + tabs_bounds.width); ++ x)
    {
        const auto& cell{frame.cell_at(x, oui::whole(tabs_bounds.y))};
        if(cell.glyph != U' ')
        {
            EXPECT_TRUE(clearly_readable(cell.foreground, cell.background))
                << "unreadable title cell at column " << x << " (fg " << static_cast<int>(cell.foreground.red) << ","
                << static_cast<int>(cell.foreground.green) << "," << static_cast<int>(cell.foreground.blue) << " bg "
                << static_cast<int>(cell.background.red) << "," << static_cast<int>(cell.background.green) << ","
                << static_cast<int>(cell.background.blue) << ")";
            if((cell.attributes & oui::text_bold) != 0)
            {
                saw_bold_title = true;
            }
        }
    }
    EXPECT_TRUE(saw_bold_title) << "the selected tab title is not bold";
}

TEST(OuiContrastTests, LegacyQuantizationKeepsTabTitleColorsDistinct)
{
    const auto theme{application_theme()};
    // The same of the sixteen for a text and what it is on would leave the text unseen.
    using oui::terminal_helpers::console_index;
    EXPECT_NE(console_index(theme.tab_active_foreground), console_index(theme.tab_active_background))
        << "the active tab collapses in the legacy console palette";
    EXPECT_NE(console_index(theme.tab_inactive_foreground), console_index(theme.tab_inactive_background))
        << "an inactive tab collapses in the legacy console palette";

    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_reference{backend.get()};
    oui::Application application{std::move(backend)};
    application.set_theme(theme);
    auto tabs{oui::tab_view()};
    tabs->add_tab("Alpha", oui::label("a"));
    tabs->add_tab("Beta", oui::label("b"));
    auto& tabs_reference{*tabs};
    application.set_content(std::move(tabs));
    application.render();

    ASSERT_TRUE(backend_reference->last_frame().has_value());
    const auto& frame{*backend_reference->last_frame()};
    const auto& tabs_bounds{tabs_reference.bounds()};
    for(auto x{oui::whole(tabs_bounds.x)}; x < oui::whole(tabs_bounds.x + tabs_bounds.width); ++ x)
    {
        const auto& cell{frame.cell_at(x, oui::whole(tabs_bounds.y))};
        if(cell.glyph != U' ')
        {
            EXPECT_NE(oui::terminal_helpers::console_index(cell.foreground),
                      oui::terminal_helpers::console_index(cell.background))
                << "title text at column " << x << " would be invisible on a legacy console";
        }
    }
}

TEST(OuiContrastTests, FocusedButtonStandsOutFromItsRestingState)
{
    const auto theme{application_theme()};
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_reference{backend.get()};
    oui::Application application{std::move(backend)};
    application.set_theme(theme);
    auto button{oui::button("Save")};
    auto& button_reference{*button};
    application.set_content(std::move(button));
    application.render();

    ASSERT_TRUE(backend_reference->last_frame().has_value());
    const auto& frame{*backend_reference->last_frame()};

    EXPECT_TRUE(button_reference.is_focused());
    const auto& button_bounds{button_reference.bounds()};
    const auto right{oui::whole(button_bounds.x + button_bounds.width)};
    const auto row{oui::whole(button_bounds.y)};
    auto label_x{right};
    for(auto x{oui::whole(button_bounds.x)}; x < right; ++ x)
    {
        if(frame.cell_at(x, row).glyph != U' ')
        {
            label_x = x;
            break;
        }
    }
    ASSERT_LT(label_x, right) << "the focused button has no visible label";
    const auto& focused_cell{frame.cell_at(label_x, row)};
    EXPECT_EQ(focused_cell.background, theme.button_focused_background);
    EXPECT_NE(focused_cell.background, theme.button_background);
    EXPECT_NE(focused_cell.background, theme.dialog_background);
    EXPECT_EQ(focused_cell.foreground, theme.button_focused_foreground);
    EXPECT_TRUE(clearly_readable(focused_cell.foreground, focused_cell.background));
}

TEST(OuiContrastTests, OccurrencesOfTheSelectionStandOutInEveryTheme)
{
    for(const auto& theme : {oui::Theme{}, oui::TurboCTheme::make(), oui::classic_theme()})
    {
        EXPECT_TRUE(clearly_readable(theme.occurrence_foreground, theme.occurrence_background));
        EXPECT_NE(theme.occurrence_background, theme.editor_background);
        EXPECT_NE(theme.occurrence_background, theme.selection_background);
        EXPECT_NE(theme.occurrence_background, theme.search_match_background);
    }
}

TEST(OuiTextEditTests, MarksTheOtherPlacesWithTheSelectedText)
{
    auto backend{std::make_unique<MemoryBackend>()};
    auto* const backend_reference{backend.get()};
    const oui::Theme theme;
    oui::Application application{std::move(backend), theme};
    auto edit{oui::text_edit()};
    auto& editor{*edit};
    application.set_content(std::move(edit));
    editor.set_text("foo bar foo\nfoo");
    editor.set_occurrences_shown(true);
    const auto background_at{[&application, backend_reference](int x, int y)
                             {
                                 application.render();
                                 return backend_reference->last_frame()->cell_at(x, y).background;
                             }};

    editor.select_range(0, 3);
    EXPECT_EQ(background_at(0, 0), theme.selection_background);
    EXPECT_EQ(background_at(8, 0), theme.occurrence_background) << "the second foo";
    EXPECT_EQ(background_at(0, 1), theme.occurrence_background) << "and the one on the next line";
    EXPECT_EQ(background_at(4, 0), theme.editor_background) << "bar is not foo";

    editor.select_range(8, 13);
    EXPECT_EQ(background_at(0, 0), theme.editor_background) << "a selection over two lines marks nothing";
    editor.select_range(3, 3);
    EXPECT_EQ(background_at(8, 0), theme.editor_background) << "nor does no selection";
    editor.set_occurrences_shown(false);
    editor.select_range(0, 3);
    EXPECT_EQ(background_at(8, 0), theme.editor_background) << "nor anything, unless asked";
}
