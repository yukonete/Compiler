#include <string_view>
#include "base/utf8.h"
#include "utf8proc/utf8proc.h"

DecodeRuneResult decode_rune(std::string_view str) {
    auto result = Rune{};
    auto size = utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t*>(str.data()), str.size(), &result);
    if (size < 0) {
        return DecodeRuneResult{RUNE_INVALID, 1};
    }
    return DecodeRuneResult{result, static_cast<u32>(size)};
}

DecodeRuneResult rune_at_pos(std::string_view str, usize pos) {
    auto result = DecodeRuneResult{};
    for (usize i = 0; i < pos + 1; ++i) {
        result = decode_rune(str);
        if (result.size == 0) {
            break;
        }
        str.remove_prefix(result.size);
    }
    return result;
}

u32 rune_size(Rune rune) {
    if (rune < 0) {
        return 0;
    }   
    if (rune >= 0x00000000 && rune <= 0x0000007F) {
        return 1;
    }
    if (rune >= 0x00000080 && rune <= 0x000007FF) {
        return 2;
    }
    if (rune >= 0x00000800 && rune <= 0x0000FFFF) {
        return 3;
    }
    if (rune >= 0x00010000 && rune <= 0x0010FFFF) {
        return 4;
    }
    return 1;
}

bool is_letter(Rune rune) {
    auto category = utf8proc_category(rune);
    return category >= UTF8PROC_CATEGORY_LU && category <= UTF8PROC_CATEGORY_LO;
}

bool is_digit(Rune rune) {
    return rune >= '0' && rune <= '9';
}