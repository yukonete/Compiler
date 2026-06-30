#ifndef BASE_ARENA_H
#define BASE_ARENA_H

#include <memory>
#include <print>
#include <span>
#include <utility>
#include <memory_resource>
#include <vector>

#include "base/types.h"
#include "base/allocator.h"

// Any Arena supports freeing only last allocation,
// but even if it is not last allocation the memory
// still will be poisoned
// DynamicArena can free last allocation of each MemoryBlock

struct Arena : public AllocatorInterface<Arena> {
    Arena(std::span<u8> data);
    Arena(u8 *data, usize size) : Arena{std::span{data, size}} {}
    Arena(const Arena&) = delete;
    Arena(const Arena&&) = delete;
    ~Arena();

    bool owns(void *pointer, usize size) const;
    
    usize size() const {
        return data_.size();
    }

    usize offset() const {
        return offset_;
    }

    void *alloc(usize size);
    void free(void *pointer, usize size);
    void free_all();

private:
    std::span<u8> data_;
    usize offset_ = 0;
};

template<usize Size>
struct InlineArena : public Arena {
    constexpr InlineArena() : Arena{data_, Size} {}
private:
    alignas(16) u8 data_[Size];
};

struct DynamicArena : public AllocatorInterface<DynamicArena> {
    constexpr static auto DEFAULT_BLOCK_SIZE = megabytes(2);

    constexpr DynamicArena(const std::pmr::polymorphic_allocator<u8>
                               &allocator = std::pmr::get_default_resource(),
                           usize block_size = DEFAULT_BLOCK_SIZE)
        : allocator_{allocator}, block_size_{block_size} {
    }
    constexpr DynamicArena(const DynamicArena&) = delete;
    constexpr DynamicArena(DynamicArena &&other) 
        : allocator_{std::move(other.allocator_)}
        , block_size_{other.block_size_}
        , first_{std::exchange(other.first_, nullptr)}
        , last_{std::exchange(other.last_, nullptr)}
        , current_{std::exchange(other.current_, nullptr)} {
    }
    constexpr DynamicArena& operator=(DynamicArena &&other) {
        return *new (drop()) DynamicArena{std::move(other)};
    }
    constexpr ~DynamicArena() {
        drop();
    }

    DynamicArena* drop();

    void *alloc(usize size);
    void free(void *pointer, usize size);
    void free_all();

private:
    bool add_block(usize size);

    struct MemoryBlock {
        Arena arena;
        MemoryBlock *next = nullptr;
    };

    static constexpr auto MEMORY_BLOCK_HEADER_SIZE = align_forward(sizeof(MemoryBlock), DEFAULT_ALIGNMENT);

    std::pmr::polymorphic_allocator<u8> allocator_;
    usize block_size_;

    MemoryBlock *first_ = nullptr;
    MemoryBlock *last_ = nullptr;
    MemoryBlock *current_ = nullptr;
};

template<usize Size>
struct Scratch : public AllocatorInterface<Scratch<Size>> {
    constexpr Scratch(const std::pmr::polymorphic_allocator<u8> &backing) : backing_{backing} {}

    void *alloc(usize size) {
        auto pointer = inline_.alloc(size);
        if (pointer != nullptr) {
            return pointer;
        }  
        return backing_.allocate_bytes(size);
    }

    void free(void *pointer, usize size) {
        if (inline_.owns(pointer, size)) {
            inline_.free(pointer, size);
            return;
        }
        backing_.deallocate_bytes(pointer, size);
    }

    void free_all() {
        inline_.free_all();
    }

private:
    InlineArena<Size> inline_;
    std::pmr::polymorphic_allocator<u8> backing_;
};

template <typename T>
using DynamicArenaAllocator = Allocator<T, DynamicArena>;

template <typename T>
using DynamicArenaVector = std::vector<T, DynamicArenaAllocator<T>>;

thread_local inline DynamicArena temp_dynamic_arena = DynamicArena{};

template <typename T>
DynamicArenaAllocator<T> temp_allocator() {
    return DynamicArenaAllocator<T>{temp_dynamic_arena};
}

template <typename T>
DynamicArenaVector<T> make_temp_vector(usize capacity = 0) {
    auto result = DynamicArenaVector<T>(temp_allocator<T>());
    if (capacity != 0) {
        result.reserve(capacity);
    }
    return result;
}

using DynamicArenaString = std::basic_string<char, std::char_traits<char>, DynamicArenaAllocator<char>>;

inline DynamicArenaString make_temp_string(usize capacity = 0) {
    auto result = DynamicArenaString(temp_allocator<char>());
    if (capacity != 0) {
        result.reserve(capacity);
    }
    return result;
}

#endif