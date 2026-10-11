#pragma once

#include <oui/event.h>

#include <vector>

// What OUI Code does for its own tests, chosen on the command line.

// --key-test: feeds what the real terminal sends for each key into a document and prints every event as it comes,
// then the text it made, to see how a terminal's dead keys and accents arrive. Needs an interactive console.
int run_key_test();
// --input-test: what is typed, in a new document, to print what it made: accents as dead keys give them.
[[nodiscard]] std::vector<olagarro::oui::Event> input_test_events();
// --stress-test: the dialogs opened and closed, typing, selecting with the mouse, undoing, and quitting.
[[nodiscard]] std::vector<olagarro::oui::Event> stress_test_events();
