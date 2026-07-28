#ifndef TYPER_TYPES_H
#define TYPER_TYPES_H

#include <string_view>
#include <span>
#include <array>
#include <cassert>

#include "base/types.h"
#include "base/down_cast.h"
#include "base/flags.h"
#include "ast.h"

namespace Typing {

struct NamedTypeEntity;
struct Scope;
struct VariableEntity;

struct Type {
    enum class Kind : u8 {
        BASIC,
        POINTER,
        ARRAY,
        SLICE,
        STRUCT,
        PROCEDURE,
        NAMED,
    };

    enum class Flags : u8 {
        NONE = 0,
        SIZED = 1 << 0,  
    };

    DEFINE_DOWNCAST_FUNCTIONS_FOR(Type, kind, KIND);

    constexpr Type(Kind kind) : kind{kind} {
    }

    void dump(std::string &out) const;

    Type *get_base_type();
    const Type *get_base_type() const;
    Type *get_core_type();
    const Type *get_core_type() const;


    bool is_integer() const;
    bool is_unsigned() const;
    bool is_float() const;
    bool is_numeric() const;
    bool is_bool() const;
    bool is_convertible_to(const Type *type) const;
    bool is_bad() const;
    bool is_basic() const;
    bool is_string() const;
    bool is_pointer() const;
    bool is_array() const;
    bool is_slice() const;
    bool is_struct() const;
    bool is_procedure() const;
    bool is_void() const;

    Kind kind;
    Flags flags = Flags::NONE;

    u64 size = 0;
    u64 align = 0;
};

DEFINE_ENUM_FLAG_OPERATORS(Type::Flags);

struct BasicType : public Type {
    static constexpr auto KIND = Kind::BASIC;

    enum class BasicFlags {
        BAD = 0,
        BOOL = 1 << 0,
        INTEGER = 1 << 1,
        UNSIGNED = 1 << 2,
        FLOAT = 1 << 3,
        STRING = 1 << 4,
        VOID = 1 << 5,
    };

    constexpr BasicType() : Type{KIND} {
    }

    BasicFlags basic_flags = BasicFlags::BAD;
    std::string_view name;
};

DEFINE_ENUM_FLAG_OPERATORS(BasicType::BasicFlags);

struct PointerType : public Type {
    static constexpr auto KIND = Kind::POINTER;

    constexpr PointerType(Type *type) : Type{KIND}, type{type} {
        flags = Flags::SIZED;
        size = 8;
        align = 8;
    }
    
    Type *type = nullptr;
};

struct ArrayType : public Type {
    static constexpr auto KIND = Kind::ARRAY;

    constexpr ArrayType(Type *type, u64 count, Scope *scope)
        : Type{KIND}, type{type}, count{count}, scope{scope} {
    }

    Type *type;
    u64 count;
    Scope *scope;
};

struct SliceType : public Type {
    static constexpr auto KIND = Kind::SLICE;

    constexpr SliceType(Type *type) : Type{KIND}, type{type} {
        flags = Flags::SIZED;
        size = 16;
        align = 8;
    }

    Type *type;
};

struct StructType : public Type {
    static constexpr auto KIND = Kind::STRUCT;

    constexpr StructType(std::span<VariableEntity *> members,
                         Scope *inner_scope)
        : Type{KIND}, members{members}, inner_scope{inner_scope} {
    }

    std::span<VariableEntity*> members;
    Scope *inner_scope;

    Maybe<u64> index_of_field(const VariableEntity *entity) const {
        for (auto i : indices(members.size())) {
            if (members[i] == entity) {
                return i;
            }
        }
        return {};
    }
};

struct ProcedureType : public Type {
    static constexpr auto KIND = Kind::PROCEDURE;

    constexpr ProcedureType(std::span<Type *> parameters, Type *return_type)
        : Type{KIND}, parameters{parameters}, return_type{return_type} {
        flags = Flags::SIZED;
        size = 8;
        align = 8;
    }

    std::span<Type *> parameters;
    Type *return_type;
};

struct NamedType : public Type {
    static constexpr auto KIND = Kind::NAMED;

    constexpr NamedType(std::string_view name, NamedTypeEntity *entity) : Type{KIND}, name{name}, entity{entity} {
    }

    std::string_view name;
    NamedTypeEntity *entity;
    Type *type = nullptr;
};

constexpr BasicType create_basic_type(BasicType::BasicFlags flags, u64 size, std::string_view name) {
    auto basic_type = BasicType{};
    basic_type.flags = Type::Flags::SIZED;
    basic_type.basic_flags = flags;
    basic_type.size = size;
    basic_type.align = size;
    basic_type.name = name;
    return basic_type;
}

// Do not reorder
inline std::array basic_types = {
    create_basic_type(BasicType::BasicFlags::BAD, 0, "bad type"),
    create_basic_type(BasicType::BasicFlags::BOOL, 1, "bool"),

    create_basic_type(BasicType::BasicFlags::INTEGER, 1, "s8"),
    create_basic_type(BasicType::BasicFlags::INTEGER, 2, "s16"),
    create_basic_type(BasicType::BasicFlags::INTEGER, 4, "s32"),
    create_basic_type(BasicType::BasicFlags::INTEGER, 8, "s64"),
    create_basic_type(BasicType::BasicFlags::INTEGER, 8, "int"),

    create_basic_type(BasicType::BasicFlags::INTEGER | BasicType::BasicFlags::UNSIGNED, 1, "u8"),
    create_basic_type(BasicType::BasicFlags::INTEGER | BasicType::BasicFlags::UNSIGNED, 2, "u16"),
    create_basic_type(BasicType::BasicFlags::INTEGER | BasicType::BasicFlags::UNSIGNED, 4, "u32"),
    create_basic_type(BasicType::BasicFlags::INTEGER | BasicType::BasicFlags::UNSIGNED, 8, "u64"),
    create_basic_type(BasicType::BasicFlags::INTEGER | BasicType::BasicFlags::UNSIGNED, 8, "uint"),

    create_basic_type(BasicType::BasicFlags::FLOAT, 4, "f32"),
    create_basic_type(BasicType::BasicFlags::FLOAT, 8, "f64"),

    create_basic_type(BasicType::BasicFlags::STRING, 16, "string"),
    create_basic_type(BasicType::BasicFlags::VOID, 0, "void"),
};

inline  BasicType * const bad_t = &basic_types[0];
inline  BasicType * const bool_t = &basic_types[1];
inline  BasicType * const s8_t = &basic_types[2];
inline  BasicType * const s16_t = &basic_types[3];
inline  BasicType * const s32_t = &basic_types[4];
inline  BasicType * const s64_t = &basic_types[5];
inline  BasicType * const int_t = &basic_types[6];
inline  BasicType * const u8_t = &basic_types[7];
inline  BasicType * const u16_t = &basic_types[8];
inline  BasicType * const u32_t = &basic_types[9];
inline  BasicType * const u64_t = &basic_types[10];
inline  BasicType * const uint_t = &basic_types[11];
inline  BasicType * const f32_t = &basic_types[12];
inline  BasicType * const f64_t = &basic_types[13];
inline  BasicType * const string_t = &basic_types[14];
inline  BasicType * const void_t = &basic_types[15];

bool are_types_the_same(const Type *a, const Type *b);

std::string type_to_string(const Type *type);

}

#endif