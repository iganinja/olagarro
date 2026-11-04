#include <paint/panels/color_panel.h>
#include <paint/panels/icons.h>
#include <paint/panels/palette_library.h>
#include <paint/panels/panel_translations.h>
#include <paint/panels/tool_options_bar.h>
#include <paint/panels/tool_panel.h>

#include <oui/check_button.h>
#include <oui/command.h>
#include <oui/event.h>
#include <oui/layout.h>
#include <oui/line_edit.h>
#include <oui/menu_bar.h>
#include <oui/number_field.h>
#include <oui/slider.h>
#include <oui/translation.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

using namespace olagarro;

namespace
{

oui::MouseEvent press(double x, double y, bool right = false)
{
    oui::MouseEvent event{x, y, true};
    event.right = right;
    return event;
}

// The cell of the button at `index` in a tool panel arranged at (left, top).
oui::MouseEvent press_button(std::size_t index, double left, double top)
{
    const auto column{static_cast<double>(index % paint::ToolPanel::columns)};
    const auto row{static_cast<double>(index / paint::ToolPanel::columns)};
    return press(left + column * paint::ToolPanel::button_width + 1, top + row);
}

std::size_t index_of(const paint::ToolPanel& panel, const std::string& id)
{
    for(std::size_t index{}; index < panel.buttons().size(); ++ index)
    {
        if(panel.buttons()[index].id == id)
        {
            return index;
        }
    }
    ADD_FAILURE() << "no button " << id;
    return 0;
}

bool same(const paint::Color& one, const paint::Color& other)
{
    return one.red == other.red && one.green == other.green && one.blue == other.blue && one.alpha == other.alpha;
}

// The bar's width field and slider: its first group's.
oui::Container& width_group(paint::ToolOptionsBar& bar)
{
    return static_cast<oui::Container&>(bar.bar().child_at(0));
}

oui::NumberField& width_field(paint::ToolOptionsBar& bar)
{
    return static_cast<oui::NumberField&>(width_group(bar).child_at(1));
}

oui::Slider& width_slider(paint::ToolOptionsBar& bar)
{
    return static_cast<oui::Slider&>(width_group(bar).child_at(2));
}

} // namespace

TEST(PaintPanelsTests, TheToolPanelHasEveryToolOfPaintInOuiPaintsOrder)
{
    paint::Icons icons;
    const paint::ToolPanel panel{icons};
    ASSERT_EQ(panel.buttons().size(), paint::all_tools().size());
    for(std::size_t index{}; index < panel.buttons().size(); ++ index)
    {
        EXPECT_EQ(panel.buttons()[index].id, paint::tool_id(paint::all_tools()[index]));
    }
    EXPECT_EQ(panel.selected_tool(), paint::Tool::rectangle_select) << "the first, until a program says";
}

TEST(PaintPanelsTests, AClickOnTheToolPanelChoosesTheTool)
{
    paint::Icons icons;
    paint::ToolPanel panel{icons};
    panel.arrange({3, 5, 4, 8});
    std::vector<std::string> picked;
    panel.on_picked([&picked](const std::string& id) { picked.push_back(id); });

    EXPECT_TRUE(panel.handle_event(press_button(index_of(panel, "brush"), 3, 5)));
    EXPECT_EQ(panel.selected_tool(), paint::Tool::brush);
    EXPECT_TRUE(panel.handle_event(press_button(index_of(panel, "bucket"), 3, 5)));
    EXPECT_EQ(panel.selected_tool(), paint::Tool::fill);
    EXPECT_EQ(picked, (std::vector<std::string>{"brush", "bucket"}));

    EXPECT_FALSE(panel.handle_event(press(3, 5, true))) << "the right button chooses nothing";
    EXPECT_FALSE(panel.handle_event(press(3, 5 + 8))) << "past the buttons";
    EXPECT_EQ(panel.selected_tool(), paint::Tool::fill);
    EXPECT_EQ(panel.tooltip_at({3, 5 + static_cast<double>(index_of(panel, "brush") / 2)}), "Paintbrush (B)");
}

