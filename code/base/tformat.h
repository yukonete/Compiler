#ifndef BASE_TFORMAT_H
#define BASE_TFORMAT_H

#include <format>
#include <string_view>
#include <iterator>

#include "base/arena.h"

template <typename... Args>
AllocatorString tformat(std::format_string<Args...> fmt, Args &&...args) {
    auto buffer = create_temp_string(fmt.get().size());
    std::format_to(std::back_inserter(buffer), fmt, std::forward<Args>(args)...);
    return buffer;
}

#endif