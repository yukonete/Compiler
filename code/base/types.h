#ifndef BASE_TYPES_H
#define BASE_TYPES_H

#include <cstdint>
#include <cstdarg>

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

constexpr auto kilobytes(auto value) { return value * 1024; }
constexpr auto megabytes(auto value) { return kilobytes(value) * 1024; }
constexpr auto gigabytes(auto value) { return megabytes(value) * 1024; }
constexpr auto terabytes(auto value) { return gigabytes(value) * 1024; }

#endif