TEST(PaintPanelsTests, PaintNetsKeyChoosesTheToolAsAClickDoes)
{
    paint::Icons icons;
    paint::ToolPanel panel{icons};
    oui::CommandRegistry commands;
    paint::add_tool_commands(commands, panel);
    std::vector<std::string> picked;
    panel.on_picked([&picked](const std::string& id) { picked.push_back(id); });
    const auto key{[&commands, &panel](char32_t character)
                   {
                       EXPECT_TRUE(commands.dispatch(oui::TextEvent{character}));
                       return panel.selected_tool();
                   }};

    EXPECT_EQ(key(U'b'), paint::Tool::brush);
    EXPECT_EQ(key(U'p'), paint::Tool::pencil);
    EXPECT_EQ(key(U'e'), paint::Tool::eraser);
    EXPECT_EQ(key(U'f'), paint::Tool::fill);
    EXPECT_EQ(key(U'g'), paint::Tool::gradient);
    EXPECT_EQ(key(U'k'), paint::Tool::color_picker);
    EXPECT_EQ(picked, (std::vector<std::string>{"brush", "pencil", "eraser", "bucket", "gradient", "color_picker"}));
    // Pressed again, a key goes to the next tool with it, and round.
    EXPECT_EQ(key(U'o'), paint::Tool::line);
    EXPECT_EQ(key(U'o'), paint::Tool::shapes);
    EXPECT_EQ(key(U'o'), paint::Tool::arc);
    EXPECT_EQ(key(U'o'), paint::Tool::line);
    EXPECT_EQ(key(U's'), paint::Tool::rectangle_select);
    EXPECT_EQ(key(U's'), paint::Tool::lasso_select);
    EXPECT_EQ(key(U's'), paint::Tool::ellipse_select);
    EXPECT_EQ(key(U's'), paint::Tool::magic_wand);
    EXPECT_EQ(key(U's'), paint::Tool::rectangle_select);
    EXPECT_EQ(key(U'm'), paint::Tool::move_pixels);
    EXPECT_EQ(key(U'm'), paint::Tool::move_selection);

    EXPECT_FALSE(commands.dispatch(oui::TextEvent{U'z'})) << "no tool has it";
    EXPECT_FALSE(panel.press_key(U'z'));
    EXPECT_EQ(panel.selected_tool(), paint::Tool::move_selection);
}

TEST(PaintPanelsTests, AProgramsOwnToolsSitAmongPaintsAndChooseNoneOfThem)
{
    paint::Icons icons;
    auto buttons{paint::paint_tool_buttons()};
    buttons.push_back({"zoom", "Zoom", "Zoom", U'z'});
    icons.add("Zoom", R"(<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16"><rect width="16" height="16")"
                      R"( fill="#e0e0e0"/></svg>)");
    paint::ToolPanel panel{buttons, icons};
    EXPECT_TRUE(panel.press_key(U'z'));
    EXPECT_EQ(panel.selected(), "zoom");
    EXPECT_EQ(panel.selected_tool(), std::nullopt);
    EXPECT_NE(icons.picture("Zoom", 16, 16), nullptr);
}

TEST(PaintPanelsTests, EveryToolsIconIsBuiltIn)
{
    paint::Icons icons;
    for(const auto& button : paint::paint_tool_buttons())
    {
        for(const auto dark : {false, true})
        {
            icons.set_dark_interface(dark);
            const auto* const picture{icons.picture(button.icon, 32, 16)};
            ASSERT_NE(picture, nullptr) << button.icon;
            EXPECT_EQ(picture->width, 32);
            EXPECT_EQ(picture->height, 16);
            auto drawn{false};
            for(const auto pixel : picture->pixels)
            {
                drawn = drawn || pixel >> 24u != 0;
            }
            EXPECT_TRUE(drawn) << button.icon;
        }
    }
    EXPECT_EQ(icons.picture("Nothing", 16, 16), nullptr);
}

