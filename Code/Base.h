#pragma once

#ifndef BASE_H
#define BASE_H

#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <print>
#include <source_location>
#include <span>
#include <stacktrace>
#include <string>
#include <type_traits>
#include <system_error>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

using f32 = float;
using f64 = double;

using isize = ptrdiff_t;
using usize = size_t;

using uintptr = uintptr_t;
using intptr = intptr_t;

template <typename T>
concept TriviallyCopyable = std::is_trivially_copyable_v<T>;

template <typename T>
concept TriviallyDestructible = std::is_trivially_destructible_v<T>;


#define kilobytes(value) ((value) * 1024LL)
#define megabytes(value) ((kilobytes(value)) * 1024LL)
#define gigabytes(value) ((megabytes(value)) * 1024LL)
#define terabytes(value) ((gigabytes(value)) * 1024LL)

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
    std::terminate();
}

#define panic(fmt, ...)                                                        \
    panic_(fmt, std::source_location::current(), ##__VA_ARGS__);

// Defer macro/thing from Jonathan Blow.
#define CONCAT_INTERNAL(x, y) x##y
#define CONCAT(x, y) CONCAT_INTERNAL(x, y)

template <typename T> struct ExitScope {
    T lambda;
    ExitScope(T lambda) : lambda(lambda) {
    }
    ~ExitScope() {
        lambda();
    }
    ExitScope(const ExitScope &) = delete;

private:
    ExitScope &operator=(const ExitScope &) = delete;
};

class ExitScopeHelp {
public:
    template <typename T> ExitScope<T> operator+(T t) {
        return t;
    }
};

#define defer const auto &CONCAT(defer__, __LINE__) = ExitScopeHelp() + [&]()

inline bool is_power_of_two(s64 value) {
    return (value & (value - 1)) == 0;
}

inline isize align_forward(isize pointer, isize alignment) {
    assert(is_power_of_two(alignment));
    isize modulo = pointer % alignment;
    if (modulo == 0) {
        return pointer;
    }
    return pointer + (alignment - modulo);
}

class Arena {
public:
    constexpr static isize default_size = megabytes(2);
    constexpr static isize allocation_default_alignment = 2 * sizeof(void *);

    Arena(isize size = default_size)
        : data_(std::make_unique<u8[]>(size)), size_{size} {};

    void *alloc(isize size, isize alignment = allocation_default_alignment) {
        assert(size >= 0 && alignment >= 0);

        if (size == 0) {
            return nullptr;
        }

        const auto base_adress = reinterpret_cast<isize>(data_.get());
        const auto current_pointer = base_adress + offset_;
        const auto mem_offset =
            align_forward(current_pointer, alignment) - base_adress;

        if (mem_offset + size > size_) {
            panic("Arena is out of memory");
        }

        offset_ = mem_offset + size;
        return data_.get() + mem_offset;
    };

    template <TriviallyDestructible Item, typename... Args>
    Item *push_item(Args &&...args) {
        auto pointer = static_cast<Item *>(alloc(sizeof(Item), alignof(Item)));
        return new (pointer) Item{static_cast<Args &&>(args)...};
    }

    // template <TriviallyCopyable Item>
    // Item *push_item(Item item) {
    //     auto pointer = static_cast<Item *>(alloc(sizeof(Item), alignof(Item)));
    //     *pointer = item;
    //     return pointer;
    // }

    template <TriviallyDestructible Item> std::span<Item> push_array(isize count) {
        return std::span<Item>{push_array_pointer<Item>(count),
                               static_cast<std::span<Item>::size_type>(count)};
    }

    template <TriviallyDestructible Item>
    std::span<Item> push_array(std::span<Item> items) {
        auto result = push_array<Item>(static_cast<isize>(items.size()));
        std::ranges::copy(items, result.begin());
        return result;
    }

    template <TriviallyDestructible Item> Item *push_array_pointer(isize count) {
        assert(count >= 0);
        auto pointer =
            static_cast<Item *>(alloc(sizeof(Item) * count, alignof(Item)));
        for (int i = 0; i < count; ++i) {
            new (&pointer[i]) Item{};
        }
        return pointer;
    }

    void clear() {
        offset_ = 0;
    }

private:
    std::unique_ptr<u8[]> data_;
    isize size_ = 0;
    isize offset_ = 0;
};

struct ReadFileToStringResult {
    std::string content;
    bool ok = false;
};

inline ReadFileToStringResult read_file_to_string(const char *path) {
    auto err = std::error_code{};
    const auto file_size = std::filesystem::file_size(path, err);
    if (err) {
        return {};
    }

    auto file = std::ifstream{path, std::ios::binary};
    if (!file.is_open()) {
        return {};
    }

    auto result = ReadFileToStringResult{.content = std::string(file_size, '\0')};
    file.read(&result.content[0], file_size);
    if (!file.fail()) {
        result.ok = true;
    }
    return result;
}

#endif
