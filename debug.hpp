#pragma once

#include "fmt/core.h"
#include <string>

#ifdef DEBUG
template <typename... Args>
void print_debug(const std::string &msg, Args... args)
{
    const std::string printme = fmt::format(msg, args...);
    fmt::print("[DEBUG] {}", printme);
}
#else
template <typename... Args>
void print_debug([[maybe_unused]] const std::string &msg, [[maybe_unused]] Args... args)
{
    ;
}
#endif
