// The size of the system's pointer, apart from the window's backend so that the system's headers stay out of it.
#include "system_pointer.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <cstdlib>
#endif

namespace olagarro::oui
{

int system_pointer_size()
{
#ifdef _WIN32
    return GetSystemMetrics(SM_CXCURSOR);
#else
    // What X's cursors and Wayland's compositors read, as the desktop's settings set it.
    if(const auto* const size{std::getenv("XCURSOR_SIZE")})
    {
        const auto value{std::atoi(size)};
        if(value > 0)
        {
            return value;
        }
    }
    return 24;
#endif
}

} // namespace olagarro::oui
