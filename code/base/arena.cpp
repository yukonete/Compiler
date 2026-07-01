#include <cassert>
#include <span>
#include <cstring>

#include "base/types.h"
#include "base/allocator.h"
#include "base/arena.h"

#ifndef __has_feature
#define __has_feature(...) 0
#endif

#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
extern "C" void __asan_poison_memory_region(void const volatile*, decltype(sizeof 0));
extern "C" void __asan_unpoison_memory_region(void const volatile*, decltype(sizeof 0));
#define ASAN_POISON_MEMORY_REGION(addr, size) \
  __asan_poison_memory_region(reinterpret_cast<volatile const void *>(addr), (size))
#define ASAN_UNPOISON_MEMORY_REGION(addr, size) \
  __asan_unpoison_memory_region(reinterpret_cast<volatile const void *>(addr), (size))
#else
#define ASAN_POISON_MEMORY_REGION(addr, size) \
  ((void)(addr), (void)(size))
#define ASAN_UNPOISON_MEMORY_REGION(addr, size) \
  ((void)(addr), (void)(size))
#endif


Arena::Arena(std::span<u8> data) : data_{data} {
    ASAN_POISON_MEMORY_REGION(data_.data(), data_.size());
}

bool Arena::owns(void *pointer, usize size) const {
    auto base_address = reinterpret_cast<uintptr>(data_.data());
    auto end = base_address + data_.size();
    auto pointer_address = reinterpret_cast<uintptr>(pointer);
    return pointer_address >= base_address && (pointer_address + size <= end);
}

void *Arena::alloc(usize size) {
    if (size == 0) {
        return nullptr;
    }

    size = align_forward(size, DEFAULT_ALIGNMENT);
    if (offset_ + size > data_.size()) {
        return nullptr;
    }

    auto pointer = &data_[offset_];
    ASAN_UNPOISON_MEMORY_REGION(pointer, size);
    std::memset(pointer, 0, size);
    offset_ += size;
    return pointer;
};

void Arena::free(void *pointer, usize size) {
    if (pointer == nullptr) {
        return;
    }
    size = align_forward(size, DEFAULT_ALIGNMENT);
    assert(owns(pointer, size));
    ASAN_POISON_MEMORY_REGION(pointer, size);
    if (static_cast<u8*>(pointer) + size == &data_[offset_]) {
        offset_ -= size;
    }
}

void Arena::free_all() {
    offset_ = 0;
    ASAN_POISON_MEMORY_REGION(data_.data(), data_.size());
}

Arena::~Arena() {
    ASAN_UNPOISON_MEMORY_REGION(data_.data(), data_.size());
}


void *DynamicArena::alloc(usize size) {
    if (size == 0) {
        return nullptr;
    }

    size = align_forward(size, DEFAULT_ALIGNMENT);
    if (current_ == nullptr && !add_block(size)) {
        return nullptr;
    }

    auto pointer = current_->arena.alloc(size);
    if (pointer != nullptr) {
        return pointer;
    }

    // If size > block_size_ it is probably better to just allocate new block
    if (size > block_size_) {
        auto curr = current_;
        if (add_block(size)) {
            pointer = current_->arena.alloc(size);
            // If adding block with needed size did not fail allocation should
            // not fail
            assert(pointer != nullptr);
            current_ = curr;
            return pointer;
        }
    }

    // Walking list here is fine, because the only
    // case when current_ is not last is after free_all()
    // which means it is basically always going to be the next
    // block that succeeds with allocation as long as size <= block_size_.
    // And if size > block_size_ and we got here that means we could not
    // allocate new block, so the hope is that block with that size has
    // already been allocated before.
    while (current_->next != nullptr) {
        current_ = current_->next;
        pointer = current_->arena.alloc(size);
        if (pointer != nullptr) {
            return pointer;
        }
    }

    if (!add_block(size)) {
        return nullptr;
    }
    return alloc(size);
};

void DynamicArena::free(void *pointer, usize size) {
    if (pointer == nullptr) {
        return;
    }

    for (auto node = first_; node != nullptr; node = node->next) {
        if (node->arena.owns(pointer, size)) {
            node->arena.free(pointer, size);
            return;
        }
    }
    assert("pointer is not in any of the memory blocks");
}

void DynamicArena::free_all() {
    for (auto node = first_; node != nullptr; node = node->next) {
        node->arena.free_all();
    }
    current_ = first_;
}

DynamicArena* DynamicArena::drop() {
    for (auto node = first_; node != nullptr;) {
        auto next = node->next;
        auto memory_block_size = node->arena.size();
        node->~MemoryBlock();
        allocator_.free(node, MEMORY_BLOCK_HEADER_SIZE + memory_block_size);
        node = next;
    }

    first_ = nullptr;
    last_ = nullptr;
    current_ = nullptr;
    return this;
}

bool DynamicArena::add_block(usize size) {
    if (size < block_size_) {
        size = block_size_;
    }

    auto memory_block = allocator_.allocate<u8>(MEMORY_BLOCK_HEADER_SIZE + size);
    if (memory_block == nullptr) {
        return false;
    }

    auto block = new (memory_block) MemoryBlock{
        .arena = Arena{memory_block + MEMORY_BLOCK_HEADER_SIZE, size},
    };
    if (first_ == nullptr) {
        first_ = block;
        last_ = block;
        current_ = block;
        return true;
    }

    last_->next = block;
    last_ = block;
    return true;
}