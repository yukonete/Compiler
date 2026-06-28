#ifndef BASE_TFORMAT_H
#define BASE_TFORMAT_H

#include <format>
#include <string_view>
#include <iterator>
#include <print>

#include "base/arena.h"

template <typename... Args>
DynamicArenaString tformat(std::format_string<Args...> fmt, Args &&...args) {
    auto buffer = make_temp_string();
    std::format_to(std::back_inserter(buffer), fmt, std::forward<Args>(args)...);
    return buffer;
}

#endif