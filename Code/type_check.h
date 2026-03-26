#pragma once

#include <string_view>
#include <unordered_map>

#include "base.h"
#include "parser.h"

namespace TypeCheck {
struct Scope {
    Scope *parent = nullptr;
    std::unordered_map<std::string_view, void *> declarations;
};

struct TypeChecker {
    std::vector<Scope> scopes;
};

void do_type_check(Ast::Program *program);
} // namespace TypeCheck