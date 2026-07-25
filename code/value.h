#ifndef VALUE_H
#define VALUE_H

#include <string_view>
#include "base/types.h"

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
    };

    Value as_int() const {
        switch (kind) {
            using enum Value::Kind;
            
            case INTEGER: {
                return *this;
            }
            case FLOAT: {
                // TODO: f64 might not be representable by s64
                return Value{.kind = Value::Kind::INTEGER, .int_value = static_cast<s64>(float_value)};
            }
            default: return Value{};
        }
    }

    Value as_float() const {
        switch (kind) {
            using enum Value::Kind;

            case INTEGER: {
                return Value{.kind = Value::Kind::FLOAT, .float_value = static_cast<f64>(int_value)};
            }
            case FLOAT: {
                return *this;
            }
            default: return Value{};
        }
    }

    Value as_compound() const {
        switch (kind) {
            using enum Value::Kind;

            case COMPOUND: {
                return *this;
            }
            default: return Value{};
        }
    }

    Kind kind = Kind::INVALID;
    union {
        bool bool_value;
        // TODO: Use unsized integer
        s64 int_value;
        f64 float_value;
        std::string_view string_value = {};
        Ast::CompoundExpression *compound_value;
    };
};

constexpr Value create_value_bool(bool value) {
    return Value{.kind = Value::Kind::BOOL, .bool_value = value};
}

constexpr Value create_value_int(s64 value) {
    return Value{.kind = Value::Kind::INTEGER, .int_value = value};
}

constexpr Value create_value_float(f64 value) {
    return Value{.kind = Value::Kind::FLOAT, .float_value = value};
}

constexpr Value create_value_string(std::string_view value) {
    return Value{.kind = Value::Kind::STRING, .string_value = value};
}


#endif