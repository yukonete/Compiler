#ifndef BASE_UTIL_H
#define BASE_UTIL_H

#include <concepts>
#include <ranges>
#include <type_traits>
#include <vector>

#include "base/types.h"

template<typename T>
struct display_type;

template <std::integral T>
[[nodiscard]] constexpr auto indices(T bound) {
    return std::views::iota(static_cast<T>(0),
                            static_cast<T>(bound));
}

template <std::integral T>
[[nodiscard]] constexpr auto
reverse_indices(T bound) {
    return indices(bound) | std::views::reverse;
}

inline constexpr auto iota = std::views::iota;

#endif