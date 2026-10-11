#include "event_log.h"

using namespace olagarro;

void record_event(std::FILE* const output, const oui::Event& event)
{
    if(output == nullptr)
    {
        return;
    }
    if(const auto* const key{std::get_if<oui::KeyEvent>(&event)})
    {
        std::fprintf(output, "key %d %d %d %d", static_cast<int>(key->key), key->control, key->alt, key->shift);
    }
    else if(const auto* const text{std::get_if<oui::TextEvent>(&event)})
    {
        std::fprintf(output, "text %u %d %d %d", static_cast<unsigned>(text->codepoint), text->control, text->alt,
                     text->shift);
    }
    else if(const auto* const resize{std::get_if<oui::ResizeEvent>(&event)})
    {
        std::fprintf(output, "resize %d %d", resize->width, resize->height);
    }
    else if(const auto* const mouse{std::get_if<oui::MouseEvent>(&event)})
    {
        std::fprintf(output, "mouse %d %d %d %d %d %d", mouse->x, mouse->y, mouse->pressed, mouse->wheel_delta,
                     mouse->dragging, mouse->released);
    }
    std::fputc('\n', output);
    std::fflush(output);
}
