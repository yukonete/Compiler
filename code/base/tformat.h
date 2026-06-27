#ifndef BASE_TFORMAT_H
#define BASE_TFORMAT_H

#include <format>
#include <string_view>
#include <iterator>

#include "base/arena.h"

template <typename... Args>
std::string_view tformat(std::format_string<Args...> fmt, Args &&...args) {
    auto buffer = make_temp_vector<char>();
    std::format_to(std::back_inserter(buffer), fmt, std::forward<Args>(args)...);
    buffer.push_back('\0');
    // Vector will deallocate memory on destruction and that memory will be poisoned
    // So have to allocate memory again and copy vector contents here
    auto temp = temp_allocator.allocate<char>(buffer.size());
    std::ranges::copy(buffer, temp);
    return std::string_view{temp, buffer.size() - 1};
}

#endif