TEST(PaintPanelsTests, TheOptionsBarChangesTheWidthItShowsAndGives)
{
    paint::ToolOptions start;
    start.brush.width = 12;
    start.primary = {1, 0, 0, 1};
    paint::ToolOptionsBar bar{start};
    EXPECT_EQ(width_field(bar).value(), 12);
    EXPECT_EQ(width_slider(bar).value(), 12);
    std::vector<double> told;
    bar.on_changed([&told](const paint::ToolOptions& options) { told.push_back(options.brush.width); });

    // Typed in the field, as a person does.
    width_field(bar).text_field().set_text("13");
    EXPECT_FALSE(width_field(bar).handle_event(oui::KeyEvent{oui::Key::enter}));
    EXPECT_EQ(width_field(bar).value(), 13);
    EXPECT_EQ(bar.options().brush.width, 13);
    EXPECT_EQ(width_slider(bar).value(), 13);
    // The slider moves the field.
    width_slider(bar).set_value(40);
    EXPECT_EQ(width_field(bar).value(), 40);
    EXPECT_EQ(bar.options().brush.width, 40);
    // ] and [, as a program binds them.
    bar.change_width(-5);
    EXPECT_EQ(width_field(bar).value(), 35);
    EXPECT_EQ(bar.options().brush.width, 35);
    EXPECT_EQ(told, (std::vector<double>{13, 40, 35})) << "each change told once";
    EXPECT_TRUE(same(bar.options().primary, {1, 0, 0, 1})) << "the colours kept as given";

    // Options a program gives are shown, not told.
    auto other{bar.options()};
    other.brush.width = 3;
    bar.set_options(other);
    EXPECT_EQ(width_field(bar).value(), 3);
    EXPECT_EQ(told.size(), 3u);
}

TEST(PaintPanelsTests, TheOptionsBarShowsWhatTheToolUsesAndGivesTheRestToo)
{
    paint::ToolOptionsBar bar;
    auto& groups{bar.bar()};
    const auto shown{[&groups]
                     {
                         std::vector<bool> visible;
                         for(std::size_t index{}; index < groups.child_count(); ++ index)
                         {
                             visible.push_back(groups.child_at(index).is_visible());
                         }
                         return visible;
                     }};
    // Width, hardness, antialiasing, fill, gradient, line, shape.
    EXPECT_EQ(shown(), (std::vector<bool>{true, true, true, false, false, false, false}));
    bar.show_for(paint::Tool::fill);
    EXPECT_EQ(shown(), (std::vector<bool>{false, false, false, true, false, false, false}));
    bar.show_for(paint::Tool::line);
    EXPECT_EQ(shown(), (std::vector<bool>{true, false, true, false, false, true, false}));
    bar.show_for("recolor");
    EXPECT_EQ(shown(), (std::vector<bool>{true, true, false, true, false, false, false}));

    paint::ToolOptions told;
    bar.on_changed([&told](const paint::ToolOptions& options) { told = options; });
    auto& line{static_cast<oui::Container&>(groups.child_at(5))};
    EXPECT_TRUE(static_cast<oui::CheckButton&>(line.child_at(1)).handle_event(oui::KeyEvent{oui::Key::space}));
    EXPECT_TRUE(told.line.arrow_at_end);
    EXPECT_FALSE(told.line.arrow_at_start);
    auto& fill{static_cast<oui::Container&>(groups.child_at(3))};
    static_cast<oui::NumberField&>(fill.child_at(3)).set_value(20);
    EXPECT_DOUBLE_EQ(told.fill.tolerance, 0.2);
    EXPECT_TRUE(told.line.arrow_at_end) << "what was changed before stays";
}

TEST(PaintPanelsTests, TheColorPanelsPrimaryAndSecondarySwapWithX)
{
    paint::ColorPanel panel;
    oui::CommandRegistry commands;
    paint::add_color_commands(commands, panel);
    auto changes{0};
    panel.on_changed([&changes] { ++ changes; });
    panel.set_primary({1, 0, 0, 1});
    EXPECT_TRUE(commands.dispatch(oui::TextEvent{U'x'}));
    EXPECT_TRUE(same(panel.primary(), {1, 1, 1, 1}));
    EXPECT_TRUE(same(panel.secondary(), {1, 0, 0, 1}));
    EXPECT_TRUE(commands.dispatch(oui::TextEvent{U'x'}));
    EXPECT_TRUE(same(panel.primary(), {1, 0, 0, 1}));
    EXPECT_TRUE(same(panel.secondary(), {1, 1, 1, 1}));
    EXPECT_EQ(changes, 3);
}

