#pragma once

#include <utility>

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

} // namespace olagarro::oui
