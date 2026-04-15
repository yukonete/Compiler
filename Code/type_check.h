#pragma once

#include <string_view>
#include <unordered_map>

#include "base.h"
#include "parser.h"
#include "types.h"

namespace TypeCheck {
struct Scope {
    Scope *parent = nullptr;
    std::unordered_map<std::string_view, Types::Type*> declarations;
};

struct TypeChecker {
    Arena *arena = nullptr;
    Scope global_scope;

    void do_type_check(Ast::Program *program);
    Types::Type *create_type_from_ast_type(Ast::Type *type);
};


} // namespace TypeCheck