TEST(PaintPanelsTests, APaletteClickSetsTheColourBeingEditedAndCChoosesWhich)
{
    paint::ColorPanel panel;
    oui::CommandRegistry commands;
    paint::add_color_commands(commands, panel);
    panel.arrange({0, 0, paint::ColorPanel::width, 20});
    std::vector<bool> picked;
    panel.on_picked([&picked](bool secondary) { picked.push_back(secondary); });
    std::vector<bool> edited;
    panel.on_edit([&edited](bool primary) { edited.push_back(primary); });

    // The palette starts under the squares: black, then greys, two to a row.
    EXPECT_TRUE(panel.handle_event(press(2, 7)));
    EXPECT_TRUE(same(panel.primary(), *panel.palette_at({2, 7})));
    EXPECT_TRUE(panel.handle_event(press(0, 7, true)));
    EXPECT_TRUE(same(panel.secondary(), *panel.palette_at({0, 7})));
    EXPECT_TRUE(commands.dispatch(oui::TextEvent{U'c'}));
    EXPECT_TRUE(panel.editing_secondary());
    EXPECT_TRUE(panel.handle_event(press(0, 3)));
    EXPECT_TRUE(same(panel.secondary(), {0, 0, 0, 1}));
    EXPECT_EQ(picked, (std::vector<bool>{false, true, true}));

    EXPECT_TRUE(panel.handle_event(press(3, 2)));
    EXPECT_TRUE(panel.handle_event(press(0, 0)));
    EXPECT_EQ(edited, (std::vector<bool>{false, true})) << "the secondary's square, then the primary's";
}

TEST(PaintPanelsTests, ThePanelsTextsComeInSpanish)
{
    ASSERT_EQ(paint::panel_translations().size(), 1u);
    EXPECT_EQ(paint::panel_translations().front().language(), "es");
    oui::install_language("es", paint::panel_translations());
    const auto buttons{paint::paint_tool_buttons()};
    const auto palettes{oui::translate("Palettes", "&Palettes")};
    oui::remove_translations();
    EXPECT_EQ(palettes, "&Paletas");
    EXPECT_EQ(buttons[8].id, "brush");
    EXPECT_EQ(buttons[8].title, "Pincel");
}

TEST(PaintPanelsTests, ScaledThePanelsAreBiggerAndTheirClicksLandWhereTheyAreDrawn)
{
    paint::Icons icons;
    paint::ToolPanel tools{icons};
    const oui::Constraints loose{100, 100};
    const auto plain{tools.measure(loose)};
    tools.set_scale(1.5);
    const auto scaled{tools.measure(loose)};
    EXPECT_DOUBLE_EQ(scaled.width, plain.width * 1.5);
    EXPECT_DOUBLE_EQ(scaled.height, plain.height * 1.5);
    EXPECT_EQ(tools.button_at({4.5, 0.5}), std::optional<std::size_t>{1}) << "the second button, three cells wide";
    EXPECT_EQ(tools.button_at({0.5, 1.6}), std::optional<std::size_t>{2}) << "the second row, a row and a half down";
    tools.set_scale(0.5);
    EXPECT_DOUBLE_EQ(tools.scale(), 1.0) << "never smaller than its cells";

    paint::ColorPanel colors;
    const auto colors_plain{colors.measure(loose)};
    colors.set_scale(2.0);
    EXPECT_DOUBLE_EQ(colors.measure(loose).width, colors_plain.width * 2.0);
    colors.arrange({0, 0, 8, 40});
    // The palette starts under the squares, six rows down at twice the size; its first two colours side by side.
    EXPECT_FALSE(colors.palette_at({1, 5}));
    ASSERT_TRUE(colors.palette_at({1, 6.5}));
    ASSERT_TRUE(colors.palette_at({5, 6.5}));
    EXPECT_FALSE(same(*colors.palette_at({1, 6.5}), *colors.palette_at({5, 6.5})));
    EXPECT_TRUE(colors.handle_event(press(5, 7)));
    EXPECT_TRUE(same(colors.primary(), *colors.palette_at({5, 6.5})));
}

