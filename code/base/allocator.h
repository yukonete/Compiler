#ifndef BASE_ALLOCATOR_H
#define BASE_ALLOCATOR_H

#include <span>
#include <utility>
#include <cassert>
#include <memory>
#include <concepts>
#include <vector>
#include <string>
#include <cstring>

#include "base/types.h"
#include "base/util.h"

static_assert(std::same_as<u8, unsigned char>);

constexpr auto DEFAULT_ALIGNMENT = alignof(std::max_align_t);

constexpr bool is_power_of_two(u64 value) {
    return (value & (value - 1)) == 0;
}

constexpr u64 align_forward(u64 value, u64 round) {
    assert(round > 0);
    auto modulo = value % round;
    if (modulo == 0) {
        return value;
    }
    return value + (round - modulo);
}

template <typename A>
struct AllocatorInterface {
    template<typename T>
    T* allocate(usize n) {
        return static_cast<T*>(static_cast<A*>(this)->alloc(sizeof(T) * n));
    }

    template<typename T>
    void deallocate(T *pointer, usize n) {
        static_cast<A*>(this)->free(pointer, sizeof(T) * n);
    }

    template <typename T, typename ...Args>
    T *new_object(Args &&...args) {
        auto pointer = allocate<T>(1);
        if (pointer != nullptr) {
            new (pointer) T(std::forward<Args>(args)...);
        }
        return pointer;
    }

    template <typename T>
    void delete_object(T *pointer) {
        if (pointer != nullptr) {
            pointer->~T();
            deallocate<T>(pointer, 1);
        }
    }
};

struct Allocator : public AllocatorInterface<Allocator> {
    enum class Mode {
        ALLOC,
        FREE,
        FREE_ALL,
    };

    using Func = void*(*)(void *data, Mode mode, usize size, void *old_memory);

    template<typename T>
    static void *default_allocator_func(void *data, Mode mode, usize size, void *old_memory) {
        auto allocator_data = static_cast<T*>(data);
        switch (mode) {
            using enum Allocator::Mode;

            case ALLOC: {
                return allocator_data->alloc(size);
            }

            case FREE: {
                allocator_data->free(old_memory, size);
                return nullptr;
            }

            case FREE_ALL: {
                allocator_data->free_all();
                return nullptr;
            }
        }    
        assert(false && "Should not trigger");
        return nullptr;
    }

    constexpr Allocator(void *data, Func func) : func_{func}, data_{data} {}

    void *alloc(usize size) const {
        return func_(data_, Mode::ALLOC, size, nullptr);
    }

    void free(void *pointer, usize size) const {
        func_(data_, Mode::FREE, size, pointer);
    }

    void free_all() const {
        func_(data_, Mode::FREE_ALL, 0, nullptr);
    }

    constexpr friend bool operator==(const Allocator &a,
                                     const Allocator &b) noexcept {
        return a.func_ == b.func_ && a.data_ == b.data_;
    }

private:
    Func func_;
    void *data_;
};

constexpr inline auto NEW_ALLOCATOR = Allocator{
    nullptr,
    [](void *, Allocator::Mode mode, usize size, void *old_memory) -> void * {
        switch (mode) {
            using enum Allocator::Mode;

            case ALLOC: {
                if (size == 0) {
                    return nullptr;
                }
                auto pointer = operator new (size, std::nothrow_t{});
                std::memset(pointer, 0, size);
                return pointer;
            }

            case FREE: {
                operator delete (old_memory, size);
                return nullptr;
            }

            case FREE_ALL: {
                return nullptr;
            }
        }
        assert(false && "Should not trigger");
        return nullptr;
    }
};

template<typename T>
struct AllocatorDeleter {
    Allocator allocator;

    constexpr void operator()(T *pointer) {
        allocator.delete_object(pointer);
    }
};

// If used to store pointer to derived, then only base destructor will be called
// (unless it is virtual) and only sizeof(base) will be freed
template<typename T>
using AllocatorUniquePtr = std::unique_ptr<T, AllocatorDeleter<T>>;

template <typename T>
AllocatorUniquePtr<T>
create_unique_ptr_with_allocator(Allocator allocator, T *pointer) {
    return AllocatorUniquePtr{pointer, AllocatorDeleter<T>{allocator}};
}

template <typename T, typename... Args>
AllocatorUniquePtr<T> make_unique_with_allocator(Allocator allocator, Args &&...args) {
    auto object = allocator.new_object<T>(std::forward<Args>(args)...);
    return create_unique_ptr_with_allocator(allocator, object);
}

// For use with standart library
template <typename T>
struct StdAllocator {
    static_assert(
        alignof(T) <= DEFAULT_ALIGNMENT,
        "Alignment bigger than DEFAULT_ALIGNMENT is not supported for now");
    using value_type = T;

    Allocator allocator;

    constexpr StdAllocator(Allocator allocator) : allocator{allocator} {
    }

    template <typename V>
    constexpr StdAllocator(StdAllocator<V> allocator) noexcept
        : allocator{allocator.allocator} {
    }

    [[nodiscard]] T *allocate(std::size_t n) {
        return allocator.allocate<T>(n);
    }

    void deallocate(T *p, std::size_t n) noexcept {
        return allocator.deallocate<T>(p, n);
    }

    constexpr friend bool operator==(const StdAllocator &a,
                                     const StdAllocator &b) noexcept = default;

    using is_always_equal = std::false_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_swap = std::true_type;
    using propagate_on_container_copy_assignment = std::false_type;
};

template <typename T>
using AllocatorVector = std::vector<T, StdAllocator<T>>;

using AllocatorString =
    std::basic_string<char, std::char_traits<char>, StdAllocator<char>>;

#endif