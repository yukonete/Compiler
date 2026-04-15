#pragma once

#include <array>
#include <optional>
#include <unordered_map>
#include <vector>

#include "base.h"
#include "parser.h"

namespace Types {

enum class TypeKind {
    invalid,
    placeholder,

    boolean,
    integer,

    record,
    pointer
};

struct Type {
    TypeKind kind = TypeKind::invalid;
    Ast::Type *ast_type = nullptr;
    std::string_view type_name;
    s64 size = 0;
    s64 align = 0;
};

struct Pointer : public Type {
    Pointer() {
        kind = TypeKind::pointer;
        size = 8;
        align = 8;
    }
    Type *points_to = nullptr;
};

struct RecordMember {
    std::string_view name;
    Type *type;
};

struct Record : public Type {
    Record() {
        kind = TypeKind::record;
    }
    std::span<RecordMember> members;
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

} // namespace Types