TEST(PaintPanelsTests, TheLibrarySavesListsRenamesAndRemovesAndKeepsDefault)
{
    const auto folder{std::filesystem::temp_directory_path() / "olagarro_palette_library_test"};
    std::filesystem::remove_all(folder);
    paint::PaletteLibrary library{folder};
    EXPECT_EQ(library.folder(), folder);
    EXPECT_EQ(library.names(), (std::vector<std::string>{"Default"})) << "there before any is saved";
    EXPECT_EQ(library.load("Default"), paint::Palette::standard());

    const paint::Palette warm{"Warm", {{1, 0, 0, 1}, {1, 1, 0, 1}}};
    const paint::Palette cold{"Cold", {{0, 0, 1, 1}}};
    EXPECT_TRUE(library.save(warm));
    EXPECT_TRUE(library.save(cold));
    EXPECT_TRUE(library.save({"Blue/Green", {{0, 1, 1, 1}}})) << "a name no file could have";
    EXPECT_EQ(library.names(), (std::vector<std::string>{"Default", "Blue/Green", "Cold", "Warm"}));
    EXPECT_EQ(library.load("Warm"), warm);
    EXPECT_EQ(library.load("Blue/Green")->name, "Blue/Green");
    EXPECT_FALSE(library.load("Nothing"));

    // A name's file replaced.
    const paint::Palette warmer{"Warm", {{1, 0, 0, 1}}};
    EXPECT_TRUE(library.save(warmer));
    EXPECT_EQ(library.load("Warm"), warmer);
    EXPECT_EQ(library.names().size(), 4u);

    EXPECT_FALSE(library.save({"Default", {}}));
    EXPECT_FALSE(library.rename("Default", "Mine"));
    EXPECT_FALSE(library.rename("Warm", "Default"));
    EXPECT_FALSE(library.rename("Warm", "Cold")) << "one there already";
    EXPECT_FALSE(library.remove("Default"));
    EXPECT_EQ(library.load("Default"), paint::Palette::standard());

    EXPECT_TRUE(library.rename("Warm", "Hot"));
    EXPECT_EQ(library.load("Hot"), (paint::Palette{"Hot", warmer.colors}));
    EXPECT_FALSE(library.load("Warm"));
    EXPECT_TRUE(library.remove("Cold"));
    EXPECT_FALSE(library.remove("Cold"));
    EXPECT_EQ(library.names(), (std::vector<std::string>{"Default", "Blue/Green", "Hot"}));
    // Kept in the folder, as another program finds them.
    EXPECT_EQ(paint::PaletteLibrary{folder}.names(), library.names());
    std::filesystem::remove_all(folder);

    const auto shared{paint::PaletteLibrary::default_folder()};
    EXPECT_EQ(shared.filename(), "palettes");
    EXPECT_EQ(shared.parent_path().filename(), "olagarro");
}

