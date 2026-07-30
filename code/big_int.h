#ifndef BIG_INT_H
#define BIG_INT_H

#include <string>
#include <string_view>
#include <limits>

#include "base/types.h"
#include "libtommath/tommath.h"

enum Ordering : int {
    ORDER_LESS = -1,
    ORDER_EQUAL = 0,
    ORDER_GREATER = 1,  
};

// All big ints data is stored in one separate arena and is never deleted
using BigInt = mp_int;

void big_int_init(BigInt *value);
BigInt big_int_create();

BigInt big_int_create_from_f64(f64 value);
BigInt big_int_create_from_s64(s64 value);
BigInt big_int_create_from_u64(u64 value);

BigInt big_int_create_min_s64();
BigInt big_int_create_max_s64();
BigInt big_int_create_max_u64();

inline BigInt BIG_INT_MIN_S64 = big_int_create_min_s64();
inline BigInt BIG_INT_MAX_S64 = big_int_create_max_s64();
inline BigInt BIG_INT_MAX_U64 = big_int_create_max_u64();

void big_int_add(BigInt *destination, const BigInt &source1, const BigInt &source2);
void big_int_sub(BigInt *destination, const BigInt &source1, const BigInt &source2);
void big_int_mul(BigInt *destination, const BigInt &source1, const BigInt &source2);
void big_int_div(BigInt *quotient, BigInt *remainder, const BigInt &source1, const BigInt &source2);

void big_int_mul_d(BigInt *destination, const BigInt &source1, u32 digit);
void big_int_add_d(BigInt *destination, const BigInt &source1, u32 digit);

void big_int_negate(BigInt *destination, const BigInt &source);

void big_int_copy(BigInt *destination, const BigInt &source);

f64 big_int_to_f64(const BigInt &source);
u64 big_int_to_u64(const BigInt &source);

Ordering big_int_cmp(const BigInt &left, const BigInt &right);
bool big_int_less(const BigInt &left, const BigInt &right);
bool big_int_less_equal(const BigInt &left, const BigInt &right);
bool big_int_greater(const BigInt &left, const BigInt &right);
bool big_int_greater_equal(const BigInt &left, const BigInt &right);

int big_int_sign(const BigInt &value);
bool big_int_is_negative(const BigInt &value);

bool big_int_is_zero(const BigInt &value);

BigInt big_int_create_from_string(std::string_view value);
std::string big_int_to_string(const BigInt &value);

#endif