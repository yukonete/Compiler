#include <cassert>
#include <cstring>
#include <utility>

#include "libtommath/tommath.h"
#include "big_int.h"
#include "base/arena.h"

static DynamicArena big_ints_arena = {NEW_ALLOCATOR};

extern void *MP_MALLOC(size_t size) {
    return big_ints_arena.alloc(size);
}

extern void *MP_REALLOC(void *mem, size_t oldsize, size_t newsize) {
    if (oldsize >= newsize) {
        return mem;
    }
    auto new_mem = big_ints_arena.alloc(newsize);
    std::memcpy(new_mem, mem, oldsize);
    return new_mem;
}

extern void *MP_CALLOC(size_t nmemb, size_t size) {
    return big_ints_arena.alloc(nmemb * size);
}

extern void MP_FREE(void *mem, size_t size) {
    big_ints_arena.free(mem, size);
}

void big_int_init(BigInt *value) {
    mp_init(value);
}

BigInt big_int_create() {
    auto result = BigInt{};
    mp_init(&result);
    return result;
}

BigInt big_int_create_from_f64(f64 value) {
    auto result = BigInt{};
    mp_init(&result);
    mp_set_double(&result, value);
    return result;
}

BigInt big_int_create_from_s64(s64 value) {
    auto result = BigInt{};
    mp_init_i64(&result, value);
    return result;
}

BigInt big_int_create_from_u64(u64 value) {
    auto result = BigInt{};
    mp_init_u64(&result, value);
    return result;
}

BigInt big_int_create_min_s64() {
    return big_int_create_from_s64(std::numeric_limits<s64>::min());
}

BigInt big_int_create_max_s64() {
    return big_int_create_from_s64(std::numeric_limits<s64>::max());
}

BigInt big_int_create_max_u64() {
    return big_int_create_from_u64(std::numeric_limits<u64>::max());
}

void big_int_add(BigInt *destination, const BigInt &source1, const BigInt &source2) {
    mp_add(&source1, &source2, destination);
}

void big_int_sub(BigInt *destination, const BigInt &source1, const BigInt &source2) {
    mp_sub(&source1, &source2, destination);
}

void big_int_mul(BigInt *destination, const BigInt &source1, const BigInt &source2) {
    mp_mul(&source1, &source2, destination);
}

void big_int_div(BigInt *quotient, BigInt *remainder, const BigInt &source1, const BigInt &source2) {
    mp_div(&source1, &source2, quotient, remainder);
}

void big_int_negate(BigInt *destination, const BigInt &source) {
    mp_neg(&source, destination);
}

void big_int_copy(BigInt *destination, const BigInt &source) {
    mp_copy(&source, destination);
}

f64 big_int_to_f64(const BigInt &source) {
    return mp_get_double(&source);
}

u64 big_int_to_u64(const BigInt &source) {
    return static_cast<u64>(mp_get_i64(&source));
}

Ordering big_int_cmp(const BigInt &left, const BigInt &right) {
    return static_cast<Ordering>(mp_cmp(&left, &right));
}

bool big_int_less(const BigInt &left, const BigInt &right) {
    auto order = big_int_cmp(left, right);
    return order == -1;
}

bool big_int_less_equal(const BigInt &left, const BigInt &right) {
    auto order = big_int_cmp(left, right);
    return order == -1 || order == 0;
}

bool big_int_greater(const BigInt &left, const BigInt &right) {
    auto order = big_int_cmp(left, right);
    return order == -1;
}

bool big_int_greater_equal(const BigInt &left, const BigInt &right) {
    auto order = big_int_cmp(left, right);
    return order == 1 || order == 0;
}

bool big_int_equal(const BigInt &left, const BigInt &right) {
    auto order = big_int_cmp(left, right);
    return order == 0;
}

int big_int_sign(const BigInt &value) {
    if (value.sign == 0) {
        return 1;
    }
    return -1;
}

bool big_int_is_negative(const BigInt &value) {
    return big_int_sign(value) == -1;
}

bool big_int_is_zero(const BigInt &value) {
    return value.used == 0;
}

std::string big_int_to_string(const BigInt &value) {
    return std::string{"i dont know how to do this now"};
}

void big_int_mul_d(BigInt *destination, const BigInt &source1, u32 digit) {
    mp_mul_d(&source1, digit, destination);
}

void big_int_add_d(BigInt *destination, const BigInt &source1, u32 digit) {
    mp_add_d(&source1, digit, destination);
}

BigInt big_int_create_from_string(std::string_view str) {
    auto result = big_int_create();

    bool negate = false;
    if (str.starts_with('-')) {
        negate = true;
        str.remove_prefix(1);
    }

    for (auto c : str) {
        assert(c >= '0' && c <= '9');
        auto digit = static_cast<u32>(c - '0');
        big_int_mul_d(&result, result, 10);
        big_int_add_d(&result, result, digit);
    }

    if (negate) {
        big_int_negate(&result, result);
    }

    return result;
}
