#pragma once

#include <string_view>
#include <unordered_map>

#include "base.h"
#include "parser.h"

namespace TypeCheck {
struct TypeChecker {
    std::vector<Scope> scopes;
};

struct Scope {
    Scope *parent = nullptr;
    std::unordered_map<std::string_view, void *> declarations;
};

void do_type_check(Ast::Program *program);
} // namespace TypeCheck