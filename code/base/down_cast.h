#ifndef DOWN_CAST_H
#define DOWN_CAST_H

#include <concepts>

#define DEFINE_DOWNCAST_FUNCTIONS_FOR(type, base_field, derived_static_field)  \
    template <typename T>                                                      \
    constexpr bool is() const                                                  \
        requires std::derived_from<T, type>                                    \
    {                                                                          \
        return base_field == T::derived_static_field;                          \
    }                                                                          \
    template <typename T>                                                      \
    const T *as() const                                                        \
        requires std::derived_from<T, type>                                    \
    {                                                                          \
        assert(is<T>());                                                       \
        return static_cast<const T *>(this);                                   \
    }                                                                          \
    template <typename T>                                                      \
    T *as()                                                                    \
        requires std::derived_from<T, type>                                    \
    {                                                                          \
        assert(is<T>());                                                       \
        return static_cast<T *>(this);                                         \
    }

#endif