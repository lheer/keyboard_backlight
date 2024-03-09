#pragma once

#include <cstdint>
#include <string>

std::string get_internal_keyboard();
std::string get_interal_mouse();
[[nodiscard]] uint8_t read_current_brightness(const std::string &filename);
void write_brightness(const std::string &filename, const uint8_t val);
