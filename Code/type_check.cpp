#include "type_check.h"
#include "base.h"
#include "parser.h"
#include "types.h"

namespace TypeCheck {

Types::Type *TypeChecker::create_type_from_ast_type(Ast::Type *ast_type) {
    switch (ast_type->type) {
        using enum Ast::NodeType;

        case type_identifier: {
            auto ast_type_identifier =
                reinterpret_cast<Ast::TypeIdentifier *>(ast_type);
            auto identifier = ast_type_identifier->identifier.identifier;

            auto bultin_type = Types::check_builtin_type(identifier);
            if (bultin_type) {
                return bultin_type.value();
            }

            if (auto search = global_scope.declarations.find(identifier);
                search != global_scope.declarations.end()) {
                return search->second;
            }

            auto placeholder = arena->push_item<Types::Type>(
                Types::Type{.kind = Types::TypeKind::placeholder,
                            .ast_type = ast_type,
                            .type_name = identifier});
            return placeholder;
        }

        case type_struct: {
            auto ast_record = reinterpret_cast<Ast::TypeStruct *>(ast_type);
            auto record = arena->push_item<Types::Record>();
            record->ast_type = ast_type;
            auto members_count = ast_record->members.size();
            record->members =
                arena->push_array<Types::RecordMember>(members_count);
            for (int i = 0; i < members_count; ++i) {
                auto member = &record->members[i];
                auto ast_member = ast_record->members[i];

                member->name = ast_member->identifier.identifier;
                member->type = create_type_from_ast_type(ast_member->type);
            }
            return record;
        }

        case type_pointer: {
            auto ast_pointer = reinterpret_cast<Ast::TypePointer *>(ast_type);
            auto pointer = arena->push_item<Types::Pointer>();
            pointer->ast_type = ast_type;
            pointer->points_to =
                create_type_from_ast_type(ast_pointer->points_to);
            return pointer;
        }

        default: panic("Not implemented");
    }
}

void TypeChecker::do_type_check(Ast::Program *program) {
    for (auto decl : program->declarations) {
        if (decl->type == Ast::NodeType::declaration_type) {
            auto type_decl = reinterpret_cast<Ast::TypeDeclaration *>(decl);
            auto identifier = type_decl->identifier.identifier;
            if (!global_scope.declarations.contains(identifier)) {
                auto type = create_type_from_ast_type(type_decl->declared_type);
                type->type_name = identifier;
                global_scope.declarations[identifier] = type;
            } else {
                // TODO: Diagnostics
            }
        }
    }
    return;
}
} // namespace TypeCheck