TEST(PaintPanelsTests, ADoubleClickAsksForAColourPlusAddsThePrimaryAndADragRemovesOrMovesOne)
{
    const auto folder{std::filesystem::temp_directory_path() / "olagarro_palette_panel_test"};
    std::filesystem::remove_all(folder);
    paint::PaletteLibrary library{folder};
    paint::ColorPanel panel;
    panel.arrange({0, 0, paint::ColorPanel::width, 30});
    std::vector<paint::Color> asked;
    paint::PaletteAsks asks;
    asks.color = [&asked](const paint::Color& start)
    {
        asked.push_back(start);
        return std::optional<paint::Color>{paint::Color{1, 0, 0, 1}};
    };
    panel.set_asks(asks);
    const auto plain{panel.measure({100, 100})};
    panel.set_library(&library);
    EXPECT_EQ(panel.measure({100, 100}).height, plain.height + 2.0) << "the name's row, and the \"+\"'s";
    auto changes{0};
    panel.on_palette_changed([&changes] { ++ changes; });
    const auto standard{paint::Palette::standard()};
    EXPECT_EQ(panel.palette(), standard);
    EXPECT_FALSE(panel.palette_modified());

    // Under the squares the name, then the colours two to a row: the third colour at the left of the second row.
    EXPECT_TRUE(same(*panel.palette_at({0, 5}), standard.colors[2]));
    EXPECT_TRUE(panel.handle_event(press(0, 5)));
    EXPECT_TRUE(asked.empty()) << "a single click picks";
    auto twice{press(0, 5)};
    twice.clicks = 2;
    EXPECT_TRUE(panel.handle_event(twice));
    ASSERT_EQ(asked.size(), 1u);
    EXPECT_TRUE(same(asked.front(), standard.colors[2]));
    EXPECT_TRUE(same(panel.palette().colors[2], {1, 0, 0, 1}));
    EXPECT_TRUE(panel.palette_modified());
    EXPECT_EQ(changes, 1);

    // The cell after the last colour adds the primary.
    panel.set_primary({0, 0, 1, 1});
    const auto plus_row{4.0 + static_cast<double>(standard.colors.size() / 2)};
    EXPECT_FALSE(panel.palette_at({0, plus_row}));
    EXPECT_TRUE(panel.handle_event(press(0, plus_row)));
    ASSERT_EQ(panel.palette().colors.size(), standard.colors.size() + 1);
    EXPECT_TRUE(same(panel.palette().colors.back(), {0, 0, 1, 1}));
    EXPECT_EQ(changes, 2);

    // The second colour dragged away from the panel goes.
    oui::MouseEvent moved{10, 4, false};
    moved.dragging = true;
    oui::MouseEvent released{10, 4, false};
    released.released = true;
    EXPECT_TRUE(panel.handle_event(press(2, 4)));
    EXPECT_TRUE(panel.handle_event(moved));
    EXPECT_TRUE(panel.handle_event(released));
    ASSERT_EQ(panel.palette().colors.size(), standard.colors.size());
    EXPECT_TRUE(same(panel.palette().colors[1], {1, 0, 0, 1}));
    EXPECT_EQ(changes, 3);

    // The first dragged onto the fourth goes there.
    released.x = 2;
    released.y = 5;
    EXPECT_TRUE(panel.handle_event(press(0, 4)));
    EXPECT_TRUE(panel.handle_event(released));
    EXPECT_TRUE(same(panel.palette().colors[3], standard.colors[0]));
    EXPECT_TRUE(same(panel.palette().colors[0], {1, 0, 0, 1}));
    EXPECT_EQ(changes, 4);
    // Let go where it was picked, it stays.
    released.x = 0;
    released.y = 4;
    EXPECT_TRUE(panel.handle_event(press(0, 4)));
    EXPECT_TRUE(panel.handle_event(released));
    EXPECT_EQ(changes, 4);

    // Without a library the palette is only picked from.
    panel.set_library(nullptr);
    EXPECT_TRUE(panel.handle_event(twice));
    EXPECT_EQ(asked.size(), 1u);
    std::filesystem::remove_all(folder);
}

