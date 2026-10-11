#include "ui_preview.h"

#include <oui/application.h>
#include <oui/button.h>
#include <oui/combo_box.h>
#include <oui/event.h>
#include <oui/label.h>
#include <oui/scripted_backend.h>
#include <oui/window.h>

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace olagarro;

namespace
{

// The label under `widget` that says `text`, if there is one.
[[nodiscard]] const oui::Label* label_saying(const oui::Widget& widget, std::string_view text)
{
    if(const auto* const label{dynamic_cast<const oui::Label*>(&widget)}; label != nullptr)
    {
        return label->text() == text ? label : nullptr;
    }
    if(const auto* const container{dynamic_cast<const oui::Container*>(&widget)})
    {
        for(std::size_t index{}; index < container->child_count(); ++ index)
        {
            if(const auto* const found{label_saying(container->child_at(index), text)})
            {
                return found;
            }
        }
    }
    return nullptr;
}

// The combo box under `widget`, if there is one.
[[nodiscard]] oui::ComboBox* find_combo_box(oui::Widget& widget)
{
    if(auto* const box{dynamic_cast<oui::ComboBox*>(&widget)})
    {
        return box;
    }
    if(auto* const container{dynamic_cast<oui::Container*>(&widget)})
    {
        for(std::size_t index{}; index < container->child_count(); ++ index)
        {
            if(auto* const found{find_combo_box(container->child_at(index))})
            {
                return found;
            }
        }
    }
    return nullptr;
}

// The button under `widget` that says `text`, if there is one.
[[nodiscard]] oui::Button* find_button(oui::Widget& widget, std::string_view text)
{
    if(auto* const button{dynamic_cast<oui::Button*>(&widget)}; button != nullptr)
    {
        return button->text() == text ? button : nullptr;
    }
    if(auto* const container{dynamic_cast<oui::Container*>(&widget)})
    {
        for(std::size_t index{}; index < container->child_count(); ++ index)
        {
            if(auto* const found{find_button(container->child_at(index), text)})
            {
                return found;
            }
        }
    }
    return nullptr;
}

} // namespace

TEST(OuiCodeUiPreviewTests, ShowsWhatADescriptionBuildsAndKeepsItWhileTheTextIsWrong)
{
    oui::Application application{oui::scripted_backend()};
    UiPreviewView view{oui::text_edit()};
    view.show("Column\n  Label \"Hello\"\n  Slot id=results\n");
    auto* const shown{view.shown()};
    ASSERT_NE(shown, nullptr);
    EXPECT_NE(label_saying(*shown, "Hello"), nullptr);
    EXPECT_NE(label_saying(*shown, "[results]"), nullptr) << "a slot shows where what the code puts goes";
    EXPECT_FALSE(view.error_line());

    view.show("Column\n  Slider\n");
    EXPECT_EQ(view.shown(), shown) << "what built last stays while the text is wrong";
    EXPECT_EQ(view.error_line(), std::optional<std::size_t>{2});
    EXPECT_EQ(view.error_text(), "Line 2, column 3: There is no widget called Slider.");

    view.show("Window \"Ask\"\n  Label \"Name\"\n");
    EXPECT_FALSE(view.error_line());
    ASSERT_NE(dynamic_cast<oui::Window*>(view.shown()), nullptr) << "a window shows as one, frame and all";
    EXPECT_NE(label_saying(*view.shown(), "Name"), nullptr);
}

TEST(OuiCodeUiPreviewTests, ItsGenerateButtonAsksForAScript)
{
    oui::Application application{oui::scripted_backend()};
    UiPreviewView view{oui::text_edit()};
    auto asked{0};
    view.on_generate([&asked] { ++ asked; });
    auto* const button{find_button(view, "Generate")};
    ASSERT_NE(button, nullptr);
    button->click();
    EXPECT_EQ(asked, 1);
}

