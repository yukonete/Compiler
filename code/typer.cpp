#include "typer.h"
#include "ast.h"

void Typer::collect_entity(Scope *scope, Ast::Declaration *declaration) {
    switch (declaration->kind) {
        using enum Ast::Declaration::Kind;

        case VARIABLE: {
            auto ast_variable = declaration->as<Ast::VariableDeclaration>();
            assert(ast_variable->type || ast_variable->value);
            auto entity = create_entity<EntityVariable>(
                scope, ast_variable, ast_variable->type, ast_variable->value);
            add_entity(scope, entity);
            break;
        }

        case CONSTANT: {
            auto ast_constant = declaration->as<Ast::ConstDeclaration>();
            auto entity = create_entity<EntityConstant>(
                scope, ast_constant, ast_constant->type, ast_constant->value);
            add_entity(scope, entity);
            break;
        }

        case FUNCTION: {
            auto ast_proc = declaration->as<Ast::ProcedureDeclaration>();
            auto entity = create_entity<EntityProcedure>(scope, ast_proc,
                                                         ast_proc->type);
            add_entity(scope, entity);
            break;
        }

        case TYPE: {
            auto ast_type = declaration->as<Ast::TypeDeclaration>();
            auto entity = create_entity<EntityNamedType>(scope, ast_type,
                                                         ast_type->type);

            if (ast_type->type->is<Ast::TypeStruct>()) {
                auto type_struct = ast_type->type->as<Ast::TypeStruct>();
                entity->inner_scope = create_scope(scope);
                collect_entities(entity->inner_scope.value(), type_struct->declarations);
            }

            add_entity(scope, entity);
            break;
        }
    }
}

void Typer::collect_entities(Scope *scope,
                             std::span<Ast::Statement *> statements) {
    for (auto statement : statements) {
        if (statement->is<Ast::DeclarationStatement>()) {
            collect_entity(scope, statement->as<Ast::DeclarationStatement>()->declaration);
        }
    }
}

void Typer::collect_entities(
    Scope *scope, std::span<Ast::DeclarationStatement *> declarations) {
    for (auto statement : declarations) {
        collect_entity(scope, statement->declaration);
    }
}

bool Typer::do_typing() {
    collect_entities(file_scope, parser_.ast());
    return true;
}