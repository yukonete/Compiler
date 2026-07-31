#ifndef VALUE_H
#define VALUE_H

#include <string_view>

#include "base/types.h"
#include "big_int.h"

namespace Ast {
    struct CompoundExpression;  
};

struct Value {
    enum class Kind {
        INVALID,
        BOOL,
        INTEGER,
        FLOAT,
        STRING,
        COMPOUND,
        POINTER,
    };

    Value as_int() const;
    Value as_int_or_invalid() const;
    Value try_convert_to_int() const;

    Value as_float() const;
    Value as_float_or_invalid() const;
    Value try_convert_to_float() const;

    Value as_compound() const;
    Value as_compound_or_invalid() const;

    Kind kind = Kind::INVALID;
    union {
        bool bool_value;
        BigInt int_value;
        f64 float_value;
        std::string_view string_value = {};
        Ast::CompoundExpression *compound_value;
        u64 pointer_value;
    };
};

constexpr Value create_value_bool(bool value) {
    return Value{.kind = Value::Kind::BOOL, .bool_value = value};
}

constexpr Value create_value_int(BigInt value) {
    return Value{.kind = Value::Kind::INTEGER, .int_value = value};
}

constexpr Value create_value_s64(s64 value) {
    auto big_value = big_int_create_from_s64(value);
    return create_value_int(big_value);
}

constexpr Value create_value_u64(u64 value) {
    auto big_value = big_int_create_from_u64(value);
    return create_value_int(big_value);
}

constexpr Value create_value_float(f64 value) {
    return Value{.kind = Value::Kind::FLOAT, .float_value = value};
}

constexpr Value create_value_string(std::string_view value) {
    return Value{.kind = Value::Kind::STRING, .string_value = value};
}

constexpr Value create_value_compound(Ast::CompoundExpression *compound) {
    return Value{.kind = Value::Kind::COMPOUND, .compound_value = compound};
}

constexpr Value create_value_pointer(u64 pointer) {
    return Value{.kind = Value::Kind::POINTER, .pointer_value = pointer};
}

std::string value_to_string(const Value &value);

#endif