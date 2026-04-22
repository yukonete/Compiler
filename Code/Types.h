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

    alias,

    boolean,
    integer,

    kind_struct,
    pointer
};

struct Type {
    TypeKind kind = TypeKind::invalid;
    Ast::Type *ast_type = nullptr;
    std::string_view type_name;
    s64 size = -1;
    s64 align = -1;
};

struct Alias : public Type {
    Alias() {
        kind = TypeKind::alias;
    }
    Type *alias_to = nullptr;
};

struct Pointer : public Type {
    Pointer() {
        kind = TypeKind::pointer;
        size = 8;
        align = 8;
    }
    Type *points_to = nullptr;
};

struct StructMember {
    std::string_view name;
    Type *type;
};

struct Struct : public Type {
    Struct() {
        kind = TypeKind::kind_struct;
    }
    std::span<StructMember> members;
};

inline std::optional<Type *> check_builtin_type(std::string_view type_name) {
    static std::unordered_map<std::string_view, Type> builtin_types = {
        {"int", Type{.kind = TypeKind::integer,
                     .type_name = "int",
                     .size = 8,
                     .align = 8}}};
    if (builtin_types.contains(type_name)) {
        return {&builtin_types.at(type_name)};
    }
    return {};
}

} // namespace TypeCheck
