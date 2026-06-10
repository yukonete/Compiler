#ifndef CONCEPTS_H_
#define CONCEPTS_H_

#include <type_traits>

template<typename T>
concept TriviallyDestructible = std::is_trivially_destructible_v<T>; 

#endif