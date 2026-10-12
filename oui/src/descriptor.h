#pragma once

#include <utility>

#include <fcntl.h>
#include <unistd.h>

namespace olagarro::oui
{

// A file descriptor closed when it goes.
class Descriptor
{
  public:
    Descriptor() = default;
    explicit Descriptor(int fd) : m_fd{fd} {}
    ~Descriptor()
    {
        if(m_fd >= 0)
        {
            ::close(m_fd);
        }
    }
    Descriptor(Descriptor&& other) noexcept : m_fd{other.m_fd} { other.m_fd = -1; }
    Descriptor& operator=(Descriptor&& other) noexcept
    {
        std::swap(m_fd, other.m_fd);
        return *this;
    }
    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
    [[nodiscard]] int get() const { return m_fd; }

  private:
    int m_fd{-1};
};

// Opens a pipe whose ends close as another program starts, and do not wait when `nonblocking`; false when it cannot.
// Linux does it in one call, pipe2; macOS has no pipe2, and marks the ends once they are open.
[[nodiscard]] inline bool open_pipe(int (&ends)[2], bool nonblocking)
{
#ifdef __linux__
    return pipe2(ends, O_CLOEXEC | (nonblocking ? O_NONBLOCK : 0)) == 0;
#else
    if(pipe(ends) != 0)
    {
        return false;
    }
    for(const auto end : ends)
    {
        fcntl(end, F_SETFD, FD_CLOEXEC);
        if(nonblocking)
        {
            fcntl(end, F_SETFL, fcntl(end, F_GETFL) | O_NONBLOCK);
        }
    }
    return true;
#endif
}

} // namespace olagarro::oui
