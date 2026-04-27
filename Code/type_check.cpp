#include "type_check.h"
#include "base.h"
#include "parser.h"
#include "types.h"

#include <stack>
#include <utility>

namespace TypeCheck {

// Returns whether the type contains recursive types
// TODO: Name this better
static bool walk_type(std::vector<Type *> *met_types, Type *type) {
    if (std::find(met_types->begin(), met_types->end(), type) !=
        met_types->end()) {
        return true;
    }

    auto result = false;
    met_types->push_back(type);
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
    } else if (type->kind == TypeKind::array) {
        auto array = reinterpret_cast<Array *>(type);
        result = walk_type(met_types, array->elem_type);
    }
    if (!result) {
        met_types->pop_back();
    }
    return result;
}

// Returns whether the type contains alias
// TODO: Name this better
static bool walk_type_alias(Alias *alias, Type *type) {
    if (alias == type) {
        return true;
    }

    if (type->kind == TypeKind::alias) {
        auto alias_type = reinterpret_cast<Alias *>(type);
        return walk_type_alias(alias, alias_type->alias_to);
    } else if (type->kind == TypeKind::kind_struct) {
        auto st = reinterpret_cast<Struct *>(type);
        for (const auto &member : st->members) {
            if (walk_type_alias(alias, member.type)) {
                return true;
            }
        }
    } else if (type->kind == TypeKind::pointer) {
        auto pointer = reinterpret_cast<Pointer *>(type);
        return walk_type_alias(alias, pointer->points_to);
    } else if (type->kind == TypeKind::array) {
        auto array = reinterpret_cast<Array *>(type);
        return walk_type_alias(alias, array->elem_type);
    }

    return false;
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
// Returns whether at least one type was reported
bool TypeChecker::check_for_recursing_structs(Scope *scope) {
    auto result = false;
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
            result = true;
            met_types.clear();
        }
    }
    return result;
}

// Returns whether at least alias is recursing
bool TypeChecker::check_for_recursing_aliases(Scope *scope) {
    auto result = false;
    for (const auto &[name, type] : scope->declarations) {
        if (type->kind != TypeKind::alias) {
            continue;
        }
        auto alias = reinterpret_cast<Alias *>(type);
        if (walk_type_alias(alias, alias->alias_to)) {
            std::println("Found self referencing alias {}.", type->type_name);
            result = true;
        }
    }
    return result;
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

        case type_array: {
            auto ast_array = reinterpret_cast<Ast::TypeArray *>(ast_type);
            auto array = arena->push_item<Array>();
            array->count = ast_array->count;
            array->elem_type = create_type_from_ast_type(ast_array->elem_type);
            return array;
        }

        default: panic("Not implemented");
    }
}

Type *TypeChecker::lookup_type(std::string_view type_name) {
    if (auto search = global_scope.declarations.find(type_name);
        search != global_scope.declarations.end()) {
        return search->second;
    } else {
        panic("Type {} is not definied", type_name);
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

            case array: {
                auto array = reinterpret_cast<Array *>(type);
                array->elem_type = resolve_type(array->elem_type);
            }

            case invalid: break;

            default: panic("Not implemented");
        }
    }

    return type;
}

static bool is_void(const Type *type) {
    if (type->kind == TypeKind::alias) {
        auto alias = reinterpret_cast<const Alias*>(type);
        return is_void(type);
    }

    return type->kind == TypeKind::kind_void;
}

static void calculate_size_and_align(Type *type) {
    if (type->size != -1 && type->align != -1) {
        return;
    }
    assert(type->size == -1);
    assert(type->align == -1);

    switch (type->kind) {
        using enum TypeKind;
        case invalid:
        case placeholder:
            panic("Tried to calculate size of invalid or placeholder type.");

        case alias: {
            auto alias = reinterpret_cast<Alias *>(type);
            calculate_size_and_align(alias->alias_to);
            alias->size = alias->alias_to->size;
            alias->align = alias->alias_to->align;
            return;
        }

        case boolean:   panic("Boolean has unknown size or alignment.");
        case pointer:   panic("Pointer has unknown size or alignment.");
        case integer:   panic("Integer has unknown size or alignment.");
        case kind_void: panic("Void has unknown size or alignment.");

        case kind_struct: {
            auto st = reinterpret_cast<Struct *>(type);
            isize st_size = 0;
            isize biggest_align = 0;
            for (const auto &member : st->members) {
                calculate_size_and_align(member.type);
                if (is_void(member.type)) {
                    panic("Sruct's member of type void is not allowed");
                }
                if (member.type->align > biggest_align) {
                    biggest_align = member.type->align;
                }
                st_size = align_forward(st_size, member.type->align);
                st_size += member.type->size;
            }
            st->align = biggest_align;
            st->size = align_forward(st_size, st->align);
            return;
        }

        case array: {
            auto array = reinterpret_cast<Array *>(type);
            auto elem = array->elem_type;
            if (is_void(elem)) {
                panic("Array of type void is not allowed");
            }
            calculate_size_and_align(elem);
            array->align = elem->align;
            array->size = array->count * elem->size;
            return;
        }
    }
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
                panic("Declaration with identifier {} already exists",
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
    auto recursive_structs = check_for_recursing_structs(&global_scope);
    auto recrusive_alaises = check_for_recursing_aliases(&global_scope);
    if (recursive_structs || recrusive_alaises) {
        panic("Recursive type");
    }
    // Calculate sizes
    for (const auto &[name, type] : global_scope.declarations) {
        calculate_size_and_align(type);
    }

    return;
}

std::string type_to_string(const Type *type, bool declaration) {
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
            auto alias = reinterpret_cast<const Alias *>(type);
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
            auto st = reinterpret_cast<const Struct *>(type);
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
            auto pointer = reinterpret_cast<const Pointer *>(type);
            return std::format("kind: pointer\n"
                               "points_to: {}\n"
                               "size: {}\n"
                               "align: {}",
                               pointer->points_to->type_name, pointer->size,
                               pointer->align);
        }
        case array: {
            auto array = reinterpret_cast<const Array *>(type);
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