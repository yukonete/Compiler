#ifndef BASE_ALLOCATOR_H
#define BASE_ALLOCATOR_H

#include <span>
#include <utility>
#include <cassert>
#include <memory>
#include <concepts>
#include <memory_resource>

#include "base/types.h"
#include "base/util.h"

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

    std::span<u8> allocate_bytes(usize n) {
        auto pointer = allocate<u8>(n);
        if (pointer != nullptr) {
            return std::span{pointer, n};
        }
        return {};
    }

    void deallocate_bytes(std::span<u8> bytes) {
        deallocate(bytes.data(), bytes.size());
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

template <typename A, typename T>
concept AllocatorOfType = std::same_as<u8, typename std::allocator_traits<A>::value_type>;

template <typename A, typename ...Args>
auto *new_object(A &allocator, Args &&...args) {
    auto pointer = std::allocator_traits<A>::allocate(allocator, 1);
    std::allocator_traits<A>::construct(allocator, pointer, std::forward<Args>(args)...);
    return pointer;
}

template <typename A, typename T>
void delete_object(A &allocator, T *pointer) {
    std::allocator_traits<A>::destroy(allocator, pointer);
    std::allocator_traits<A>::deallocate(allocator, pointer, 1);
}

template <typename T, typename A>
    requires std::derived_from<A, AllocatorInterface<A>>
struct Allocator {
    static_assert(
        alignof(T) <= DEFAULT_ALIGNMENT,
        "Alignment bigger than DEFAULT_ALIGNMENT is not supported for now");
    using value_type = T;

    A *allocator;

    constexpr Allocator(A &allocator) noexcept : allocator{&allocator} {
    }

    [[nodiscard]] T *allocate(std::size_t n) {
        return allocator->template allocate<T>(n);
    }

    void deallocate(T *p, std::size_t n) noexcept {
        return allocator->template deallocate<T>(p, n);
    }

    constexpr friend bool operator==(const Allocator &a,
                                     const Allocator &b) noexcept = default;
    constexpr friend bool operator!=(const Allocator &a,
                                     const Allocator &b) noexcept {
        return !(a == b);
    }

    using is_always_equal = std::false_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_swap = std::true_type;
    using propagate_on_container_copy_assignment = std::false_type;
};

template <typename T, typename A>
struct AllocatorDeleter {
    A allocator;

    constexpr AllocatorDeleter(A allocator) : allocator{allocator} {
    }

    void operator()(T *pointer) {
        delete_object(allocator, pointer);
    }
};

template<typename T, typename A>
using AllocatorUniquePtr = std::unique_ptr<T, AllocatorDeleter<T, A>>;

template <typename T, typename A>
AllocatorUniquePtr<T, A> create_unique_ptr_with_allocator(const A &allocator,
                                                          T *pointer)
    requires(!std::derived_from<A, AllocatorInterface<A>>)
{
    return AllocatorUniquePtr{pointer, AllocatorDeleter<T, A>{allocator}};
}

template <typename T, typename A>
AllocatorUniquePtr<T, Allocator<T, A>> create_unique_ptr_with_allocator(A &allocator_like,
                                                          T *pointer)
    requires std::derived_from<A, AllocatorInterface<A>>
{
    return create_unique_ptr_with_allocator(Allocator<T, A>{allocator_like}, pointer);
}

template <typename T, typename A, typename... Args>
AllocatorUniquePtr<T, A> make_unique_with_allocator(A allocator, Args &&...args)
    requires (!std::derived_from<A, AllocatorInterface<A>>)
{
    auto object = new_object(allocator, std::forward<Args>(args)...);
    return create_unique_ptr_with_allocator(allocator, object);
}

template <typename T, typename A, typename... Args>
AllocatorUniquePtr<T, Allocator<T, A>> make_unique_with_allocator(A &allocator_like,
                                                    Args &&...args)
    requires std::derived_from<A, AllocatorInterface<A>>
{
    return make_unique_with_allocator<T>(Allocator<T, A>{allocator_like}, std::forward<Args>(args)...);
}

template <typename A>
    requires std::derived_from<A, AllocatorInterface<A>>
struct MemoryResource : public std::pmr::memory_resource {
    A *allocator;

    constexpr MemoryResource(A &allocator) noexcept : allocator{&allocator} {  
    }

private:
    void *do_allocate(std::size_t bytes, std::size_t alignment) override {
        assert(alignment <= DEFAULT_ALIGNMENT);
        return allocator->template allocate<u8>(bytes);
    }

    void do_deallocate(void *p, std::size_t bytes, std::size_t alignment) noexcept override {
        allocator->template deallocate<u8>(p, bytes);
    }
};

#endif