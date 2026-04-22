#pragma once

#include <string_view>
#include <unordered_map>
#include <stack>

#include "base.h"
#include "parser.h"
#include "types.h"

namespace TypeCheck {
struct Scope {
    Scope *parent = nullptr;
    std::unordered_map<std::string_view, Type *> declarations;
};

class TypeChecker {
public:
    TypeChecker(Arena *arena) : arena{arena} {
    }
    void do_type_check(Ast::Program *program);
    Scope global_scope;

private:
    Type *create_type_from_ast_type(Ast::Type *type);
    Type *lookup_type(std::string_view type_name);
    Type *resolve_type(Type *type, bool resolve_non_anonymous_types = false);
    void check_for_recursing_structs(Scope *scope);
    bool walk_type(std::vector<Type*> *met_types, Type *type);

    Arena *arena = nullptr;
};

std::string type_to_string(const Type *type, bool declaration = false);

} // namespace TypeCheck