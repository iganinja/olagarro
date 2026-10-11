#pragma once

#include <oui/backend.h>
#include <oui/cell_buffer.h>
#include <oui/event.h>

#include <signalslot/signalslot.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// OUI's own desktop: one program, the desktop, has the screen and shows the windows of the others, which draw
// themselves in memory they share with it and hear of their input from it, through a local socket. Each window is
// a whole number of cells, of the same size in every program, so what it draws lines up with the desktop's cells.
// A desktop in a terminal, as over SSH, shows cells, and its programs draw cells instead of pixels. Linux only.
namespace olagarro::oui
{

class Application;

// Where the desktop the programs started in it are to show in listens: OUI_DESKTOP says.
inline constexpr const char* desktop_variable{"OUI_DESKTOP"};

struct DesktopClientOptions
{
    // What the desktop shows over the window and in its task bar.
    std::string title{"OUI"};
    // The desktop's socket; OUI_DESKTOP's when empty.
    std::filesystem::path socket;
};

// A window of cells on OUI's desktop in a terminal. It throws when there is no desktop to show in, or the desktop
// shows pixels: a program that can show there too takes the overload in <oui/graphics/desktop.h>.
[[nodiscard]] std::unique_ptr<Backend> desktop_backend(DesktopClientOptions options = {});

// What a desktop shows: pixels, on a screen or in a window, or cells, in a terminal.
enum class DesktopFrames
{
    pixels,
    cells
};

// The desktop's side: it listens for programs, and tells on the application's thread what each asks to show. What
// it does with them, where their windows go and which has the keys, is the desktop's own.
class DesktopServer
{
  public:
    // A program, from when it says hello to when it goes.
    struct Client
    {
        int id{};
        std::string title;
        // Its last frame, `width` by `height`: pixels as 0x00RRGGBB on a desktop of pixels, cells on one of cells.
        // Empty until it draws one.
        std::vector<std::uint32_t> pixels;
        std::vector<Cell> cells;
        int width{};
        int height{};
        // The pointer it wants over its window.
        Pointer pointer{Pointer::arrow};
    };

    // Listens at `socket`, which must not be in use; it is removed when the server is destroyed. Programs draw
    // `frames`.
    DesktopServer(Application& application, std::filesystem::path socket, DesktopFrames frames = DesktopFrames::pixels);
    ~DesktopServer();
    DesktopServer(const DesktopServer&) = delete;
    DesktopServer& operator=(const DesktopServer&) = delete;

    [[nodiscard]] const std::filesystem::path& socket() const;
    [[nodiscard]] const Client* client(int id) const;
    // The size of a program's window, in cells, and whether it has the keys. A program waits for its first.
    void configure(int id, int columns, int rows, bool focused);
    // Input for a program, in its own cells: a mouse event from its window's top left corner.
    void send(int id, const Event& event);
    // Asks the program to close, as its close button does; it may ask about unsaved work first, or decline.
    void ask_to_close(int id);

    template <typename Callback> olagarro::signal_slot::Connection on_connected(Callback&& callback)
    {
        return m_connected.connect(std::forward<Callback>(callback));
    }
    // A new frame or a new title.
    template <typename Callback> olagarro::signal_slot::Connection on_changed(Callback&& callback)
    {
        return m_changed.connect(std::forward<Callback>(callback));
    }
    // A program asked for another pointer over its window.
    template <typename Callback> olagarro::signal_slot::Connection on_pointer(Callback&& callback)
    {
        return m_pointer.connect(std::forward<Callback>(callback));
    }
    template <typename Callback> olagarro::signal_slot::Connection on_disconnected(Callback&& callback)
    {
        return m_disconnected.connect(std::forward<Callback>(callback));
    }

    class Impl;

  private:
    std::unique_ptr<Impl> m_impl;
    olagarro::signal_slot::Signal<int> m_connected;
    olagarro::signal_slot::Signal<int> m_pointer;
    olagarro::signal_slot::Signal<int> m_changed;
    olagarro::signal_slot::Signal<int> m_disconnected;
};

} // namespace olagarro::oui
