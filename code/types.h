#ifndef TYPER_TYPES_H
#define TYPER_TYPES_H

#include <string_view>
#include <span>

#include "base/types.h"
#include "base/down_cast.h"
#include "base/flags.h"
#include "ast.h"

namespace Typing {

struct NamedTypeEntity;
struct Scope;

struct Type {
    enum class Kind : u8 {
        BAD,
        INT,
        BOOL,
        FLOAT,
        STRING,
        POINTER,
        ARRAY,
        STRUCT,
        PROCEDURE,
        NAMED,
    };

    enum class Flags : u8 {
        NONE,
        SIZED,  
    };

    DEFINE_DOWNCAST_FUNCTIONS_FOR(Type, kind, KIND);

    constexpr Type(Kind kind) : kind{kind} {
    }

    Kind kind;
    Flags flags = Flags::NONE;

    u64 size = 0;
    u64 align = 0;
};

DEFINE_ENUM_FLAG_OPERATORS(Type::Flags);

struct BadType : public Type {
    static constexpr auto KIND = Kind::BAD;

    constexpr BadType() : Type{KIND} {
    }
};

struct IntType : public Type {
    static constexpr auto KIND = Kind::INT;

    constexpr IntType(u64 size_in, u64 align_in) : Type{KIND} {
        size = size_in;
        align = align_in;
    }
};

struct BoolType : public Type {
    static constexpr auto KIND = Kind::BOOL;

    constexpr BoolType() : Type{KIND} {
        size = 1;
        align = 1;
    }
};

struct FloatType : public Type {
    static constexpr auto KIND = Kind::FLOAT;

    constexpr FloatType(u64 size_in, u64 align_in) : Type{KIND} {
        size = size_in;
        align = align_in;
    }
};

struct StringType : public Type {
    static constexpr auto KIND = Kind::STRING;

    constexpr StringType() : Type{KIND} {
    } 
};

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

    constexpr ArrayType(Type *type, u64 count,
                        Ast::Expression *count_expression, Scope *scope)
        : Type{KIND}, type{type}, count{count}, count_expression{count_expression}, scope{scope} {
    }

    Type *type;
    u64 count;
    Ast::Expression *count_expression;
    Scope *scope;
};

struct StructMember {
    Ast::Field *field = nullptr;

    std::string_view name;
    Type *type = nullptr;
};

struct StructType : public Type {
    static constexpr auto KIND = Kind::STRUCT;

    constexpr StructType(std::span<StructMember> members,  Scope *inner_scope) 
        : Type{KIND}, members{members}, inner_scope{inner_scope} {
    }

    std::span<StructMember> members;
    Scope *inner_scope;
};

struct ProcedureParameter {
    Ast::Field *field = nullptr;
    
    std::string_view name;
    Type *type = nullptr;   
};

struct ProcedureType : public Type {
    static constexpr auto KIND = Kind::PROCEDURE;

    constexpr ProcedureType(std::span<ProcedureParameter> parameters, std::optional<Type *> return_type) 
        : Type{KIND}, parameters{parameters}, return_type{return_type} {
        flags = Flags::SIZED;
    }

    std::span<ProcedureParameter> parameters;
    std::optional<Type *> return_type = nullptr;
};

struct NamedType : public Type {
    static constexpr auto KIND = Kind::NAMED;

    constexpr NamedType(std::string_view name,
                        NamedTypeEntity *entity)
        : Type{KIND}, name{name}, entity{entity} {
    }

    std::string_view name;
    NamedTypeEntity *entity;

    Type *type = nullptr;
};

constexpr bool is_integer(const Type *type) {
    switch (type->kind) {
        using enum Type::Kind;

        case BAD: 
        case BOOL:
        case FLOAT:
        case STRING:
        case POINTER:
        case ARRAY:
        case STRUCT:
        case PROCEDURE: return false;

        case INT: return true;
        case NAMED: {
            auto named_type = type->as<NamedType>();
            return is_integer(named_type->type);
        }
    }
    assert(false && "Should not trigger");
    return false;
}

}

#endif