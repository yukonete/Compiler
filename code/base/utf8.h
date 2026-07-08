#ifndef BASE_UTF8_H
#define BASE_UTF8_H

#include <string_view>
#include "base/types.h"

using Rune = s32;
constexpr Rune RUNE_INVALID = 0x0000FFFD;
constexpr Rune RUNE_MAX = 0x0010FFFF;
constexpr Rune RUNE_EOF = -1;

struct DecodeRuneResult {
    Rune rune = {};
    u32 size = 0;
};

// returns RUNE_EOF, 0 if str is empty
// returns RUNE_INVALID, 1 if rune is invalid
DecodeRuneResult decode_rune(std::string_view str);
// pos is rune position
DecodeRuneResult rune_at_pos(std::string_view str, usize pos);
// return 1 if rune is not valid codepoint
u32 rune_size(Rune rune);

bool is_letter(Rune rune);
bool is_digit(Rune rune);

#endif