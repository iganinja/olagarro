#include "self_tests.h"

#include "ui_descriptions.h"

#include <oui/oui.h>

#include <cstdio>
#include <memory>
#include <string>

using namespace olagarro;

int run_key_test()
{
    oui::TerminalOptions terminal_options;
    terminal_options.use_alternate_screen = false;
    auto backend{oui::terminal_backend(terminal_options)};
    auto* const terminal_backend{backend.get()};
    terminal_backend->set_diagnose(true);
    oui::Application application{std::move(backend)};
    auto ui{build_oui_code_ui(OuiCodeUi::key_test)};
    auto& editor_reference{ui.get<oui::TextEdit>("editor")};
    application.set_window(ui.take_window());
    application.focus(editor_reference);
    application.render();
    std::fputs("KEY_TEST: type accented characters. Each event prints as [type:codepoint]. Enter finishes.\n", stdout);
    std::fflush(stdout);
    bool finished{false};
    auto log_event{[&](const oui::Event& event)
                   {
                       if(const auto* const text{std::get_if<oui::TextEvent>(&event)})
                       {
                           std::printf("  [text:U+%X ctrl=%d alt=%d shift=%d]\n",
                                       static_cast<unsigned>(text->codepoint), text->control, text->alt, text->shift);
                       }
                       else if(const auto* const key{std::get_if<oui::KeyEvent>(&event)})
                       {
                           std::printf("  [key:%d ctrl=%d alt=%d shift=%d]\n", static_cast<int>(key->key), key->control,
                                       key->alt, key->shift);
                       }
                       std::fflush(stdout);
                   }};
    while(!finished)
    {
        while(const auto event{terminal_backend->poll_event()})
        {
            log_event(*event);
            if(const auto* const key{std::get_if<oui::KeyEvent>(&*event)})
            {
                if(key->key == oui::Key::enter)
                {
                    finished = true;
                }
            }
            if(!finished)
            {
                application.dispatch_event(*event);
            }
        }
        if(!finished)
        {
            terminal_backend->wait_for_input(std::nullopt);
        }
    }
    std::fputs(("KEY_TEST_RESULT " + std::string{editor_reference.text()} + "\n").c_str(), stdout);
    std::fflush(stdout);
    return 0;
}

std::vector<oui::Event> input_test_events()
{
    return {oui::TextEvent{U'n', true}, oui::KeyEvent{oui::Key::end}, oui::TextEvent{U'a'}, oui::TextEvent{U'â'},
            oui::TextEvent{U'e'},       oui::TextEvent{U'ê'},         oui::TextEvent{U'i'}, oui::TextEvent{U'î'},
            oui::TextEvent{U'o'},       oui::TextEvent{U'ô'},         oui::TextEvent{U'u'}, oui::TextEvent{U'û'},
            oui::TextEvent{U'a'},       oui::TextEvent{U'á'},         oui::TextEvent{U'e'}, oui::TextEvent{U'é'},
            oui::TextEvent{U'i'},       oui::TextEvent{U'í'},         oui::TextEvent{U'o'}, oui::TextEvent{U'ó'},
            oui::TextEvent{U'u'},       oui::TextEvent{U'ú'},         oui::TextEvent{U'a'}, oui::TextEvent{U'à'},
            oui::TextEvent{U'e'},       oui::TextEvent{U'è'},         oui::TextEvent{U'i'}, oui::TextEvent{U'ì'},
            oui::TextEvent{U'o'},       oui::TextEvent{U'ò'},         oui::TextEvent{U'u'}, oui::TextEvent{U'ù'},
            oui::TextEvent{U'a'},       oui::TextEvent{U'ä'},         oui::TextEvent{U'e'}, oui::TextEvent{U'ë'},
            oui::TextEvent{U'i'},       oui::TextEvent{U'ï'},         oui::TextEvent{U'o'}, oui::TextEvent{U'ö'},
            oui::TextEvent{U'u'},       oui::TextEvent{U'ü'},         oui::TextEvent{U'a'}, oui::TextEvent{U'ã'},
            oui::TextEvent{U'e'},       oui::TextEvent{U'ẽ'},         oui::TextEvent{U'i'}, oui::TextEvent{U'ĩ'},
            oui::TextEvent{U'o'},       oui::TextEvent{U'õ'},         oui::TextEvent{U'u'}, oui::TextEvent{U'ũ'},
            oui::TextEvent{U'A'},       oui::TextEvent{U'Â'},         oui::TextEvent{U'E'}, oui::TextEvent{U'Ê'},
            oui::TextEvent{U'I'},       oui::TextEvent{U'Î'},         oui::TextEvent{U'O'}, oui::TextEvent{U'Ô'},
            oui::TextEvent{U'U'},       oui::TextEvent{U'Û'},         oui::TextEvent{U'^'}, oui::TextEvent{U' '},
            oui::TextEvent{U'ñ'},       oui::TextEvent{U'ç'},         oui::TextEvent{U'ü'}, oui::TextEvent{U'ö'},
            oui::TextEvent{U'ê'},       oui::TextEvent{U'ê'},         oui::TextEvent{U'ê'}, oui::TextEvent{U'ê'}};
}

std::vector<oui::Event> stress_test_events()
{
    return {oui::TextEvent{U'f', false, true},
            oui::KeyEvent{oui::Key::escape},
            oui::TextEvent{15, true},
            oui::KeyEvent{oui::Key::down},
            oui::KeyEvent{oui::Key::up},
            oui::KeyEvent{oui::Key::escape},
            oui::TextEvent{16, true},
            oui::KeyEvent{oui::Key::escape},
            oui::TextEvent{6, true},
            oui::KeyEvent{oui::Key::escape},
            oui::TextEvent{8, true},
            oui::KeyEvent{oui::Key::escape},
            oui::TextEvent{7, true},
            oui::KeyEvent{oui::Key::escape},
            oui::TextEvent{14, true},
            oui::MouseEvent{30, 2, true},
            oui::MouseEvent{30, 2, false, 0, false, true},
            oui::TextEvent{U'H'},
            oui::TextEvent{U'o'},
            oui::TextEvent{U'l'},
            oui::TextEvent{U'a'},
            oui::TextEvent{U' '},
            oui::TextEvent{U'ñ'},
            oui::KeyEvent{oui::Key::enter},
            oui::TextEvent{U'l'},
            oui::TextEvent{U'i'},
            oui::TextEvent{U'n'},
            oui::TextEvent{U'e'},
            oui::TextEvent{U'a'},
            oui::KeyEvent{oui::Key::enter},
            oui::TextEvent{U't'},
            oui::TextEvent{U'e'},
            oui::TextEvent{U's'},
            oui::TextEvent{U't'},
            oui::MouseEvent{30, 2, true},
            oui::MouseEvent{34, 2, false, 0, true},
            oui::MouseEvent{34, 2, false, 0, false, true},
            oui::MouseEvent{30, 2, false, -2},
            oui::TextEvent{26, true},
            oui::TextEvent{25, true},
            oui::TextEvent{23, true},
            oui::KeyEvent{oui::Key::tab},
            oui::KeyEvent{oui::Key::space},
            oui::TextEvent{13, true},
            oui::TextEvent{13, true},
            oui::TextEvent{2, true},
            oui::TextEvent{2, true},
            oui::TextEvent{17, true}};
}
