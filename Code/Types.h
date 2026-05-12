#pragma once

#include <array>
#include <optional>
#include <unordered_map>
#include <vector>

#include "base.h"
#include "parser.h"

namespace TypeCheck {

enum class TypeKind {
    invalid,
    placeholder,
    kind_void,

    integer,
    boolean,

    alias,
    kind_struct,
    pointer,
    array
};

// TODO: Remove ast_type because it is not needed, i think (at least in a base
// type)
struct Type {
    TypeKind kind = TypeKind::invalid;
    Ast::TypeDeclaration *ast_declaration = nullptr;
    Ast::Type *ast_type = nullptr;
    std::string_view type_name;
    s64 size = -1;
    s64 align = -1;

    Type(){}
    Type(TypeKind kind, std::string_view type_name, s64 size, s64 align) :
        kind{kind}, type_name{type_name}, size{size}, align{align} {}

#ifdef _DEBUG
    virtual void exists_only_to_display_actuall_types_in_the_debuger() {}
#endif
};

struct Alias : public Type {
    Alias() : Type() {
        kind = TypeKind::alias;
    }
    Type *alias_to = nullptr;
};

struct Pointer : public Type {
    Pointer() : Type(){
        kind = TypeKind::pointer;
        size = 8;
        align = 8;
    }
    Type *points_to = nullptr;
};

struct StructMember {
    std::string_view name;
    Type *type = nullptr;
};

struct Struct : public Type {
    Struct() : Type(){
        kind = TypeKind::kind_struct;
    }
    std::span<StructMember> members;
};

struct Array : public Type {
    Array() : Type(){
        kind = TypeKind::array;
    }
    Type *elem_type = nullptr;
    s64 count = 0;
};

inline std::optional<Type *> check_builtin_type(std::string_view type_name) {
    static std::unordered_map<std::string_view, Type> builtin_types = {
        {"void", Type{TypeKind::kind_void,
                      "void",
                      0,
                      0}},
        {"int", Type{TypeKind::integer,
                     "int",
                     8,
                     8}},
        // {"s64", Type{.kind = TypeKind::integer,
        //              .type_name = "s64",
        //              .size = 8,
        //              .align = 8}},
        // {"s32", Type{.kind = TypeKind::integer,
        //              .type_name = "s32",
        //              .size = 4,
        //              .align = 4}},
        // {"s16", Type{.kind = TypeKind::integer,
        //              .type_name = "s16",
        //              .size = 2,
        //              .align = 2}},
        // {"byte", Type{.kind = TypeKind::integer,
        //               .type_name = "s16",
        //               .size = 1,
        //               .align = 1}},
        {"bool", Type{TypeKind::boolean,
                      "bool",
                      1,
                      1}},
    };
    if (builtin_types.contains(type_name)) {
        return {&builtin_types.at(type_name)};
    }
    return {};
}

} // namespace TypeCheck
