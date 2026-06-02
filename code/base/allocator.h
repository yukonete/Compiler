#ifndef ALLOCATOR_H_
#define ALLOCATOR_H_

#include <span>
#include <utility>
#include <cassert>

#include "base/types.h"

constexpr auto DEFAULT_ALIGNMENT = 16;

constexpr bool is_power_of_two(u64 value) {
    return (value & (value - 1)) == 0;
}

constexpr usize round(usize value, usize round) {
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

    template<typename T, typename ...Args>
    T* create(Args &&...args) {
        auto pointer = allocate<T>(1);
        if (pointer != nullptr) {
            new (pointer) T{std::forward<Args>(args)...};
        }
        return pointer;
    }

    template<typename T>
    void destroy(T *object) {
        if (object != nullptr) {
            object->~T();
            deallocate(object, 1);
        }
    }
};

struct Allocator : public AllocatorInterface<Allocator> {
    enum class Mode {
        ALLOC,
        FREE,
        FREE_ALL,
    };

    using Func = void*(void *data, Mode mode, usize size, void *old_memory);

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

    constexpr Allocator(void *data, Func *func) : func_{func}, data_{data} {}

    void *alloc(usize size) const {
        return func_(data_, Mode::ALLOC, size, nullptr);
    }

    void free(void *pointer, usize size) const {
        func_(data_, Mode::FREE, size, pointer);
    }

    void free_all() const {
        func_(data_, Mode::FREE_ALL, 0, nullptr);
    }

private:
    Func *func_;
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
                return new u8[size]{};
            }

            case FREE: {
                delete[] static_cast<u8 *>(old_memory);
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

#endif