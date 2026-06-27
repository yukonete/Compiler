#ifndef ARENA_H_
#define ARENA_H_

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

    Allocator create_allocator() {
        return Allocator{this, Allocator::default_allocator_func<Arena>};
    }
    
    struct TempMemory {
        TempMemory(Arena &arena) 
            : arena_{arena}, previous_offset{arena.offset()} {}
        TempMemory(const TempMemory &other) = delete;
        TempMemory(TempMemory &&other) = delete;

        ~TempMemory();
    private:
        Arena &arena_;
        usize previous_offset;
    };

    friend TempMemory;

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

    constexpr DynamicArena(Allocator allocator, 
                           usize block_size = DEFAULT_BLOCK_SIZE) 
    :  allocator_{allocator}, block_size_{block_size} {
    }
    constexpr DynamicArena(const DynamicArena&) = delete;
    constexpr DynamicArena(DynamicArena &&other) 
        : allocator_{other.allocator_}
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

    Allocator create_allocator() {
        return Allocator{this, Allocator::default_allocator_func<DynamicArena>};
    }

private:
    bool add_block(usize size);

    struct MemoryBlock {
        Arena arena;
        MemoryBlock *next = nullptr;
    };

    static constexpr auto MEMORY_BLOCK_HEADER_SIZE = round(sizeof(MemoryBlock), DEFAULT_ALIGNMENT);

    Allocator allocator_;
    usize block_size_;

    MemoryBlock *first_ = nullptr;
    MemoryBlock *last_ = nullptr;
    MemoryBlock *current_ = nullptr;
};

template<usize Size>
struct Scratch : public AllocatorInterface<Scratch<Size>> {
    constexpr Scratch(Allocator backing) : backing_{backing} {}

    void *alloc(usize size) {
        auto pointer = inline_.alloc(size);
        if (pointer != nullptr) {
            return pointer;
        }  
        return backing_.alloc(size);
    }

    void free(void *pointer, usize size) {
        if (inline_.owns(pointer, size)) {
            inline_.free(pointer, size);
            return;
        }
        backing_.free(pointer, size);
    }

    void free_all() {
        inline_.free_all();
        backing_.free_all();
    }

    Allocator create_allocator() {
        return Allocator{this, Allocator::default_allocator_func<Scratch>};
    }

private:
    InlineArena<Size> inline_;
    Allocator backing_;
};

// For use with std library
template<class T>
struct DynamicArenaAllocator {
    static_assert(alignof(T) <= DEFAULT_ALIGNMENT);
    using value_type = T;

    DynamicArena &arena;

    DynamicArenaAllocator(DynamicArena &arena) : arena{arena} {
    }

    template<class U>
    DynamicArenaAllocator(const DynamicArenaAllocator<U>& other) : arena{other.arena} {
    }

    [[nodiscard]] T* allocate(std::size_t n) {
        return arena.allocate<T>(n);
    }

    void deallocate(T* p, std::size_t n) noexcept {
        return arena.deallocate<T>(p, n);
    }

    friend bool operator==(const DynamicArenaAllocator& a, const DynamicArenaAllocator& b) {
        return &a.arena == &b.arena;
    }

    friend bool operator!=(const DynamicArenaAllocator& a, const DynamicArenaAllocator& b) {
        return !(a == b);
    }

    using is_always_equal = std::false_type;
    using propagate_on_container_move_assignment = std::true_type;
};

template <typename T>
using DynamicArenaVector = std::vector<T, DynamicArenaAllocator<T>>;

thread_local inline DynamicArena temp_allocator = DynamicArena{NEW_ALLOCATOR};

template <typename T>
DynamicArenaVector<T> make_temp_vector(usize capacity = 0) {
    auto result = DynamicArenaVector<T>(temp_allocator);
    if (capacity != 0) {
        result.reserve(capacity);
    }
    return result;
}

#endif