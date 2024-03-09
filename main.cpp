#include <mutex>
#include <thread>
#include <atomic>
#include <csignal>
#include <linux/input.h>
#include <fcntl.h>

#include "externals/sml.hpp"
#include "fmt/core.h"

#include "debug.hpp"
#include "devices.hpp"


namespace sml = boost::sml;
using namespace std::chrono_literals;


namespace controller
{
const std::string DEFAULT_BACKLIGHT_PATH = "/sys/class/leds/tpacpi::kbd_backlight/brightness";
const unsigned int DEFAULT_TIMEOUT_S = 10;

std::atomic<bool> g_end{false};
std::atomic<bool> g_reset_timer{false};
uint8_t cur_brightness{};

namespace events
{
    struct start_on{};
    struct start_off{};
    struct device_input{};
    struct timeout{};
}

namespace actions
{
const auto turn_off = []
{
    print_debug("Turning light off\n");
    write_brightness(DEFAULT_BACKLIGHT_PATH, 0);
};

const auto store_cur_brightness = []
{
    cur_brightness = read_current_brightness(DEFAULT_BACKLIGHT_PATH);
    print_debug("Storing current brightness of {}\n", cur_brightness);
};

const auto reset_timer = []
{
    g_reset_timer = true;
};

const auto write_cur_brightness = []
{
    print_debug("Writing current brightness of {}\n", cur_brightness);
    write_brightness(DEFAULT_BACKLIGHT_PATH, cur_brightness);
};
} // namespace actions

struct SML
{
    auto operator()() const noexcept
    {
        using namespace sml;
        return make_transition_table(
            *"s_init"_s + event<events::start_on> = "s_on"_s,
            "s_init"_s + event<events::start_off> = "s_off"_s,
            "s_off"_s + event<events::device_input> / actions::write_cur_brightness = "s_on"_s,
            "s_on"_s + event<events::device_input> / actions::reset_timer = "s_on"_s,
            "s_on"_s + event<events::timeout> / (actions::store_cur_brightness, actions::turn_off) = "s_off"_s
        );
    }
};
sml::sm<SML, sml::thread_safe<std::recursive_mutex>> state_machine;


void countdown_thread(const unsigned int timeout_s)
{
    auto countdown = timeout_s;
    while (!g_end)
    {
        countdown--;
        std::this_thread::sleep_for(1s);
        if (g_reset_timer)
        {
            countdown = timeout_s;
            g_reset_timer = false;
        }

        if (countdown == 0)
        {
            print_debug("countdown reached zero\n");
            state_machine.process_event(events::timeout{});
            countdown = timeout_s;
        }
    }
}

void read_events(const std::string &event_path)
{
    int dev_fd = open(event_path.c_str(), O_RDONLY);
    if (dev_fd < 0)
    {
        throw std::runtime_error(fmt::format("Cannot open event file {}", event_path));
    }

    while (!g_end)
    {
        struct input_event ie{};
        auto rd = read(dev_fd, &ie, sizeof(struct input_event));
        if (rd != 0)
        {
            state_machine.process_event(events::device_input{});
        }
    }
    close(dev_fd);
}

} // namespace controller



void signal_handler(int sig)
{
    switch (sig)
    {
    case SIGTERM:
    case SIGINT:
        controller::g_end = true;
        break;
    default:
        break;
    }
}

int main(int argc, char **argv)
{
    unsigned int timeout_s = controller::DEFAULT_TIMEOUT_S;

    if (argc == 2)
    {
        timeout_s = std::atoi(argv[1]);
        if (timeout_s == 0)
        {
            fmt::print("Error: Invalid timeout parameter passed\n");
            return EXIT_SUCCESS;
        }
    }

    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    const auto keyboard = get_internal_keyboard();
    const auto mouse = get_interal_mouse();
    controller::cur_brightness = read_current_brightness(controller::DEFAULT_BACKLIGHT_PATH);

    if (keyboard.empty())
    {
        fmt::print("Could not determine keyboard event file\n");
        return EXIT_FAILURE;
    }
    if (mouse.empty())
    {
        fmt::print("Could not determine mouse event file\n");
        return EXIT_FAILURE;
    }

    fmt::print("Listening on keyboard {} and mouse {}, cur. brightness={}\n", keyboard, mouse, controller::cur_brightness);

    if (controller::cur_brightness != 0)
    {
        controller::state_machine.process_event(controller::events::start_on{});
    }
    else
    {
        controller::state_machine.process_event(controller::events::start_off{});
    }

    auto timer_thread = std::thread(controller::countdown_thread, timeout_s);
    std::thread(controller::read_events, keyboard).detach();
    std::thread(controller::read_events, mouse).detach();

    while (!controller::g_end)
    {
        std::this_thread::sleep_for(1s);
    }
    print_debug("Exiting ...\n");
    timer_thread.join();

    return EXIT_SUCCESS;
}
