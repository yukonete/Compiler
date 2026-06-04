#include <optional>
#include <utility>
#include <variant>

#include "base/panic.h" 
#include "ast.h"
#include "parser.h"
#include "type_check.h"
#include "types.h"

// TODO: 
// 1) Figure out what to do when i want to report where the type is used.
//    Probably should just walk AST in cases like that
// 2) Add error handling in calculate_size_and_alignment()

namespace TypeCheck {

std::optional<Type *> Scope::lookup_type(std::string_view type_name) {
    if (auto search = declarations.find(type_name);
        search != declarations.end()) {
        return search->second;
    }
    if (parent != nullptr) {
        return parent->lookup_type(type_name);
    }
    return {};
}

Type *TypeChecker::create_type_from_ast_type(Scope *scope, Ast::Type *ast_type) {
    switch (ast_type->kind) {
        using enum Ast::Type::Kind;

        case BAD: panic("Bad type got in type checker");

        case IDENTIFIER: {
            auto identifier = ast_type->as<Ast::TypeIdentifier>()->get_full_type_name();

            auto bultin_type = check_builtin_type(identifier);
            if (bultin_type) {
                return bultin_type.value();
            }

            if (auto lookup_result = scope->lookup_type(identifier);
                lookup_result) {
                return lookup_result.value();
            }

            auto type = Type{};
            type.kind = TypeKind::placeholder;
            type.ast_type = ast_type;
            type.type_name = identifier;
            auto placeholder =
                arena_->create<Type>(type);
            return placeholder;
        }
        
        case STRUCT: {
            auto ast_struct = ast_type->as<Ast::TypeStruct>();
            auto type = arena_->create<Struct>();
            type->ast_type = ast_type;

            auto members_count = ast_struct->members.size();
            // type->members = arena_->push_array<StructMember>(members_count);
            for (usize i = 0; i < members_count; ++i) {
                auto member = &type->members[i];
                auto ast_member = ast_struct->members[i];

                member->name = ast_member->identifier->token.value;
                member->type = create_type_from_ast_type(scope, ast_member->type);
            }

            return type;
        }

        case POINTER: {
            auto ast_pointer = ast_type->as<Ast::TypePointer>();
            auto pointer = arena_->create<Pointer>();
            pointer->ast_type = ast_type;
            pointer->points_to =
                create_type_from_ast_type(scope, ast_pointer->type);
            return pointer;
        }

        case ARRAY: {
            auto ast_array = ast_type->as<Ast::TypeArray>();
            auto array = arena_->create<Array>();
            if (!ast_array->count->is<Ast::IntegerLiteralExpression>()) {
                // TODO:
                report_error(/*ast_array->count->location*/{}, "Array size should be integer literal (for now).");
            } else {
                array->count = ast_array->count->as<Ast::IntegerLiteralExpression>()->value;
            }
            array->elem_type = create_type_from_ast_type(scope, ast_array->element_type);
            return array; 
        }

        case FUNCTION: {
            auto ast_procedure = ast_type->as<Ast::TypeProcedure>();
            report_error(ast_procedure->token.start, "Declaring type of procedure is not supported (for now).");
            return arena_->create<Type>();
        }
    }

    panic("Should be unreachable")
}

Type *TypeChecker::resolve_type(Scope *scope, Type *type, bool resolve_non_anonymous_types) {
    auto is_type_anonymous = type->type_name == "";
    if (type->kind == TypeKind::placeholder || is_type_anonymous ||
        resolve_non_anonymous_types) {
        switch (type->kind) {
            using enum TypeKind;

            case placeholder: {
                auto lookup_result = scope->lookup_type(type->type_name);
                if (lookup_result) {
                    type = lookup_result.value();
                } else {
                    // TODO:
                    report_error(/*type->ast_type->location*/{}, "Type {} is not defined.", type->type_name);
                }
                break;
            }

            case alias: {
                auto alias = static_cast<Alias *>(type);
                alias->alias_to = resolve_type(scope, alias->alias_to);
                break;
            }

            case kind_struct: {
                auto st = static_cast<Struct *>(type);
                for (auto &member : st->members) {
                    member.type = resolve_type(scope, member.type);
                }
                break;
            }

            case pointer: {
                auto pointer = static_cast<Pointer *>(type);
                pointer->points_to = resolve_type(scope, pointer->points_to);
                break;
            }

            case array: {
                auto array = static_cast<Array *>(type);
                array->elem_type = resolve_type(scope, array->elem_type);
            }

            case invalid: break;

            default: panic("Not implemented");
        }
    }

    return type;
}

static bool is_void(const Type *type) {
    if (type->kind == TypeKind::alias) {
        auto alias = static_cast<const Alias *>(type);
        return is_void(alias->alias_to);
    }

    return type->kind == TypeKind::kind_void;
}

static bool check_for_recursive_structs_recurse(std::vector<Type *> *met_types, Type *type) {
    if (std::find(met_types->begin(), met_types->end(), type) !=
        met_types->end()) {
        return true;
    }

    auto result = false;
    met_types->push_back(type);
    if (type->kind == TypeKind::alias) {
        auto alias = static_cast<Alias *>(type);
        result = check_for_recursive_structs_recurse(met_types, alias->alias_to);
    } else if (type->kind == TypeKind::kind_struct) {
        auto st = static_cast<Struct *>(type);
        for (const auto &member : st->members) {
            result = check_for_recursive_structs_recurse(met_types, member.type);
            if (result) {
                break;
            }
        }
    } else if (type->kind == TypeKind::array) {
        auto array = static_cast<Array *>(type);
        result = check_for_recursive_structs_recurse(met_types, array->elem_type);
    }
    if (!result) {
        met_types->pop_back();
    }
    return result;
}


static bool type_is_or_contains_alias(Alias *alias, Type *type) {
    if (alias == type) {
        return true;
    }

    if (type->kind == TypeKind::alias) {
        auto alias_type = static_cast<Alias *>(type);
        return type_is_or_contains_alias(alias, alias_type->alias_to);
    } else if (type->kind == TypeKind::kind_struct) {
        auto st = static_cast<Struct *>(type);
        for (const auto &member : st->members) {
            if (type_is_or_contains_alias(alias, member.type)) {
                return true;
            }
        }
    } else if (type->kind == TypeKind::pointer) {
        auto pointer = static_cast<Pointer *>(type);
        return type_is_or_contains_alias(alias, pointer->points_to);
    } else if (type->kind == TypeKind::array) {
        auto array = static_cast<Array *>(type);
        return type_is_or_contains_alias(alias, array->elem_type);
    }

    return false;
}

static void calculate_size_and_alignment(Type *type) {
    if (type->size != -1 && type->align != -1) {
        return;
    }
    assert(type->size == -1);
    assert(type->align == -1);

    switch (type->kind) {
        using enum TypeKind;
        case invalid:
        case placeholder:
            panic("Tried to calculate size of invalid or placeholder type");
        case boolean: panic("Boolean has unknown size or alignment");
        case pointer: panic("Pointer has unknown size or alignment");
        case integer: panic("Integer has unknown size or alignment");
        case kind_void: panic("Void has unknown size or alignment");

        case alias: {
            auto alias = static_cast<Alias *>(type);
            calculate_size_and_alignment(alias->alias_to);
            alias->size = alias->alias_to->size;
            alias->align = alias->alias_to->align;
            return;
        }

        case kind_struct: {
            auto st = static_cast<Struct *>(type);
            isize st_size = 0;
            isize biggest_align = 0;
            for (const auto &member : st->members) {
                calculate_size_and_alignment(member.type);
                if (is_void(member.type)) {
                    panic("Struct's member of type void is not allowed");
                }
                if (member.type->align > biggest_align) {
                    biggest_align = member.type->align;
                }
                st_size = round(st_size, member.type->align);
                st_size += member.type->size;
            }
            st->align = biggest_align;
            st->size = st_size;
            if (!st->members.empty()) {
                st->size = round(st->size, st->align);
            }
            return;
        }

        case array: {
            auto array = static_cast<Array *>(type);
            auto elem = array->elem_type;
            if (is_void(elem)) {
                panic("Array of type void is not allowed");
            }
            if (array->count <= 0) {
                panic("Size of an array must be greater than 0");
            }
            calculate_size_and_alignment(elem);
            array->align = elem->align;
            array->size = array->count * elem->size;
            return;
        }
    }
}

bool TypeChecker::add_type_declarations_to_scope(std::span<Ast::Statement *> statements,
                                   Scope *scope) {
    auto check_for_recursing_structs = [this](Scope *scope) -> bool {
        auto result = false;
        std::vector<Type *> met_types;
        for (const auto &[name, type] : scope->declarations) {
            if (type->kind != TypeKind::kind_struct) {
                continue;
            }
            if (check_for_recursive_structs_recurse(&met_types, type)) {
                if (met_types.at(0) == met_types.at(met_types.size() - 1)) {
                    report_error(type->ast_declaration->identifier->token.start,
                                 "Found self reference in type {}.",
                                 type->type_name);
                }
                result = true;
                met_types.clear();
            }
        }
        return result;
    };

    auto check_for_recursing_aliases = [this](Scope *scope) -> bool {
        auto result = false;
        for (const auto &[name, type] : scope->declarations) {
            if (type->kind != TypeKind::alias) {
                continue;
            }
            auto alias = static_cast<Alias *>(type);
            if (type_is_or_contains_alias(alias, alias->alias_to)) {
                report_error(type->ast_declaration->identifier->token.start,
                             "Found self referencing alias {}.",
                             type->type_name);
                result = true;
            }
        }
        return result;
    };

    for (auto statement : statements) {
        if (statement->is<Ast::DeclarationStatement>()) {
            auto decl = statement->as<Ast::DeclarationStatement>()->declaration;
            auto decl_identifier = decl->identifier->token.value;
            if (scope->lookup_type(decl_identifier)) {
                report_error(decl->identifier->token.start,
                             "Declaration with identifier {} already exists.",
                             decl_identifier);
                return false;
            }
            if (!decl->is<Ast::TypeDeclaration>()) {
                report_error(decl->identifier->token.start, 
                    "Declaration {} is not a type. Type checker only supports types (for now).", 
                    decl_identifier);
                continue;
            }
            auto type_decl = decl->as<Ast::TypeDeclaration>();
            auto type = create_type_from_ast_type(scope, type_decl->type);
            if (type->kind == TypeKind::kind_struct) {
                type->ast_declaration = type_decl;
                type->type_name = decl_identifier;
                scope->declarations[decl_identifier] = type;
            } else {
                // If it is not a struct, then create alias to that type
                // Instead maybe make struct an alias as well
                // But then two aliases which declare same struct will be
                // threated as same type
                // But if i add distinct alias, than struct can just be distinct alias
                auto alias = arena_->create<Alias>();
                alias->ast_declaration = type_decl;
                alias->ast_type = type_decl->type;
                alias->alias_to = type;
                alias->type_name = decl_identifier;
                scope->declarations[decl_identifier] = alias;
            }
        }
    }

    for (auto &[name, type] : scope->declarations) {
        resolve_type(scope, type, true);
    }

    if (error_count_ != 0) {
        return false;
    }

    auto recursive_structs = check_for_recursing_structs(scope);
    auto recrusive_alaises = check_for_recursing_aliases(scope);
    if (recursive_structs || recrusive_alaises) {
        return false;
    }

    for (const auto &[name, type] : scope->declarations) {
        calculate_size_and_alignment(type);
    }

    return error_count_ == 0;
}

bool TypeChecker::do_type_check(Ast::Parser &parser) {
    return add_type_declarations_to_scope(std::span{parser.ast}, &global_scope);
}

std::string type_to_string(const Type *type, bool) {
    switch (type->kind) {
        using enum TypeKind;
        case invalid: {
            return std::format("kind: invalid\n"
                               "name: {}",
                               type->type_name);
        }
        case placeholder: {
            return std::format("kind: placeholder\n"
                               "name: {}",
                               type->type_name);
        }
        case kind_void: {
            return std::format("kind: void\n");
        }
        case alias: {
            auto alias = static_cast<const Alias *>(type);
            return std::format("kind: alias\n"
                               "name: {}\n"
                               "alias_to: {}\n"
                               "size: {}\n"
                               "align: {}",
                               alias->type_name, alias->alias_to->type_name,
                               alias->size, alias->align);
        }
        case integer: {
            return std::format("kind: integer\n"
                               "name: {}\n"
                               "size: {}\n"
                               "align: {}",
                               type->type_name, type->size, type->align);
        }
        case boolean: {
            return std::format("kind: boolean\n"
                               "name: {}\n"
                               "size: {}\n"
                               "align: {}",
                               type->type_name, type->size, type->align);
        }
        case kind_struct: {
            auto st = static_cast<const Struct *>(type);
            auto result = std::format("kind: kind_struct\n"
                                      "name: {}\n"
                                      "members:",
                                      st->type_name);
            for (const auto &member : st->members) {
                result += std::format("\n    {}: {}", member.name,
                                      member.type->type_name);
            }
            result += std::format("\nsize: {}\n"
                                  "align: {}",
                                  st->size, st->align);
            return result;
        }
        case pointer: {
            auto pointer = static_cast<const Pointer *>(type);
            return std::format("kind: pointer\n"
                               "points_to: {}\n"
                               "size: {}\n"
                               "align: {}",
                               pointer->points_to->type_name, pointer->size,
                               pointer->align);
        }
        case array: {
            auto array = static_cast<const Array *>(type);
            return std::format("kind: array\n"
                               "elem_type: {}\n"
                               "count: {}\n"
                               "size: {}\n"
                               "align: {}",
                               array->elem_type->type_name, array->count,
                               array->size, array->align);
        }
    }

    return "no printter";
}

} // namespace TypeCheck