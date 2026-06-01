#pragma once

#include <stack>
#include <string_view>
#include <unordered_map>
#include <optional>

#include "base/arena.h"
#include "ast.h"
#include "types.h"
#include "log.h"

namespace TypeCheck {

struct Scope {
    Scope *parent = nullptr;
    std::unordered_map<std::string_view, Type *> declarations;

    std::optional<Type *> lookup_type(std::string_view type_name);
};

class TypeChecker {
public:
    TypeChecker(DynamicArena *arena, FILE *log = stderr) : log_{log}, arena_{arena} {
    }
    bool do_type_check(Ast::Program *program);
    Scope global_scope;

private:
    Type *create_type_from_ast_type(Scope *scope, Ast::Type *type);
    Type *resolve_type(Scope *scope, Type *type, bool resolve_non_anonymous_types = false);
    bool add_type_declarations_to_scope(std::span<Ast::Statement *> statements, Scope *scope);

    template <typename... Args>
    void report_error(const FileLocation &location, std::format_string<Args...> fmt,
                        Args &&...args) {
        error_count_ += 1;
        log_diagnostics(log_, DiagnosticsLevel::Error, location, fmt, std::forward<Args>(args)...);
    }

    u64 error_count_ = 0;
    FILE *log_ = nullptr;
    DynamicArena *arena_ = nullptr;
};

std::string type_to_string(const Type *type, bool declaration = false);

} // namespace TypeCheck