// The second combo box under `widget`, the first being the language's.
[[nodiscard]] oui::ComboBox* script_box(oui::Widget& widget)
{
    std::vector<oui::ComboBox*> boxes;
    const auto collect{[&boxes](auto& self, oui::Widget& each) -> void
                       {
                           if(auto* const box{dynamic_cast<oui::ComboBox*>(&each)})
                           {
                               boxes.push_back(box);
                           }
                           if(auto* const container{dynamic_cast<oui::Container*>(&each)})
                           {
                               for(std::size_t index{}; index < container->child_count(); ++ index)
                               {
                                   self(self, container->child_at(index));
                               }
                           }
                       }};
    collect(collect, widget);
    return boxes.size() > 1 ? boxes[1] : nullptr;
}

// Picks the option at `index` in the box with the pointer, as a person would: opens it, then clicks the option.
void pick(oui::ComboBox& box, std::size_t index)
{
    static_cast<void>(box.handle_event(oui::MouseEvent{box.bounds().x, box.bounds().y, true}));
    static_cast<void>(
        box.handle_event(oui::MouseEvent{box.bounds().x, box.bounds().y + static_cast<int>(index), true}));
}

TEST(OuiCodeUiPreviewTests, TheScriptAfterGenerateIsChosenBesideItOrNone)
{
    oui::Application application{oui::scripted_backend()};
    UiPreviewView view{oui::text_edit()};
    auto wanted{0};
    auto dropped{0};
    view.on_script_wanted([&wanted] { ++ wanted; });
    view.on_script_dropped([&dropped] { ++ dropped; });
    auto created{0};
    view.on_script_created([&created] { ++ created; });
    auto* const box{script_box(view)};
    ASSERT_NE(box, nullptr);
    EXPECT_EQ(box->options(), (std::vector<std::string>{"No Script", "Choose...", "New..."}));
    EXPECT_EQ(box->selected(), 0u);

    pick(*box, 1);
    EXPECT_EQ(wanted, 1);
    // What was chosen, or what there was when nothing was: set by the code, which is not a choice of the person's.
    view.set_script("build.js");
    EXPECT_EQ(box->options(), (std::vector<std::string>{"No Script", "build.js", "Choose...", "New..."}));
    EXPECT_EQ(box->selected(), 1u);
    EXPECT_EQ(wanted, 1);
    EXPECT_EQ(dropped, 0);

    pick(*box, 0);
    EXPECT_EQ(dropped, 1);
    view.set_script(std::nullopt);
    EXPECT_EQ(box->options(), (std::vector<std::string>{"No Script", "Choose...", "New..."}));
    EXPECT_EQ(box->selected(), 0u);
    EXPECT_EQ(wanted, 1);

    pick(*box, 2);
    EXPECT_EQ(created, 1);
    EXPECT_EQ(wanted, 1);
    EXPECT_EQ(dropped, 1);
}

TEST(OuiCodeUiPreviewTests, ItsLanguageIsChosenBesideGenerateAndSaidWhenItChanges)
{
    oui::Application application{oui::scripted_backend()};
    UiPreviewView view{oui::text_edit()};
    std::vector<SkeletonLanguage> chosen;
    view.on_language_chosen([&chosen](SkeletonLanguage language) { chosen.push_back(language); });
    EXPECT_EQ(view.language(), SkeletonLanguage::javascript);
    // As remembered from before: set by the code, which is not a choice of the person's.
    view.set_language(SkeletonLanguage::cpp);
    EXPECT_EQ(view.language(), SkeletonLanguage::cpp);
    EXPECT_TRUE(chosen.empty());
    auto* const box{find_combo_box(view)};
    ASSERT_NE(box, nullptr);
    EXPECT_EQ(box->options(), (std::vector<std::string>{"JavaScript", "C++"}));
    box->set_selected(0);
    EXPECT_EQ(view.language(), SkeletonLanguage::javascript);
}