TEST(PaintPanelsTests, APaletteSavedAsANameComesBackFromPalettes)
{
    const auto folder{std::filesystem::temp_directory_path() / "olagarro_palette_menu_test"};
    std::filesystem::remove_all(folder);
    paint::PaletteLibrary library{folder};
    paint::ColorPanel panel;
    panel.arrange({0, 0, paint::ColorPanel::width, 30});
    std::vector<oui::MenuItem> menu;
    std::vector<std::string> questions;
    std::string name{"Mine"};
    paint::PaletteAsks asks;
    asks.open_menu = [&menu](std::vector<oui::MenuItem> items, oui::Point) { menu = std::move(items); };
    asks.name = [&name](const std::string&, const std::string&) { return std::optional<std::string>{name}; };
    asks.confirm = [&questions](const std::string& question)
    {
        questions.push_back(question);
        return true;
    };
    panel.set_asks(asks);
    panel.set_library(&library);
    const auto entry{[&menu](const std::vector<oui::MenuItem>& items, const std::string& title) -> const oui::MenuItem&
                     {
                         for(const auto& item : items)
                         {
                             if(oui::strip_mnemonic(item.title()) == title)
                             {
                                 return item;
                             }
                         }
                         ADD_FAILURE() << "no entry " << title;
                         return menu.front();
                     }};
    const auto standard{paint::Palette::standard()};
    const auto plus_row{4.0 + static_cast<double>(standard.colors.size() / 2)};

    // A right click away from the colours opens the menu; on one it picks the other colour.
    EXPECT_TRUE(panel.handle_event(press(0, 4, true)));
    EXPECT_TRUE(menu.empty());
    EXPECT_TRUE(panel.handle_event(press(2, plus_row, true)));
    ASSERT_FALSE(menu.empty());
    EXPECT_FALSE(entry(menu, "Save").enabled()) << "not over Default";
    EXPECT_FALSE(entry(menu, "Rename…").enabled());
    EXPECT_FALSE(entry(menu, "Remove").enabled());
    panel.set_primary({0, 1, 0, 1});
    entry(menu, "Add the Primary Colour").run();
    EXPECT_TRUE(panel.palette_modified());

    entry(menu, "Save As…").run();
    EXPECT_TRUE(questions.empty());
    EXPECT_EQ(panel.palette().name, "Mine");
    EXPECT_FALSE(panel.palette_modified());
    EXPECT_EQ(library.names(), (std::vector<std::string>{"Default", "Mine"}));
    EXPECT_TRUE(same(library.load("Mine")->colors.back(), {0, 1, 0, 1}));

    // Back to Default from the list, then Mine again.
    EXPECT_TRUE(panel.handle_event(press(2, plus_row + 1, true)));
    const auto palettes{entry(menu, "Palettes").items()};
    ASSERT_EQ(palettes.size(), 2u);
    EXPECT_EQ(entry(palettes, "Mine").checked(), std::optional<bool>{true});
    EXPECT_EQ(entry(palettes, "Default").checked(), std::optional<bool>{false});
    entry(palettes, "Default").run();
    EXPECT_EQ(panel.palette(), standard);
    entry(entry(menu, "Palettes").items(), "Mine").run();
    EXPECT_EQ(panel.palette(), *library.load("Mine"));
    EXPECT_FALSE(panel.palette_modified());

    // Saved as a name there already, it asks first; Rename and Remove act on the library's.
    EXPECT_TRUE(panel.handle_event(press(2, plus_row + 1, true)));
    EXPECT_TRUE(entry(menu, "Save").enabled());
    entry(menu, "Save As…").run();
    EXPECT_EQ(questions.size(), 1u);
    name = "Ours";
    entry(menu, "Rename…").run();
    EXPECT_EQ(panel.palette().name, "Ours");
    EXPECT_EQ(library.names(), (std::vector<std::string>{"Default", "Ours"}));
    entry(menu, "Remove").run();
    EXPECT_EQ(questions.size(), 2u);
    EXPECT_EQ(library.names(), (std::vector<std::string>{"Default"}));
    EXPECT_TRUE(panel.palette_modified()) << "its colours kept, no longer saved";
    std::filesystem::remove_all(folder);
}

TEST(PaintPanelsTests, MoreColoursToARowMakeThePaletteShorterEachColourSmaller)
{
    paint::ColorPanel panel;
    const oui::Constraints loose{100, 100};
    const auto two{panel.measure(loose)};
    panel.set_palette_columns(4);
    const auto four{panel.measure(loose)};
    EXPECT_DOUBLE_EQ(four.width, two.width);
    // The squares take three rows either way; the palette about half as many rows, each half as tall, a last row half
    // full counting whole.
    EXPECT_LE(four.height - 3.0, (two.height - 3.0) / 4.0 + 0.5);
    EXPECT_LT(four.height, two.height);
    panel.arrange({0, 0, paint::ColorPanel::width, 20});
    // Four colours along the first row, each a cell wide and half a row tall.
    ASSERT_TRUE(panel.palette_at({0.5, 3.2}));
    ASSERT_TRUE(panel.palette_at({3.5, 3.2}));
    ASSERT_TRUE(panel.palette_at({0.5, 3.7}));
    EXPECT_FALSE(same(*panel.palette_at({0.5, 3.2}), *panel.palette_at({3.5, 3.2})));
    EXPECT_FALSE(same(*panel.palette_at({0.5, 3.2}), *panel.palette_at({0.5, 3.7}))) << "the second row";
    EXPECT_TRUE(panel.handle_event(press(3, 3)));
    EXPECT_TRUE(same(panel.primary(), *panel.palette_at({3.5, 3.2})));
}
