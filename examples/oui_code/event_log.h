#pragma once

#include <oui/event.h>

#include <cstdio>

// Writes `event` as a line of `output`, for a debug build's oui_code_events.log: what came in, to replay a bug.
void record_event(std::FILE* output, const olagarro::oui::Event& event);
