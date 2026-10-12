#include <oui/graphics/drm_backend.h>

#include <oui/crash_reporter.h>

#include "wheel.h"
#include "../descriptor.h"
#include "keyboard_layout.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <csignal>
#include <cerrno>
#include <cstring>
#include <deque>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// The kernel's own definitions, which some systems keep among the kernel's headers and others with libdrm's;
// only the headers, no library.
#if __has_include(<drm/drm.h>)
#include <drm/drm.h>
#include <drm/drm_mode.h>
#else
#include <libdrm/drm.h>
#include <libdrm/drm_mode.h>
#endif
#include <fcntl.h>
#include <linux/input.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace olagarro::oui
{

namespace
{

// What ioctl takes as its request: glibc an unsigned long, musl an int, which the kernel reads back as the same
// bits.
#ifdef __GLIBC__
using IoctlRequest = unsigned long;
#else
using IoctlRequest = int;
#endif

// An ioctl, tried again while a signal interrupts it.
int control(int fd, unsigned long request, void* argument)
{
    int result{};
    do
    {
        result = ioctl(fd, static_cast<IoctlRequest>(request), argument);
    } while(result == -1 && (errno == EINTR || errno == EAGAIN));
    return result;
}

// An ioctl whose argument is a number rather than a place.
int control_with(int fd, unsigned long request, int value)
{
    return ioctl(fd, static_cast<IoctlRequest>(request), value);
}

[[noreturn]] void fail(const std::string& what)
{
    throw std::runtime_error{"Cannot take the screen: " + what + " (" + std::strerror(errno) + ")"};
}

[[nodiscard]] bool has_bit(const std::vector<unsigned long>& bits, unsigned int bit)
{
    constexpr auto per_word{sizeof(unsigned long) * 8};
    return bit / per_word < bits.size() && ((bits[bit / per_word] >> (bit % per_word)) & 1ul) != 0;
}

[[nodiscard]] std::vector<unsigned long> event_bits(int fd, unsigned int type, unsigned int count)
{
    constexpr auto per_word{sizeof(unsigned long) * 8};
    std::vector<unsigned long> bits((count + per_word - 1) / per_word);
    if(control(fd, EVIOCGBIT(type, bits.size() * sizeof(unsigned long)), bits.data()) < 0)
    {
        bits.clear();
    }
    return bits;
}

// What the signal handlers reach, which can only be what is global.
volatile sig_atomic_t stop_signal{0};
volatile sig_atomic_t release_asked{0};
volatile sig_atomic_t acquire_asked{0};
std::atomic<int> wake_fd{-1};
std::atomic<int> console_fd{-1};
// How the console read the keyboard before the program told it to leave the keyboard alone.
std::atomic<int> console_keyboard_mode{-1};

// The kernel asked the program to stop, or to give the screen to another console, or gave it back.
void on_signal(int signal)
{
    if(signal == SIGUSR1)
    {
        release_asked = 1;
    }
    else if(signal == SIGUSR2)
    {
        acquire_asked = 1;
    }
    else
    {
        stop_signal = signal;
    }
    if(const auto fd{wake_fd.load()}; fd >= 0)
    {
        const std::uint64_t one{1};
        if(write(fd, &one, sizeof one) < 0)
        {
            // A wake-up already pending is enough, so a failed write needs no handling.
        }
    }
}

// A crash leaves the console as the program had it, drawing nothing; this gives it its text back.
void restore_console_after_crash()
{
    if(const auto fd{console_fd.load()}; fd >= 0)
    {
        vt_mode automatic{};
        automatic.mode = VT_AUTO;
        ioctl(fd, static_cast<IoctlRequest>(VT_SETMODE), &automatic);
        ioctl(fd, static_cast<IoctlRequest>(KDSETMODE), KD_TEXT);
        if(const auto mode{console_keyboard_mode.load()}; mode >= 0)
        {
            ioctl(fd, static_cast<IoctlRequest>(KDSKBMODE), mode);
        }
    }
}

// The pointer, drawn over the frame: an arrow, white with a black edge, its tip at the top left.
constexpr std::array<std::string_view, 18> arrow{{
    "X",
    "XX",
    "X.X",
    "X..X",
    "X...X",
    "X....X",
    "X.....X",
    "X......X",
    "X.......X",
    "X........X",
    "X.....XXXXX",
    "X..X..X",
    "X.X X..X",
    "XX  X..X",
    "X    X..X",
    "     X..X",
    "      X..X",
    "      XXX",
}};

// Over a line that moves left and right, or up and down: a double arrow along it, its middle where the pointer is.
constexpr std::array<std::string_view, 7> resize_columns_arrow{{
    "   X         X   ",
    "  XX         XX  ",
    " X.XXXXXXXXXXX.X ",
    "X...............X",
    " X.XXXXXXXXXXX.X ",
    "  XX         XX  ",
    "   X         X   ",
}};
constexpr std::array<std::string_view, 17> resize_rows_arrow{{
    "   X   ",
    "  X.X  ",
    " X...X ",
    "XXX.XXX",
    "  X.X  ",
    "  X.X  ",
    "  X.X  ",
    "  X.X  ",
    "  X.X  ",
    "  X.X  ",
    "  X.X  ",
    "  X.X  ",
    "  X.X  ",
    "XXX.XXX",
    " X...X ",
    "  X.X  ",
    "   X   ",
}};

class DrmBackend final : public PixelBackend
{
  public:
    explicit DrmBackend(const DrmOptions& options) : PixelBackend{options.style}, m_keyboard{keyboard(options)}
    {
        m_card = Descriptor{open(options.card.c_str(), O_RDWR | O_CLOEXEC)};
        if(m_card.get() < 0)
        {
            fail("cannot open " + options.card);
        }
        // Its events, as a flip done, are read only when there are some.
        fcntl(m_card.get(), F_SETFL, fcntl(m_card.get(), F_GETFL) | O_NONBLOCK);
        take_output();
        find_input();
        m_wake = Descriptor{eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)};
        wake_fd = m_wake.get();
        take_console();
        for(const auto signal : {SIGTERM, SIGHUP, SIGUSR1, SIGUSR2})
        {
            struct sigaction action
            {
            };
            action.sa_handler = on_signal;
            action.sa_flags = SA_RESTART;
            sigemptyset(&action.sa_mask);
            sigaction(signal, &action, nullptr);
        }
        m_pointer_x = m_width / 2;
        m_pointer_y = m_height / 2;
        resize(m_width, m_height);
    }

    ~DrmBackend() override
    {
        for(const auto signal : {SIGTERM, SIGHUP, SIGUSR1, SIGUSR2})
        {
            std::signal(signal, SIG_DFL);
        }
        wake_fd = -1;
        if(m_console.get() >= 0)
        {
            restore_console_after_crash();
            console_fd = -1;
        }
        // What the screen showed before, as the console, comes back.
        if(m_saved_crtc.crtc_id != 0)
        {
            m_saved_crtc.set_connectors_ptr = reinterpret_cast<std::uintptr_t>(&m_connector);
            m_saved_crtc.count_connectors = 1;
            control(m_card.get(), DRM_IOCTL_MODE_SETCRTC, &m_saved_crtc);
        }
        for(auto& buffer : m_buffers)
        {
            if(buffer.map != nullptr)
            {
                munmap(buffer.map, buffer.size);
            }
            if(buffer.framebuffer != 0)
            {
                control(m_card.get(), DRM_IOCTL_MODE_RMFB, &buffer.framebuffer);
            }
            if(buffer.handle != 0)
            {
                drm_mode_destroy_dumb destroy{};
                destroy.handle = buffer.handle;
                control(m_card.get(), DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
            }
        }
    }

    [[nodiscard]] bool reports_modifiers() const override { return true; }

    std::optional<Event> poll_event() override
    {
        if(release_asked != 0)
        {
            release_asked = 0;
            release_screen();
        }
        if(acquire_asked != 0)
        {
            acquire_asked = 0;
            acquire_screen();
        }
        if(stop_signal != 0 && !m_stop_reported)
        {
            m_stop_reported = true;
            return ShutdownEvent{stop_signal == SIGTERM ? ShutdownEvent::Reason::terminated
                                                        : ShutdownEvent::Reason::terminal_closed};
        }
        read_card_events();
        if(m_pending.empty())
        {
            read_input();
        }
        if(m_pending.empty())
        {
            return std::nullopt;
        }
        auto event{m_pending.front()};
        m_pending.pop_front();
        return event;
    }

    void wait_for_input(std::optional<std::chrono::steady_clock::time_point> deadline) override
    {
        if(!m_pending.empty())
        {
            return;
        }
        std::vector<pollfd> watched;
        for(const auto& device : m_devices)
        {
            watched.push_back({device.fd.get(), POLLIN, 0});
        }
        watched.push_back({m_wake.get(), POLLIN, 0});
        watched.push_back({m_card.get(), POLLIN, 0});
        auto timeout{-1};
        if(deadline)
        {
            const auto left{
                std::chrono::duration_cast<std::chrono::milliseconds>(*deadline - std::chrono::steady_clock::now())};
            timeout = static_cast<int>(std::max<std::chrono::milliseconds::rep>(0, left.count()));
        }
        poll(watched.data(), watched.size(), timeout);
        std::uint64_t woken{};
        if(read(m_wake.get(), &woken, sizeof woken) < 0)
        {
            // Nothing waiting to be read is fine: the wait is over either way.
        }
    }

    void wake() override
    {
        const std::uint64_t one{1};
        if(write(m_wake.get(), &one, sizeof one) < 0)
        {
            // A wake-up already pending is enough, so a failed write needs no handling.
        }
    }

  protected:
    void show(const Canvas&) override { present(); }

    void set_pointer(Pointer pointer) override
    {
        m_pointer = pointer;
        if(m_active)
        {
            present();
        }
    }

  private:
    struct Buffer
    {
        std::uint32_t handle{};
        std::uint32_t framebuffer{};
        std::uint32_t pitch{};
        std::uint8_t* map{};
        std::size_t size{};
    };

    struct Device
    {
        Descriptor fd;
        bool keyboard{false};
        // A tablet, as a virtual machine's, says where the pointer is rather than how far it moved.
        bool absolute{false};
        input_absinfo x_range{};
        input_absinfo y_range{};
    };

    // Finds a connected screen, its preferred mode and a CRTC to drive it, and shows a buffer of our own on it.
    void take_output()
    {
        const auto fd{m_card.get()};
        drm_mode_card_res resources{};
        if(control(fd, DRM_IOCTL_MODE_GETRESOURCES, &resources) != 0)
        {
            fail("the card has no mode setting");
        }
        std::vector<std::uint32_t> connectors(resources.count_connectors);
        std::vector<std::uint32_t> crtcs(resources.count_crtcs);
        std::vector<std::uint32_t> encoders(resources.count_encoders);
        std::vector<std::uint32_t> framebuffers(resources.count_fbs);
        resources.connector_id_ptr = reinterpret_cast<std::uintptr_t>(connectors.data());
        resources.crtc_id_ptr = reinterpret_cast<std::uintptr_t>(crtcs.data());
        resources.encoder_id_ptr = reinterpret_cast<std::uintptr_t>(encoders.data());
        resources.fb_id_ptr = reinterpret_cast<std::uintptr_t>(framebuffers.data());
        if(control(fd, DRM_IOCTL_MODE_GETRESOURCES, &resources) != 0)
        {
            fail("cannot read the card's resources");
        }
        for(const auto connector_id : connectors)
        {
            drm_mode_get_connector connector{};
            connector.connector_id = connector_id;
            if(control(fd, DRM_IOCTL_MODE_GETCONNECTOR, &connector) != 0 || connector.connection != 1 ||
               connector.count_modes == 0)
            {
                continue;
            }
            std::vector<drm_mode_modeinfo> modes(connector.count_modes);
            std::vector<std::uint32_t> connector_encoders(connector.count_encoders);
            connector.modes_ptr = reinterpret_cast<std::uintptr_t>(modes.data());
            connector.encoders_ptr = reinterpret_cast<std::uintptr_t>(connector_encoders.data());
            connector.count_props = 0;
            connector.props_ptr = 0;
            connector.prop_values_ptr = 0;
            if(control(fd, DRM_IOCTL_MODE_GETCONNECTOR, &connector) != 0)
            {
                continue;
            }
            const auto preferred{std::find_if(modes.begin(), modes.end(), [](const drm_mode_modeinfo& mode)
                                              { return (mode.type & DRM_MODE_TYPE_PREFERRED) != 0; })};
            m_mode = preferred != modes.end() ? *preferred : modes.front();
            m_connector = connector_id;
            m_crtc = crtc_for(connector, connector_encoders, crtcs);
            if(m_crtc != 0)
            {
                break;
            }
        }
        if(m_crtc == 0)
        {
            errno = ENODEV;
            fail("no screen is connected");
        }
        m_width = m_mode.hdisplay;
        m_height = m_mode.vdisplay;

        for(auto& buffer : m_buffers)
        {
            buffer = make_buffer();
        }

        m_saved_crtc.crtc_id = m_crtc;
        if(control(fd, DRM_IOCTL_MODE_GETCRTC, &m_saved_crtc) != 0)
        {
            m_saved_crtc = {};
        }
        drm_mode_crtc set{};
        set.crtc_id = m_crtc;
        set.fb_id = m_buffers[m_front].framebuffer;
        set.set_connectors_ptr = reinterpret_cast<std::uintptr_t>(&m_connector);
        set.count_connectors = 1;
        set.mode = m_mode;
        set.mode_valid = 1;
        if(control(fd, DRM_IOCTL_MODE_SETCRTC, &set) != 0)
        {
            fail("cannot show the buffer, as when another program holds the screen");
        }
    }

    // A buffer the size of the screen, which the card can show and the program can write, black to start.
    [[nodiscard]] Buffer make_buffer()
    {
        const auto fd{m_card.get()};
        Buffer buffer;
        drm_mode_create_dumb dumb{};
        dumb.width = static_cast<std::uint32_t>(m_width);
        dumb.height = static_cast<std::uint32_t>(m_height);
        dumb.bpp = 32;
        if(control(fd, DRM_IOCTL_MODE_CREATE_DUMB, &dumb) != 0)
        {
            fail("cannot make a buffer for the screen");
        }
        buffer.handle = dumb.handle;
        buffer.pitch = dumb.pitch;
        buffer.size = static_cast<std::size_t>(dumb.size);

        drm_mode_fb_cmd framebuffer{};
        framebuffer.width = dumb.width;
        framebuffer.height = dumb.height;
        framebuffer.pitch = dumb.pitch;
        framebuffer.bpp = 32;
        framebuffer.depth = 24;
        framebuffer.handle = dumb.handle;
        if(control(fd, DRM_IOCTL_MODE_ADDFB, &framebuffer) != 0)
        {
            fail("cannot make a frame of the buffer");
        }
        buffer.framebuffer = framebuffer.fb_id;

        drm_mode_map_dumb map{};
        map.handle = dumb.handle;
        if(control(fd, DRM_IOCTL_MODE_MAP_DUMB, &map) != 0)
        {
            fail("cannot reach the buffer");
        }
        auto* const memory{
            mmap(nullptr, buffer.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, static_cast<off_t>(map.offset))};
        if(memory == MAP_FAILED)
        {
            fail("cannot map the buffer");
        }
        buffer.map = static_cast<std::uint8_t*>(memory);
        std::memset(buffer.map, 0, buffer.size);
        return buffer;
    }

    // Shows the frame with the arrow over it: drawn in the buffer the screen is not showing, which the card then
    // shows from the next refresh on, so no half-drawn frame is ever seen. A frame asked for while the last still
    // waits for its refresh is drawn once that one is up, with whatever changed meanwhile.
    void present()
    {
        if(!m_active)
        {
            return;
        }
        if(m_flip_pending)
        {
            m_present_wanted = true;
            return;
        }
        m_present_wanted = false;
        const auto back{1 - m_front};
        auto& buffer{m_buffers[back]};
        const auto& frame{canvas()};
        const auto& pixels{frame.pixels()};
        const auto rows{std::min(m_height, frame.height())};
        const auto columns{static_cast<std::size_t>(std::min(m_width, frame.width()))};
        for(int y{}; y < rows; ++ y)
        {
            std::memcpy(buffer.map + static_cast<std::size_t>(y) * buffer.pitch,
                        pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width()),
                        columns * 4);
        }
        draw_pointer(buffer);
        // Some cards, as a virtual machine's, show only what they are told changed.
        drm_mode_fb_dirty_cmd dirty{};
        dirty.fb_id = buffer.framebuffer;
        control(m_card.get(), DRM_IOCTL_MODE_DIRTYFB, &dirty);
        drm_mode_crtc_page_flip flip{};
        flip.crtc_id = m_crtc;
        flip.fb_id = buffer.framebuffer;
        flip.flags = DRM_MODE_PAGE_FLIP_EVENT;
        if(control(m_card.get(), DRM_IOCTL_MODE_PAGE_FLIP, &flip) == 0)
        {
            m_flip_pending = true;
        }
        else
        {
            // A card that cannot flip shows the buffer at once instead.
            show_now(buffer.framebuffer);
        }
        m_front = back;
    }

    void show_now(std::uint32_t framebuffer)
    {
        drm_mode_crtc set{};
        set.crtc_id = m_crtc;
        set.fb_id = framebuffer;
        set.set_connectors_ptr = reinterpret_cast<std::uintptr_t>(&m_connector);
        set.count_connectors = 1;
        set.mode = m_mode;
        set.mode_valid = 1;
        control(m_card.get(), DRM_IOCTL_MODE_SETCRTC, &set);
    }

    // The card says a flip is up: the next frame may go, and one asked for meanwhile does.
    void read_card_events()
    {
        std::array<char, 1024> events{};
        for(;;)
        {
            const auto got{read(m_card.get(), events.data(), events.size())};
            if(got <= 0)
            {
                break;
            }
            for(std::size_t at{}; at + sizeof(drm_event) <= static_cast<std::size_t>(got);)
            {
                drm_event header{};
                std::memcpy(&header, events.data() + at, sizeof header);
                if(header.type == DRM_EVENT_FLIP_COMPLETE)
                {
                    m_flip_pending = false;
                }
                at += std::max<std::size_t>(header.length, sizeof header);
            }
        }
        if(!m_flip_pending && m_present_wanted)
        {
            present();
        }
    }

    // The CRTC that drives the connector now, or the first its encoders can use.
    [[nodiscard]] std::uint32_t crtc_for(const drm_mode_get_connector& connector,
                                         const std::vector<std::uint32_t>& encoders,
                                         const std::vector<std::uint32_t>& crtcs) const
    {
        const auto fd{m_card.get()};
        if(connector.encoder_id != 0)
        {
            drm_mode_get_encoder encoder{};
            encoder.encoder_id = connector.encoder_id;
            if(control(fd, DRM_IOCTL_MODE_GETENCODER, &encoder) == 0 && encoder.crtc_id != 0)
            {
                return encoder.crtc_id;
            }
        }
        for(const auto encoder_id : encoders)
        {
            drm_mode_get_encoder encoder{};
            encoder.encoder_id = encoder_id;
            if(control(fd, DRM_IOCTL_MODE_GETENCODER, &encoder) != 0)
            {
                continue;
            }
            for(std::size_t index{}; index < crtcs.size(); ++ index)
            {
                if((encoder.possible_crtcs & (1u << index)) != 0)
                {
                    return crtcs[index];
                }
            }
        }
        return 0;
    }

    // Opens every keyboard and pointer there is.
    void find_input()
    {
        for(int index{}; index < 64; ++ index)
        {
            const auto path{"/dev/input/event" + std::to_string(index)};
            Descriptor fd{open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC)};
            if(fd.get() < 0)
            {
                continue;
            }
            const auto types{event_bits(fd.get(), 0, EV_MAX)};
            const auto keys{event_bits(fd.get(), EV_KEY, KEY_MAX)};
            Device device;
            device.keyboard = has_bit(types, EV_KEY) && has_bit(keys, KEY_A) && has_bit(keys, KEY_ENTER);
            const auto relative{has_bit(types, EV_REL) && has_bit(event_bits(fd.get(), EV_REL, REL_MAX), REL_X)};
            device.absolute = has_bit(types, EV_ABS) && has_bit(event_bits(fd.get(), EV_ABS, ABS_MAX), ABS_X);
            if(device.absolute)
            {
                control(fd.get(), EVIOCGABS(ABS_X), &device.x_range);
                control(fd.get(), EVIOCGABS(ABS_Y), &device.y_range);
            }
            if(!device.keyboard && !relative && !device.absolute)
            {
                continue;
            }
            device.fd = std::move(fd);
            m_devices.push_back(std::move(device));
        }
    }

    void read_input()
    {
        for(auto& device : m_devices)
        {
            std::array<input_event, 64> events{};
            for(;;)
            {
                const auto got{read(device.fd.get(), events.data(), sizeof events)};
                if(got <= 0)
                {
                    break;
                }
                // What is typed and pointed at while another console has the screen is that console's.
                for(std::size_t index{}; m_active && index < static_cast<std::size_t>(got) / sizeof(input_event);
                    ++ index)
                {
                    take(device, events[index]);
                }
            }
        }
    }

    void take(const Device& device, const input_event& event)
    {
        switch(event.type)
        {
        case EV_KEY:
            if(event.code == BTN_LEFT || event.code == BTN_RIGHT)
            {
                take_button(event.code == BTN_RIGHT, event.value != 0);
            }
            else if(device.keyboard)
            {
                take_key(event.code, event.value);
            }
            break;
        case EV_REL:
            if(event.code == REL_X)
            {
                m_pointer_x = std::clamp(m_pointer_x + event.value, 0, m_width - 1);
                m_pointer_moved = true;
            }
            else if(event.code == REL_Y)
            {
                m_pointer_y = std::clamp(m_pointer_y + event.value, 0, m_height - 1);
                m_pointer_moved = true;
            }
            else if(event.code == REL_WHEEL)
            {
                take_wheel(event.value);
            }
            break;
        case EV_ABS:
        {
            const auto scaled{[](int value, const input_absinfo& range, int size)
                              {
                                  const auto span{std::max(1, range.maximum - range.minimum)};
                                  return std::clamp((value - range.minimum) * (size - 1) / span, 0, size - 1);
                              }};
            if(event.code == ABS_X)
            {
                m_pointer_x = scaled(event.value, device.x_range, m_width);
                m_pointer_moved = true;
            }
            else if(event.code == ABS_Y)
            {
                m_pointer_y = scaled(event.value, device.y_range, m_height);
                m_pointer_moved = true;
            }
            break;
        }
        case EV_SYN:
            if(m_pointer_moved)
            {
                m_pointer_moved = false;
                pointer_moved();
            }
            break;
        default:
            break;
        }
    }

    void take_key(unsigned int code, int value)
    {
        // Ctrl+Alt and a function key goes to that console, as on any Linux; the console the program runs on
        // leaves the keyboard alone, so the program does it.
        if(value == 1 && m_keyboard.control() && m_keyboard.alt() && code >= KEY_F1 && code <= KEY_F10 &&
           m_console.get() >= 0)
        {
            control_with(m_console.get(), VT_ACTIVATE, static_cast<int>(code - KEY_F1 + 1));
            return;
        }
        for(auto& event : m_keyboard.key(code, value))
        {
            m_pending.push_back(std::move(event));
        }
    }

    [[nodiscard]] static KeyboardLayout keyboard(const DrmOptions& options)
    {
        auto layout{KeyboardLayout::named(options.keyboard)};
        if(!layout)
        {
            throw std::runtime_error{"No keyboard layout called " + options.keyboard};
        }
        return *layout;
    }

    [[nodiscard]] std::optional<Point> cell_at_pointer() const
    {
        const auto& content{content_area()};
        if(m_pointer_x < content.x || m_pointer_y < content.y || m_pointer_x >= content.x + content.width ||
           m_pointer_y >= content.y + content.height)
        {
            return std::nullopt;
        }
        return Point{(m_pointer_x - content.x) / cell_size().width,
                     GraphicalPainter::row_at(m_pointer_y - content.y, content.height, cell_size().height)};
    }

    [[nodiscard]] int offset_in_cell() const { return std::max(0, m_pointer_x - content_area().x) % cell_size().width; }

    void take_button(bool right, bool pressed)
    {
        (right ? m_right_down : m_left_down) = pressed;
        const auto cell{cell_at_pointer()};
        if(!cell && pressed)
        {
            return;
        }
        const auto at{cell.value_or(m_last_cell.value_or(Point{}))};
        MouseEvent mouse{at.x, at.y, pressed};
        mouse.cell_offset = offset_in_cell();
        mouse.released = !pressed;
        mouse.right = right;
        mouse.shift = m_keyboard.shift();
        mouse.control = m_keyboard.control();
        mouse.alt = m_keyboard.alt();
        m_pending.push_back(mouse);
    }

    void take_wheel(int notches)
    {
        if(const auto cell{cell_at_pointer()})
        {
            for(const auto& mouse :
                wheel_events(*cell, notches, m_keyboard.control(), m_keyboard.alt(), m_keyboard.shift()))
            {
                m_pending.push_back(mouse);
            }
        }
    }

    void pointer_moved()
    {
        if(!m_active)
        {
            return;
        }
        // The arrow follows at once, whether or not the application has anything to draw.
        present();
        const auto cell{cell_at_pointer()};
        if(!cell)
        {
            return;
        }
        const auto offset{offset_in_cell()};
        if(m_last_cell && m_last_cell->x == cell->x && m_last_cell->y == cell->y &&
           (!m_left_down || offset == m_last_offset))
        {
            return;
        }
        m_last_cell = cell;
        m_last_offset = offset;
        MouseEvent mouse{cell->x, cell->y, false};
        mouse.cell_offset = offset;
        if(m_left_down)
        {
            mouse.dragging = true;
        }
        else
        {
            mouse.hover = true;
        }
        m_pending.push_back(mouse);
    }

    void draw_pointer(Buffer& buffer)
    {
        if(m_pointer == Pointer::resize_columns)
        {
            draw_shape(buffer, resize_columns_arrow, true);
        }
        else if(m_pointer == Pointer::resize_rows)
        {
            draw_shape(buffer, resize_rows_arrow, true);
        }
        else
        {
            draw_shape(buffer, arrow, false);
        }
    }

    // A shape of X for black and . for white, its top left at the pointer, or its middle when `centred`.
    template <std::size_t Rows>
    void draw_shape(Buffer& buffer, const std::array<std::string_view, Rows>& shape, bool centred)
    {
        const auto left{m_pointer_x - (centred ? static_cast<int>(shape.front().size()) / 2 : 0)};
        const auto top{m_pointer_y - (centred ? static_cast<int>(Rows) / 2 : 0)};
        for(std::size_t row{}; row < Rows; ++ row)
        {
            for(std::size_t column{}; column < shape[row].size(); ++ column)
            {
                const auto mark{shape[row][column]};
                if(mark != ' ')
                {
                    put(buffer, left + static_cast<int>(column), top + static_cast<int>(row),
                        mark == 'X' ? 0x000000u : 0xFFFFFFu);
                }
            }
        }
    }

    void put(Buffer& buffer, int x, int y, std::uint32_t pixel)
    {
        if(x >= 0 && y >= 0 && x < m_width && y < m_height)
        {
            std::memcpy(buffer.map + static_cast<std::size_t>(y) * buffer.pitch + static_cast<std::size_t>(x) * 4,
                        &pixel, 4);
        }
    }

    // The console the program runs on, which stops drawing while the screen is ours and tells the program before
    // another console takes it, so it can let go.
    void take_console()
    {
        // The program's own console, as a session on one has, which its user owns; or, for a program with none,
        // as the first program of a system, the one showing now, which only root may take.
        m_console = Descriptor{open("/dev/tty", O_RDWR | O_CLOEXEC)};
        // Only a console answers what mode it is in.
        int console_mode{};
        const auto is_console{m_console.get() >= 0 && control(m_console.get(), KDGETMODE, &console_mode) == 0};
        if(!is_console)
        {
            Descriptor any{open("/dev/tty0", O_RDWR | O_CLOEXEC)};
            vt_stat state{};
            if(any.get() < 0 || control(any.get(), VT_GETSTATE, &state) != 0)
            {
                m_console = Descriptor{};
                return;
            }
            m_console = Descriptor{open(("/dev/tty" + std::to_string(state.v_active)).c_str(), O_RDWR | O_CLOEXEC)};
            if(m_console.get() < 0)
            {
                return;
            }
        }
        console_fd = m_console.get();
        add_crash_cleanup(restore_console_after_crash);
        control_with(m_console.get(), KDSETMODE, KD_GRAPHICS);
        // The console leaves the keyboard alone, so what is typed does not also reach the shell behind; the kernel
        // still sees every key, so it knows what is held when another console takes the keyboard.
        int mode{};
        if(control(m_console.get(), KDGKBMODE, &mode) == 0)
        {
            console_keyboard_mode = mode;
            control_with(m_console.get(), KDSKBMODE, K_OFF);
        }
        vt_mode told{};
        told.mode = VT_PROCESS;
        told.relsig = SIGUSR1;
        told.acqsig = SIGUSR2;
        control(m_console.get(), VT_SETMODE, &told);
    }

    // Another console takes the screen: the screen and the keyboard go to it.
    void release_screen()
    {
        m_active = false;
        control(m_card.get(), DRM_IOCTL_DROP_MASTER, nullptr);
        control_with(m_console.get(), VT_RELDISP, 1);
    }

    // The screen comes back: ours again, as it was, with the keys held when it went forgotten.
    void acquire_screen()
    {
        control_with(m_console.get(), VT_RELDISP, VT_ACKACQ);
        control(m_card.get(), DRM_IOCTL_SET_MASTER, nullptr);
        show_now(m_buffers[m_front].framebuffer);
        take_held_keys();
        m_active = true;
        m_flip_pending = false;
        present();
    }

    // The modifiers as the keyboards hold them now, which changed while another console had them: Ctrl and Alt
    // still down from Ctrl+Alt+F1 make the F1 after them go to the console, not the application.
    void take_held_keys()
    {
        m_keyboard.forget();
        for(const auto& device : m_devices)
        {
            if(!device.keyboard)
            {
                continue;
            }
            const auto held{
                [&device]
                {
                    constexpr auto per_word{sizeof(unsigned long) * 8};
                    std::vector<unsigned long> bits((KEY_MAX + per_word - 1) / per_word);
                    if(control(device.fd.get(), EVIOCGKEY(bits.size() * sizeof(unsigned long)), bits.data()) != 0)
                    {
                        bits.clear();
                    }
                    return bits;
                }()};
            for(const auto code :
                {KEY_LEFTCTRL, KEY_RIGHTCTRL, KEY_LEFTSHIFT, KEY_RIGHTSHIFT, KEY_LEFTALT, KEY_RIGHTALT})
            {
                if(has_bit(held, static_cast<unsigned int>(code)))
                {
                    static_cast<void>(m_keyboard.key(static_cast<unsigned int>(code), 1));
                }
            }
        }
        m_pending.push_back(ModifiersEvent{m_keyboard.control(), m_keyboard.alt(), m_keyboard.shift()});
    }

    KeyboardLayout m_keyboard;
    // Whether the screen is ours, rather than another console's.
    bool m_active{true};
    bool m_stop_reported{false};
    Descriptor m_card;
    Descriptor m_console;
    Descriptor m_wake;
    std::uint32_t m_connector{};
    std::uint32_t m_crtc{};
    drm_mode_modeinfo m_mode{};
    drm_mode_crtc m_saved_crtc{};
    std::array<Buffer, 2> m_buffers{};
    // The buffer the screen shows, or will from the next refresh.
    int m_front{0};
    bool m_flip_pending{false};
    bool m_present_wanted{false};
    int m_width{};
    int m_height{};
    std::vector<Device> m_devices;
    std::deque<Event> m_pending;
    int m_pointer_x{};
    int m_pointer_y{};
    bool m_pointer_moved{false};
    // What the pointer looks like, as the application last asked.
    Pointer m_pointer{Pointer::arrow};
    bool m_left_down{false};
    bool m_right_down{false};
    std::optional<Point> m_last_cell;
    int m_last_offset{};
};

} // namespace

std::unique_ptr<PixelBackend> drm_backend(DrmOptions options)
{
    return std::make_unique<DrmBackend>(options);
}

} // namespace olagarro::oui
