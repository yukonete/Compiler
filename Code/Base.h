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
#include <utility>

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

template <typename T, typename... Types>
concept AnyOf = (std::same_as<T, Types> || ...);

template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

template <typename T>
concept TriviallyCopyable = std::is_trivially_copyable_v<T>;

template <typename T>
concept TriviallyDestructible = std::is_trivially_destructible_v<T>;

constexpr auto kilobytes(auto value) { return value * 1024; }
constexpr auto megabytes(auto value) { return kilobytes(value) * 1024; }
constexpr auto gigabytes(auto value) { return megabytes(value) * 1024; }
constexpr auto terabytes(auto value) { return gigabytes(value) * 1024; }

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

inline void *align_forward(void *pointer, isize alignment) {
    return reinterpret_cast<void *>(
        align_forward(reinterpret_cast<isize>(pointer), alignment));
}

constexpr isize allocation_default_alignment = 2 * sizeof(void *);

struct FixedBuffer {
    FixedBuffer() {
    }
    FixedBuffer(u8 *buffer, isize size)
        : data_(buffer), size_{size} {};

    const u8 *data() const {
        return data_;
    }

    isize size() const {
        return size_;
    }

    isize offset() const {
        return offset_;
    }

    void clear() {
        offset_ = 0;
    }

    void *alloc(isize size, isize alignment = allocation_default_alignment) {
        assert(size >= 0 && alignment >= 0);

        if (size == 0) {
            return nullptr;
        }

        const auto base_adress = reinterpret_cast<isize>(data_);
        const auto current_pointer = base_adress + offset_;
        const auto mem_offset =
            align_forward(current_pointer, alignment) - base_adress;

        if (mem_offset + size > size_) {
            return nullptr;
        }

        offset_ = mem_offset + size;
        return data_ + mem_offset;
    };

private:
    u8 *data_ = nullptr;
    isize size_ = 0;
    isize offset_ = 0;
};

class Arena {
public:
    constexpr static isize default_size = megabytes(2);

    Arena(isize size = default_size) : first(ArenaMemoryBlock::create(size)) {};

    void *alloc(isize size, isize alignment = allocation_default_alignment) {
        return first->alloc(size, alignment);
    };

    void clear() {
        if (first != nullptr) {
            first->clear();
        }
    }

    ~Arena() {
        clear();
    }

private:
    struct ArenaMemoryBlock {
        struct Deleter {
            void operator()(ArenaMemoryBlock *pointer) const {
                pointer->~ArenaMemoryBlock();
                delete[] reinterpret_cast<u8*>(pointer);
            };
        };

        static std::unique_ptr<ArenaMemoryBlock, Deleter> create(isize size) {
            static_assert(alignof(ArenaMemoryBlock) <  __STDCPP_DEFAULT_NEW_ALIGNMENT__);

            auto block_info_size_plus_alignment = align_forward(
                sizeof(ArenaMemoryBlock), allocation_default_alignment);
            auto to_allocate = block_info_size_plus_alignment + size;

            auto memory_block = new u8[to_allocate]{};
            auto block = new (memory_block) ArenaMemoryBlock;

            block->buffer = FixedBuffer{
                static_cast<u8 *>(memory_block) + block_info_size_plus_alignment,
                size};

            return std::unique_ptr<ArenaMemoryBlock, Deleter>(block, Deleter{});
        }

        FixedBuffer buffer;
        std::unique_ptr<ArenaMemoryBlock, Deleter> next = nullptr;

        void *alloc(isize size,
                    isize alignment = allocation_default_alignment) {
            if (size > buffer.size() || size == 0) {
                return nullptr;
            }

            auto allocated = buffer.alloc(size, alignment);
            if (allocated != nullptr) {
                return allocated;
            }

            if (next == nullptr) {
                next = create(buffer.size());
            }

            return next->alloc(size, alignment);
        };

        void clear() {
            if (next != nullptr) {
                next->clear();
            }
            buffer.clear();
        }
    };

    std::unique_ptr<ArenaMemoryBlock, ArenaMemoryBlock::Deleter> first;
};

template <typename Buffer> 
struct BumpAllocator {
    BumpAllocator(Buffer *buffer) : buffer{buffer} {
        
    }

    void *alloc(isize size, isize alignment = allocation_default_alignment) {
        return buffer->alloc(size, alignment);
    };

    template <typename Item, typename... Args>
    Item *push_item(Args &&...args) {
        return push_item_impl<Item>(std::forward<Args>(args)...);
    }

    template <typename Item>
    Item *push_item(Item &&item) {
        return push_item_impl<Item>(std::forward<Item>(item));
    }

    template <TriviallyDestructible Item>
    std::span<Item> push_array(isize count) {
        return std::span<Item>{push_array_pointer<Item>(count),
                               static_cast<std::span<Item>::size_type>(count)};
    }

    template <TriviallyDestructible Item>
    std::span<Item> push_array(std::span<Item> items) {
        auto result = push_array<Item>(static_cast<isize>(items.size()));
        std::ranges::copy(items, result.begin());
        return result;
    }

    template <TriviallyDestructible Item>
    Item *push_array_pointer(isize count) {
        auto pointer =
            static_cast<Item *>(alloc(sizeof(Item) * count, alignof(Item)));
        for (isize i = 0; i < count; ++i) {
            new (&pointer[i]) Item{};
        }
        return pointer;
    }

    void clear() {
        while (last_allocation_ != nullptr) {
            last_allocation_->destructor(last_allocation_->object);
            last_allocation_ = last_allocation_->previous;
        }
        if (buffer != nullptr) {
            buffer->clear();
        }
    }

    ~BumpAllocator() {
        // TODO: Maybe i should not clear and only call destructors
        clear();
    }

private:
    template <typename Item, typename... Args>
    Item *push_item_impl(Args &&...args) {
        auto pointer = alloc(sizeof(Item), alignof(Item));
        if constexpr (!TriviallyDestructible<Item>) {
            last_allocation_ = push_item(AllocationWithDestructor{
                .object = pointer,
                .destructor =
                    [](void *object) { static_cast<Item *>(object)->~Item(); },
                .previous = last_allocation_,
            });
        }
        return new (pointer) Item{std::forward<Args>(args)...};
    }

    struct AllocationWithDestructor {
        using DestructorFunc = void(void *);

        void *object = nullptr;
        DestructorFunc *destructor = nullptr;
        AllocationWithDestructor *previous = nullptr;
    };

    Buffer *buffer = nullptr;
    AllocationWithDestructor *last_allocation_ = nullptr;
};

using ArenaAllocator = BumpAllocator<Arena>;
using FixedBufferAllocator = BumpAllocator<FixedBuffer>;

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
