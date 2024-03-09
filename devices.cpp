#include "devices.hpp"

#include <string>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <regex>

#include "fmt/core.h"
#include "debug.hpp"


std::string get_internal_keyboard()
{
    const std::string path = "/proc/bus/input/devices";
    std::ifstream file_p(path);
    if (!file_p.is_open())
    {
        throw std::runtime_error(fmt::format("Unable to open {}", path));
    }

    bool is_keyboard = false;
    std::string line;
    std::string token;
    std::istringstream ss;
    std::string keyboard{};

    while (std::getline(file_p, line))
    {
        auto line_lower = line;
        std::transform(line_lower.begin(), line_lower.end(), line_lower.begin(), tolower);
        // get device name
        if (line_lower.find("name=") != std::string::npos)
        {
            is_keyboard = line_lower.find("keyboard") != std::string::npos &&
                          line_lower.find("wired") == std::string::npos && line_lower.find("usb") == std::string::npos;
            if (is_keyboard)
            {
                print_debug("Detected keyboard: {}\n", line_lower);
            }
            else
            {
                print_debug("Ignoring (non) keyboard device: {}\n", line_lower);
            }
        }

        if (line_lower.find("handlers=") != std::string::npos)
        {
            if (!is_keyboard)
            {
                continue;
            }

            ss = std::istringstream(line);
            while (std::getline(ss, token, ' '))
            {
                if (token.find("event") != std::string::npos)
                {
                    keyboard = "/dev/input/" + token;
                    print_debug("Added keyboard {}\n", keyboard);
                }
            }
        }
    }
    return keyboard;
}


std::string get_interal_mouse()
{
    return "/dev/input/mice";
    /*
    for (const auto &dev: std::filesystem::directory_iterator("/dev/input/by-path"))
    {
        print_debug("{}\n", dev.path().string());
        // if (regex_match(std::string(dev.path()), std::regex("..*event\\-mouse.*")))
        if (regex_search(dev.path().string(), std::regex("serio-.*event*-mouse.*")))
        {
            print_debug("Found internal mouse {}\n", dev.path().string());
            return dev.path().string();
        }
    }
    return "";
    */
}


[[nodiscard]] uint8_t read_current_brightness(const std::string &filename)
{
    FILE *fp = fopen(filename.c_str(), "r");
    if (fp == nullptr)
    {
        throw std::runtime_error(fmt::format("Cannot open {}", filename));
    }

    unsigned int data{};
    if (fscanf(fp, "%u", &data) != 1)
    {
        fclose(fp);
        throw std::runtime_error(fmt::format("Cannot read brightness from {}", filename));
    }

    fclose(fp);
    return static_cast<uint8_t>(data);
}


void write_brightness(const std::string &filename, const uint8_t val)
{
    FILE *fp = fopen(filename.c_str(), "w");
    if (fp == nullptr)
    {
        throw std::runtime_error(fmt::format("Cannot open {}", filename));
    }

    if (fprintf(fp, "%u", val) < 0)
    {
        fclose(fp);
        throw std::runtime_error(fmt::format("Cannot write uint8 to {}", filename));
    }
    fclose(fp);
}
