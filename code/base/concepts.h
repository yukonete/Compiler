#ifndef BASE_CONCEPTS_H
#define BASE_CONCEPTS_H

#include <type_traits>

template<typename T>
concept TriviallyDestructible = std::is_trivially_destructible_v<T>; 

template<typename T>
concept TriviallyCopyable = std::is_trivially_copy_constructible_v<T>;

#endif