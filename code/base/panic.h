#ifndef PANIC_H
#define PANIC_H

#include <cstdio>
#include <format>
#include <print>
#include <source_location>
#include <stacktrace>
#include <utility>
#include <cstdlib>

#include "base/types.h"

template <typename... Args>
[[noreturn]] void panic_(std::format_string<Args...> fmt,
                         std::source_location loc, Args &&...args) {
    const auto msg = std::format(fmt, std::forward<Args>(args)...);
    std::println(stderr,
                 "Panic: {}.\n"
                 "At {}:{}.\n"
                 "Stacktrace:\n"
                 "{}",
                 msg, loc.file_name(), loc.line(), std::stacktrace::current());
    std::exit(1);
}

#define panic(fmt, ...)                                                        \
    panic_(fmt, std::source_location::current(), ##__VA_ARGS__);

#endif