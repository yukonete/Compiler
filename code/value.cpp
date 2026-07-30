#include "value.h"
#include "base/panic.h"

Value Value::as_int() const {
    switch (kind) {
        using enum Value::Kind;

        case INVALID:
        case INTEGER: return *this;

        default: panic("Value is not INVALID or INTEGER");
    }
}

Value Value::as_int_or_invalid() const {
    if (kind == Kind::INTEGER) {
        return *this;
    }
    return Value{};
}

Value Value::try_convert_to_int() const {
    switch (kind) {
        using enum Value::Kind;

        case INTEGER: return *this;
        case FLOAT: {
            auto big_int_value = big_int_create_from_f64(float_value);
            auto converted_back = big_int_to_f64(big_int_value);
            if (float_value == converted_back) {
                return create_value_int(big_int_value);
            }
        }
        default: return Value{};
    }
}

Value Value::as_float() const {
    switch (kind) {
        using enum Value::Kind;

        case INVALID:
        case FLOAT: {
            return *this;
        }

        default: panic("Value is not INVALID or FLOAT");
    }
}

Value Value::as_float_or_invalid() const {
    if (kind == Kind::FLOAT) {
        return *this;
    }
    return Value{};
}

Value Value::try_convert_to_float() const {
    switch (kind) {
        using enum Value::Kind;

        case FLOAT: return *this;
        // NOTE: Technically, int_value might not be representable by f64, but it is not handled for now
        case INTEGER: return create_value_float(big_int_to_f64(int_value));

        default: return Value{};
    }
}

Value Value::as_compound() const {
    switch (kind) {
        using enum Value::Kind;

        case INVALID:
        case COMPOUND: {
            return *this;
        }

        default: panic("Value is not INVALID or COMPOUND");
    }
}

Value Value::as_compound_or_invalid() const {
    if (kind == Kind::COMPOUND) {
        return *this;
    }
    return Value{};
}
