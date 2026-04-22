#include "type_check.h"
#include "base.h"
#include "parser.h"
#include "types.h"

#include <stack>

namespace TypeCheck {

// Returns whether the type contains recursive types
// TODO: Name this better
bool TypeChecker::walk_type(std::vector<Type *> *met_types, Type *type) {
    if (type->kind != TypeKind::alias && type->kind != TypeKind::kind_struct) {
        return false;
    }

    auto result = false;
    met_types->push_back(type);
    if (std::find(met_types->begin(), met_types->end() - 1, type) !=
        met_types->end() - 1) {
        return true;
    }
    if (type->kind == TypeKind::alias) {
        auto alias = reinterpret_cast<Alias *>(type);
        result = walk_type(met_types, alias->alias_to);
    } else if (type->kind == TypeKind::kind_struct) {
        auto st = reinterpret_cast<Struct *>(type);
        for (const auto &member : st->members) {
            result = walk_type(met_types, member.type);
            if (result) {
                break;
            }
        }
    }
    if (!result) {
        met_types->pop_back();
    }
    return result;
}

// This will report recursive types
// Example:
// type A = struct {
//     b: B;
// };
// type B = struct {
//     c: C;
// };
// type C = struct {
//     b: B;
// };
// If we will check type A we will get self reference with path A -> B -> C -> B
// For B we will get B -> C -> B
// And for C: C -> B -> C
// Even though here we have 3 types which contain recursive type,
// we only report error for types B and C, because their paths in a tree have
// the same starting and ending types
void TypeChecker::check_for_recursing_structs(Scope *scope) {
    std::vector<Type *> met_types;
    for (const auto &[name, type] : scope->declarations) {
        if (type->kind != TypeKind::kind_struct) {
            continue;
        }
        if (walk_type(&met_types, type)) {
            if (met_types.at(0) == met_types.at(met_types.size() - 1)) {
                std::println("Found self reference in type {}.",
                             type->type_name);
            }
            met_types.clear();
        }
    }
}

Type *TypeChecker::create_type_from_ast_type(Ast::Type *ast_type) {
    switch (ast_type->type) {
        using enum Ast::NodeType;

        case type_struct: {
            auto ast_struct = reinterpret_cast<Ast::TypeStruct *>(ast_type);
            auto st = arena->push_item<Struct>();
            st->ast_type = ast_type;
            auto members_count = ast_struct->members.size();
            st->members = arena->push_array<StructMember>(members_count);
            for (int i = 0; i < members_count; ++i) {
                auto member = &st->members[i];
                auto ast_member = ast_struct->members[i];

                member->name = ast_member->identifier.identifier;
                member->type = create_type_from_ast_type(ast_member->type);
            }
            return st;
        }

        case type_identifier: {
            auto ast_type_identifier =
                reinterpret_cast<Ast::TypeIdentifier *>(ast_type);
            auto identifier = ast_type_identifier->identifier.identifier;

            auto bultin_type = check_builtin_type(identifier);
            if (bultin_type) {
                return bultin_type.value();
            }

            if (auto search = global_scope.declarations.find(identifier);
                search != global_scope.declarations.end()) {
                return search->second;
            }

            auto placeholder =
                arena->push_item<Type>(Type{.kind = TypeKind::placeholder,
                                            .ast_type = ast_type,
                                            .type_name = identifier});
            return placeholder;
        }

        case type_pointer: {
            auto ast_pointer = reinterpret_cast<Ast::TypePointer *>(ast_type);
            auto pointer = arena->push_item<Pointer>();
            pointer->ast_type = ast_type;
            pointer->points_to =
                create_type_from_ast_type(ast_pointer->points_to);
            return pointer;
        }

        default: panic("Not implemented");
    }
}

Type *TypeChecker::lookup_type(std::string_view type_name) {
    if (auto search = global_scope.declarations.find(type_name);
        search != global_scope.declarations.end()) {
        return search->second;
    } else {
        panic("Type {} is not definied.", type_name);
    }
}

Type *TypeChecker::resolve_type(Type *type, bool resolve_non_anonymous_types) {
    auto is_type_anonymous = type->type_name == "";
    if (type->kind == TypeKind::placeholder || is_type_anonymous ||
        resolve_non_anonymous_types) {
        switch (type->kind) {
            using enum TypeKind;

            case placeholder: {
                type = lookup_type(type->type_name);
                break;
            }

            case alias: {
                auto alias = reinterpret_cast<Alias *>(type);
                alias->alias_to = resolve_type(alias->alias_to);
                break;
            }

            case kind_struct: {
                auto st = reinterpret_cast<Struct *>(type);
                for (auto &member : st->members) {
                    member.type = resolve_type(member.type);
                }
                break;
            }

            case pointer: {
                auto pointer = reinterpret_cast<Pointer *>(type);
                pointer->points_to = resolve_type(pointer->points_to);
                break;
            }
        }
    }

    return type;
}

void TypeChecker::do_type_check(Ast::Program *program) {
    for (auto decl : program->declarations) {
        if (decl->type == Ast::NodeType::declaration_type) {
            auto type_decl = reinterpret_cast<Ast::TypeDeclaration *>(decl);
            auto identifier = type_decl->identifier.identifier;
            if (!global_scope.declarations.contains(identifier)) {
                auto type = create_type_from_ast_type(type_decl->declared_type);
                if (!(type_decl->declared_type->type ==
                      Ast::NodeType::type_struct)) {
                    // If it is not a struct it has to be an alias
                    // TODO: Might instead make structs aliases as well
                    auto alias = arena->push_item<Alias>();
                    alias->ast_type = type_decl->declared_type;
                    alias->alias_to = type;
                    alias->type_name = identifier;
                    global_scope.declarations[identifier] = alias;
                } else {
                    type->type_name = identifier;
                    global_scope.declarations[identifier] = type;
                }
            } else {
                panic("Declaration with identifier {} already exists.",
                      identifier);
            }
        }
    }

    for (auto &[name, type] : global_scope.declarations) {
        auto resolved_type = resolve_type(type, true);
        // There should be no placeholders at this level
        // And in that case the returned type should not change
        assert(type == resolved_type);
    }
    check_for_recursing_structs(&global_scope);
    return;
}

std::string type_to_string(const Type *type, bool declaration) {
    switch (type->kind) {
        using enum TypeKind;
        case invalid: {
            return "invalid";
        }
        case placeholder: {
            return std::format("placeholder({})", type->type_name);
        }
        case alias: {
            auto alias = reinterpret_cast<const Alias *>(type);
            if (declaration) {
                return std::format("type {} = {}", alias->type_name,
                                   type_to_string(alias->alias_to));
            } else {
                return std::format("{}({})", alias->type_name, "");
            }
        }
        case boolean: {
            return "bool";
        }
        case integer: {
            return "int";
        }
        case kind_struct: {
            auto st = reinterpret_cast<const Struct *>(type);
            if (declaration) {
                auto result =
                    std::format("type {} = struct {{\n", st->type_name);
                for (const auto &member : st->members) {
                    result += std::format("{}: {};\n", member.name,
                                          type_to_string(member.type));
                }
                result += "};";
                return result;
            } else if (st->type_name == "") {
                auto result = std::format("struct {{\n", st->type_name);
                for (const auto &member : st->members) {
                    result += std::format("{}: {};\n", member.name,
                                          type_to_string(member.type));
                }
                result += "}";
                return result;
            } else {
                return std::string(st->type_name);
            }
        }
        case pointer: {
            auto pointer = reinterpret_cast<const Pointer *>(type);
            return std::format("*{}", type_to_string(pointer->points_to));
        }
    }

    return "no printter";
}

} // namespace TypeCheck