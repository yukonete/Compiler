#ifndef BASE_FLAGS_H
#define BASE_FLAGS_H

#include <type_traits>

#define DEFINE_ENUM_FLAG_OPERATORS(type)                                       \
    constexpr type operator|(type lhs, type rhs) {                             \
        return static_cast<type>(                                              \
            static_cast<std::underlying_type_t<type>>(lhs) |                   \
            static_cast<std::underlying_type_t<type>>(rhs));                   \
    }                                                                          \
                                                                               \
    constexpr type &operator|=(type &lhs, type rhs) {                          \
        lhs = lhs | rhs;                                                       \
        return lhs;                                                            \
    }                                                                          \
                                                                               \
    constexpr type operator&(type lhs, type rhs) {                             \
        return static_cast<type>(                                              \
            static_cast<std::underlying_type_t<type>>(lhs) &                   \
            static_cast<std::underlying_type_t<type>>(rhs));                   \
    }                                                                          \
                                                                               \
    constexpr type &operator&=(type &lhs, type rhs) {                          \
        lhs = lhs & rhs;                                                       \
        return lhs;                                                            \
    }                                                                          \
    constexpr bool has_flag(type value, type flag) {                           \
        return (value & flag) == flag;                                         \
    }

#endif