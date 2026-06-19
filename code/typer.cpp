#include <ranges>
#include <string_view>
#include <unordered_set>

#include "base/panic.h"
#include "typer.h"
#include "ast.h"
#include "error.h"

namespace Typing {

static std::string full_entity_name(const Entity *entity) {
    std::string_view entity_name;
    if (has_flag(entity->flags, Entity::Flags::BUILTIN)) {
        entity_name = entity->type->as<NamedType>()->name;
    } else {
        entity_name = entity->declaration->identifier->token.value;
    }

    auto scope_name = entity->scope->full_name();
    if (scope_name == "") {
        return std::string{entity_name};
    }
    return std::format("{}.{}", entity->scope->full_name(), entity_name);
}

std::string Scope::full_name() const {
    if (!entity || !(*entity)->is<NamedTypeEntity>()) {
        return "";
    }

    auto named_type = (*entity)->type->as<NamedType>();
    auto parent_scope_name = parent.value()->full_name();
    if (parent_scope_name == "") {
        return std::string{named_type->name};
    }

    return std::format("{}.{}", parent_scope_name, named_type->name);
}

std::optional<Entity*> Scope::look_up(Ast::Identifier *identifier) const {
    auto search = entities.find(identifier->token.value);
    if (search != entities.end()) {
        auto entity = search->second;
        if (!entity->is<VariableEntity>()) {
            return search->second;
        }
        
        auto variable = entity->as<VariableEntity>();
        if (variable->variable_kind != VariableEntity::VariableKind::LOCAL) {
            return search->second;
        }

        if (variable->declaration->identifier == identifier) {
            return search->second;
        }

        auto declaration_position = variable->declaration->end_token().end.byte;
        auto usage_position = identifier->token.start.byte;
        if (usage_position > declaration_position) {
            return search->second;
        }

        return {};
    }
    if (parent) {
        return (*parent)->look_up(identifier);
    }
    return {};
}

std::optional<Entity*> Scope::look_up(Ast::TypePath path) const {
    assert(path.size() > 0);

    const Scope *lookup_scope = this;
    Entity *looked_up_entity = nullptr;
    usize path_index = 0;
    for (; path_index < path.size(); ++path_index) {
        looked_up_entity = nullptr;
        auto identifier = path[path_index];
        
        auto search = lookup_scope->look_up(identifier);
        if (!search) {
            break;
        }

        looked_up_entity = *search;
        if (!looked_up_entity->is<NamedTypeEntity>()) {
            break;
        }

        auto type_entity = looked_up_entity->as<NamedTypeEntity>();
        bool is_struct = false;
        if (type_entity->type->as<NamedType>()->type != nullptr) {
            is_struct = type_entity->type->as<NamedType>()->type->is<StructType>();
        } else {
            assert(type_entity->ast_type != nullptr);
            is_struct = type_entity->ast_type->is<Ast::StructType>();
        }

        if (!is_struct) {
            break;
        }

        lookup_scope = type_entity->inner_scope.value();
    }

    // If looked up type is struct than index is going to be bigger by one
    // so decrement it
    if (looked_up_entity != nullptr && path_index == path.size()) {
        path_index -= 1;
    }

    if (looked_up_entity == nullptr || path_index != path.size() - 1) {
        return {};
    }

    return looked_up_entity;
}

bool Typer::add_entity(Scope *scope, Entity *entity) {
    return add_entity(scope, entity, entity->declaration->identifier->token.value);
}

bool Typer::add_entity(Scope *scope, Entity *entity, std::string_view name) {
    // manually search entities of a scope, so that parent scope is not being searched
    // (so that shadowing is allowed)
    auto old_entity = scope->entities.find(name);
    if (old_entity != scope->entities.end()) {
        redeclaration_error(old_entity->second, entity);
        return false;
    }

    entities_.push_back(entity);
    scope->entities[name] = entity;
    return true;
}

Scope *Typer::create_scope(Scope *parent) {
    scopes_.push_back(make_allocator_unique<Scope>(scopes_storage_));
    auto new_scope = scopes_.back().get();
    if (parent != nullptr) {
        new_scope->parent = parent;
    }
    return scopes_.back().get();
}

void Typer::collect_entities_from_statement(Scope *scope, Ast::Statement *statement) {
    switch (statement->kind) {
        using enum Ast::Statement::Kind;
        case DECLARATION: {
            auto declaration =
                statement->as<Ast::DeclarationStatement>()->declaration;
            collect_entity(scope, declaration, true);
            break;
        }
        case IF: {
            auto if_statement = statement->as<Ast::IfStatement>();
            collect_entities_from_statement(scope, if_statement->body);
            if (if_statement->else_branch) {
                collect_entities_from_statement(scope, if_statement->else_branch->body);
            }
            break;
        }
        case WHILE: {
            auto while_statement = statement->as<Ast::WhileStatement>();
            collect_entities_from_statement(scope, while_statement->body);
            break;
        }
        case BLOCK: {
            auto block = statement->as<Ast::BlockStatement>();
            auto block_scope = create_scope(scope);
            block_scopes_[block] = block_scope;
            for (auto stmt : block->body) {
                collect_entities_from_statement(block_scope, stmt);
            }
            break;
        }
        default: break;
    }
}

bool Typer::collect_entity(Scope *scope, Ast::Declaration *declaration,
                           bool local) {
    if ((declaration->flags & Ast::Declaration::Flags::HANDLED) ==
        Ast::Declaration::Flags::HANDLED) {
        return false;
    }
    declaration->flags |= Ast::Declaration::Flags::HANDLED;

    switch (declaration->kind) {
        using enum Ast::Declaration::Kind;

        case VARIABLE: {
            auto ast_variable = declaration->as<Ast::VariableDeclaration>();
            assert(ast_variable->type);
            auto entity = create_entity<VariableEntity>(
                scope, ast_variable, ast_variable->type.value(), ast_variable->value);
            add_entity(scope, entity);
            if (local) {
                entity->variable_kind = VariableEntity::VariableKind::LOCAL;
            }
            break;
        }

        case CONSTANT: {
            auto ast_constant = declaration->as<Ast::ConstDeclaration>();
            assert(ast_constant->type);
            auto entity = create_entity<ConstantEntity>(
                scope, ast_constant, ast_constant->type.value(), ast_constant->value);
            add_entity(scope, entity);
            break;
        }

        case PROCEDURE: {
            auto ast_proc = declaration->as<Ast::ProcedureDeclaration>();
            auto entity = create_entity<ProcedureEntity>(
                scope, ast_proc, ast_proc->type, create_scope(scope));
            add_entity(scope, entity);

            for (auto ast_parameter : ast_proc->type->parameters) {
                auto parameter_entity = create_entity<VariableEntity>(
                    entity->inner_scope, ast_parameter, ast_parameter->type,
                    std::optional<Ast::Expression *>{});
                parameter_entity->variable_kind = VariableEntity::VariableKind::PARAMETER;
                add_entity(entity->inner_scope, parameter_entity);
            }

            collect_entities_from_statement(entity->inner_scope, ast_proc->body);
            break;
        }

        case TYPE: {
            auto ast_type = declaration->as<Ast::TypeDeclaration>();
            auto entity = create_entity<NamedTypeEntity>(
                scope, ast_type, ast_type->type, create_scope(scope));
            entity->inner_scope.value()->entity = entity;
            
            add_entity(scope, entity);

            if (ast_type->type->is<Ast::StructType>()) {
                auto ast_struct = ast_type->type->as<Ast::StructType>();
                collect_entities(entity->inner_scope.value(), ast_struct->declarations);
            }

            entity->type = create_type<NamedType>(ast_type->identifier->token.value, entity);
            break;
        }

        case FIELD: panic("Should be caught in parser");
    }
    return true;
}

bool Typer::collect_entities(Scope *scope,
                             std::span<Ast::Statement *> statements) {
    auto collected = false; 
    for (auto statement : statements) {
        if (statement->is<Ast::DeclarationStatement>()) {
            collected |= collect_entity(scope, statement->as<Ast::DeclarationStatement>()->declaration, false);
        }
    }
    return collected;
}

bool Typer::collect_entities(
    Scope *scope, std::span<Ast::DeclarationStatement *> declarations) {
    auto collected = false; 
    for (auto statement : declarations) {
        collected |= collect_entity(scope, statement->declaration, false);
    }
    return collected;
}

Type *Typer::ast_type_to_type(Scope *scope, Ast::Type *ast_type) {
    switch (ast_type->kind) {
        using enum Ast::Type::Kind;
        
        case BAD: panic("BAD type");

        case IDENTIFIER: {
            auto ast_identifier = ast_type->as<Ast::IdentifierType>();
            auto entity = look_up_type(scope, ast_identifier->path);
            if (!entity) {
                return create_type<BadType>();
            }
            return (*entity)->type;
        }
        
        case POINTER: {
            auto ast_pointer = ast_type->as<Ast::PointerType>();
            auto type = ast_type_to_type(scope, ast_pointer->type);
            return create_type<PointerType>(type);
        }

        case ARRAY: {
            auto ast_array = ast_type->as<Ast::ArrayType>();
            auto type = ast_type_to_type(scope, ast_array->element_type);
            auto array = create_type<ArrayType>(type, u64(0), ast_array->count, scope);
            arrays_without_size_.push_back(array);
            return array;
        }

        case FUNCTION: {
            auto ast_proc = ast_type->as<Ast::ProcedureType>();
            std::vector<VariableEntity*> parameters_temp;
            for (auto ast_parameter : ast_proc->parameters) {
                auto parameter_entity = create_entity<VariableEntity>(
                    scope, ast_parameter, ast_parameter->type,
                    std::optional<Ast::Expression *>{});
                parameter_entity->variable_kind = VariableEntity::VariableKind::PARAMETER;
                parameter_entity->flags |= Entity::Flags::RESOLVED;
                parameter_entity->type = ast_type_to_type(scope, ast_parameter->type);
                parameters_temp.push_back(parameter_entity);
            }
            auto parameters = create_array(std::span{parameters_temp});
            auto return_type = std::optional<Type*>();
            if (ast_proc->return_type) {
                return_type = ast_type_to_type(scope, *ast_proc->return_type);
            }
            return create_type<PointerType>(create_type<ProcedureType>(parameters, return_type));
        }

        case STRUCT: {
            auto ast_struct = ast_type->as<Ast::StructType>();
            auto entity = get_entity_by_ast_type(ast_struct);
            // If there is no entity for a struct or entity is not
            // NamedTypeEntity, that means that struct is anonymous
            if (!entity || !(*entity)->is<NamedTypeEntity>()) {
                scope = create_scope(scope);
            } else {
                scope = (*entity)->as<NamedTypeEntity>()->inner_scope.value(); 
            }

            std::vector<VariableEntity*> members_temp;
            for (auto ast_member : ast_struct->members) {
                auto member_entity = create_entity<VariableEntity>(
                    scope, ast_member, ast_member->type,
                    std::optional<Ast::Expression *>{});
                member_entity->variable_kind = VariableEntity::VariableKind::STRUCT_MEMBER;
                member_entity->flags |= Entity::Flags::RESOLVED;
                member_entity->type = ast_type_to_type(scope, ast_member->type);
                members_temp.push_back(member_entity);
            }
            auto members = create_array(std::span{members_temp});
            return create_type<StructType>(members, scope);
        }
    }
    assert(false && "Shoud not trigger");
    return nullptr;
}

void Typer::resolve_entity(Entity *entity) {
    if ((entity->flags & Entity::Flags::RESOLVED) == Entity::Flags::RESOLVED) {
        return;
    } 
    entity->flags |= Entity::Flags::RESOLVED;

    if (entity->is<NamedTypeEntity>()) {
        entity->type->as<NamedType>()->type = ast_type_to_type(entity->scope, entity->ast_type);
    } else {
        entity->type = ast_type_to_type(entity->scope, entity->ast_type);
    }
}

void Typer::resolve_alias(NamedTypeEntity *entity) {
    auto type = entity->type->as<NamedType>();
    if ((entity->flags & Entity::Flags::RESOLVED) == Entity::Flags::RESOLVED ||
        !entity->ast_type->is<Ast::IdentifierType>()) {
        return;
    }
    entity->flags |= Entity::Flags::RESOLVED;

    auto path = entity->ast_type->as<Ast::IdentifierType>()->path;
    for (usize i = 0; i < path.size(); ++i) {
        auto looked_up_entity = look_up_type(entity->scope, path.subspan(0, i + 1));
        if (looked_up_entity) {
            resolve_alias(*looked_up_entity);
        }
    }

    auto aliased_of = look_up_type(entity->scope, path);
    if (aliased_of) {
        entity->aliased_of = *aliased_of;
        type->type = entity->aliased_of->type;
    } else {
        type->type = create_type<BadType>();
    }
}

bool Typer::check_for_recursive_expression(Scope *scope,
                                           Ast::Expression *expression,
                                           std::vector<const Entity *> &path) {
    switch (expression->kind) {
        using enum Ast::Expression::Kind;

        case BAD: panic("BAD type");
        case INTEGER_LITERAL:
        case BOOL_LITERAL:
        case STRING_LITERAL:
        case FLOAT_LITERAL: return false;
        case CAST_OPERATOR: {
            auto cast = expression->as<Ast::CastOperatorExpression>();
            // Might want to store that type somewhere, but for now just check
            // it for recursion
            auto type = ast_type_to_type(scope, cast->type);
            if (check_for_recursive_type(scope, type, path)) {
                return true;
            }
            return check_for_recursive_expression(scope, cast->expression, path);
        }
        case UNARY_OPERATOR: {
            auto unary = expression->as<Ast::UnaryOperatorExpression>();
            return check_for_recursive_expression(scope, unary->right, path);
        }
        case BINARY_OPERATOR: {
            auto binary = expression->as<Ast::BinaryOperatorExpression>();
            if (check_for_recursive_expression(scope, binary->left, path)) {
                return true;
            }
            return check_for_recursive_expression(scope, binary->right, path);
        }
        case INDEX: {
            auto index = expression->as<Ast::IndexExpression>();
            if (check_for_recursive_expression(scope, index->expression, path)) {
                return true;
            }
            return check_for_recursive_expression(scope, index->index, path);
        }
        case CALL_OPERATOR: {
            auto call = expression->as<Ast::CallOperatorExpression>();
            for (auto arg : call->arguments) {
                if (check_for_recursive_expression(scope, arg, path)) {
                    return true;
                }
            }
            if (check_for_recursive_expression(scope, call->expression, path)) {
                return true;
            }
            return false;
        }
        case IDENTIFIER: {
            auto identifier =
                expression->as<Ast::IdentifierExpression>()->identifier;
            auto entity = scope->look_up(identifier);
            if (!entity) {
                return false;
            }
            return check_for_recursive_declaration(*entity, path);
        }
        case SELECTOR: {
            std::vector<Ast::Identifier *> type_path;
            auto type_path_from_expression =
                [&type_path](this auto &&self, Scope *scope,
                             Ast::Expression *expression) -> bool {
                switch (expression->kind) {
                    case IDENTIFIER: {
                        auto identifier =
                            expression->as<Ast::IdentifierExpression>()
                                ->identifier;
                        type_path.push_back(identifier);
                        return true;
                    }

                    case SELECTOR: {
                        auto selector =
                            expression->as<Ast::SelectorExpression>();
                        if (!self(scope, selector->expression)) {
                            return false;
                        }
                        type_path.push_back(selector->identifier);
                        return true;
                    }

                    default: return false;
                }
            };

            auto selector = expression->as<Ast::SelectorExpression>();
            if (check_for_recursive_expression(scope, selector->expression, path)) {
                return true;
            }
            
            auto type_path_valid = type_path_from_expression(scope, selector);
            if (!type_path_valid) {
                return false;
            }

            auto entity = scope->look_up(std::span{type_path});
            if (!entity) {
                return false;
            }
            return check_for_recursive_declaration(*entity, path);
        }
    }
    assert(false && "Should not trigger");
}

bool Typer::check_for_recursive_type(Scope *scope, const Type *type,
                                     std::vector<const Entity *> &path) {
    switch (type->kind) {
        using enum Type::Kind;

        case BAD: panic("BAD type");
        case INT:
        case ANY:
        case BOOL:
        case FLOAT:
        case STRING:
        case POINTER:
        case PROCEDURE: return false;
        case NAMED: {
            auto named_type = type->as<NamedType>();
            auto entity = named_type->entity;
            if (std::ranges::contains(path, entity)) {
                path.push_back(entity);
                return true;
            }

            path.push_back(entity);
            bool found = check_for_recursive_type(entity->scope, named_type->type, path);
            if (!found) {
                path.pop_back();
            }
            return found;
        }
        case STRUCT: {
            auto struct_type = type->as<StructType>();
            for (auto member : struct_type->members) {
                if (check_for_recursive_type(struct_type->inner_scope,
                                             member->type, path)) {
                    return true;
                }
            }
            return false;
        }
        case ARRAY: {
            auto array_type = type->as<ArrayType>();
            if (check_for_recursive_expression(
                    scope, array_type->count_expression, path)) {
                return true;
            }
            return check_for_recursive_type(scope, array_type->type, path);
        }
    }
    assert(false && "Should not trigger");
    return false;
}

bool Typer::check_for_recursive_statement(Scope *scope,
                                          Ast::Statement *statement,
                                          std::vector<const Entity *> &path) {
    switch (statement->kind) {
        using enum Ast::Statement::Kind;

        case BAD: panic("BAD type");
        case EMPTY: return false;
        case IF: {
            auto if_statement = statement->as<Ast::IfStatement>();
            if (check_for_recursive_expression(scope, if_statement->condition, path)) {
                return true;
            }
            if (check_for_recursive_statement(scope, if_statement->body, path)) {
                return true;
            }
            if (!if_statement->else_branch) {
                return false;
            } 
            return check_for_recursive_statement(scope, if_statement->else_branch->body, path);
        }
        case WHILE: {
            auto while_statement = statement->as<Ast::WhileStatement>();
            if (check_for_recursive_expression(
                    scope, while_statement->condition, path)) {
                return true;
            }
            return check_for_recursive_statement(scope, while_statement->body,
                                                 path);
        }
        case ASSIGNMENT: {
            auto assignment = statement->as<Ast::AssignmentStatement>();
            if (check_for_recursive_expression(scope, assignment->expression, path)) {
                return true;
            }
            return check_for_recursive_expression(scope, assignment->value, path);
        }
        case BLOCK: {
            auto block = statement->as<Ast::BlockStatement>();
            auto search = block_scopes_.find(block);
            assert(search != block_scopes_.end());
            auto block_scope = search->second;
            for (auto stmt : block->body) {
                if (check_for_recursive_statement(block_scope, stmt, path)) {
                    return true;
                }
            }
            return false;
        }
        case RETURN: {
            auto return_statement = statement->as<Ast::ReturnStatement>();
            if (return_statement->value) {
                return check_for_recursive_expression(
                    scope, *return_statement->value, path);
            }
            return false;
        }
        case DECLARATION: {
            auto declaration = statement->as<Ast::DeclarationStatement>()->declaration;
            auto entity = scope->look_up(declaration->identifier);
            return check_for_recursive_declaration(entity.value(), path);
        }
        case CONTINUE:
        case BREAK: return false;
        case EXPRESSION: {
            auto expression_statement =
                statement->as<Ast::ExpressionStatement>();
            return check_for_recursive_expression(
                scope, expression_statement->expression, path);
        }
    }
    assert(false && "Should not trigger");
    return false;
}

bool Typer::check_for_recursive_declaration(const Entity *entity,
                                            std::vector<const Entity *> &path) {
    if (std::ranges::contains(path, entity)) {
        if (entity->is<ProcedureEntity>()) {
            return false;
        }
        // This is needed so that recursive function call result can be
        // assigned to a local variable
        if (path[0] == entity && entity->is<VariableEntity>()) {
            auto varible = entity->as<VariableEntity>();
            if (varible->variable_kind == VariableEntity::VariableKind::LOCAL ||
                varible->variable_kind ==
                    VariableEntity::VariableKind::PARAMETER) {
                return false;
            }
        }
        path.push_back(entity);
        return true;
    }
    path.push_back(entity);

    bool found = false;
    switch (entity->kind) {
        using enum Entity::Kind;

        case VARIABLE: {
            auto variable = entity->as<VariableEntity>();
            if (check_for_recursive_type(variable->scope, variable->type, path)) {
                found = true;
                break;
            }
            if (variable->init_expression) {
                found = check_for_recursive_expression(
                    variable->scope, *variable->init_expression, path);
            }
            break;
        }
        case CONSTANT: {
            auto constant = entity->as<ConstantEntity>();
            if (check_for_recursive_type(constant->scope, constant->type, path)) {
                found = true;
                break;
            }
            found = check_for_recursive_expression(
                constant->scope, constant->init_expression, path);
            break;
        }
        case NAMED_TYPE: {
            auto named_type = entity->as<NamedTypeEntity>();
            found = check_for_recursive_type(
                named_type->scope, named_type->type->as<NamedType>()->type,
                path);
            break;
        }
        case PROCEDURE: {
            auto procedure = entity->as<ProcedureEntity>();
            auto declaration = procedure->declaration->as<Ast::ProcedureDeclaration>();
            found = check_for_recursive_statement(procedure->inner_scope, declaration->body, path);
            break;
        }
    }

    if (!found) {
        path.pop_back();
    }
    return found;
}

bool Typer::do_typing() {
    auto add_builtin_type = [this](Scope *scope, Type *type,
                                   std::string_view name) -> bool {
        type->flags |= Type::Flags::SIZED;
        auto entity =
            create_entity<NamedTypeEntity>(scope, nullptr, nullptr, std::optional<Scope *>{});
        auto type_named = create_type<NamedType>(name, entity);
        type_named->type = type;
        entity->type = type_named;
        entity->flags = Entity::Flags::RESOLVED | Entity::Flags::BUILTIN;
        return add_entity(scope, entity, name);
    };

    bool added = true;
    added &= add_builtin_type(file_scope, create_type<IntType>(u64(8), u64(8)), "int");
    added &= add_builtin_type(file_scope, create_type<IntType>(u64(1), u64(1)), "s8");
    added &= add_builtin_type(file_scope, create_type<IntType>(u64(4), u64(4)), "s32");
    added &= add_builtin_type(file_scope, create_type<IntType>(u64(8), u64(8)), "s64");
    added &= add_builtin_type(file_scope, create_type<FloatType>(u64(4), u64(4)), "f32");
    added &= add_builtin_type(file_scope, create_type<FloatType>(u64(8), u64(8)), "f64");
    added &= add_builtin_type(file_scope, create_type<IntType>(u64(8), u64(8), true), "uint");
    added &= add_builtin_type(file_scope, create_type<IntType>(u64(1), u64(1), true), "u8");
    added &= add_builtin_type(file_scope, create_type<IntType>(u64(4), u64(4), true), "u32");
    added &= add_builtin_type(file_scope, create_type<IntType>(u64(8), u64(8), true), "u64");
    added &= add_builtin_type(file_scope, create_type<AnyType>(), "any");
    added &= add_builtin_type(file_scope, create_type<BoolType>(), "bool");
    // added &= add_builtin_type(file_scope, create_type<StringType>(), "string");
    if (!added) {
        panic("Could not add builtin type to file scope");
    }

    assert(!parser_.lexer.any_errors());

    collect_entities(file_scope, parser_.ast);

    if (parser_.lexer.any_errors()) {
        return false;
    }

    for (auto entity : entities_) {
        if (!entity->is<NamedTypeEntity>()) {
            continue;   
        }

        resolve_alias(entity->as<NamedTypeEntity>());
    }

    if (parser_.lexer.any_errors()) {
        return false;
    }

    for (auto entity : entities_) {
        resolve_entity(entity);
    }

    if (parser_.lexer.any_errors()) {
        return false;
    }

    // Actuall typing can happen even before recursive declarations detection 
    // because size of types is not needed at that stage.
    // But aliases still have to be checked before typing (maybe?).
    // This will also simplify entity collection and recursive declarations detection.

    {
        std::unordered_set<const Entity *> reported_entities;
        std::vector<const Entity *> path;
        for (auto entity : entities_) {
            bool is_recursive_entity = check_for_recursive_declaration(entity, path);
            if (is_recursive_entity && !reported_entities.contains(entity)) {
                assert(!path.empty());
                if (path.front() != path.back()) {
                    path.clear();
                    continue;
                }
                reported_entities.insert(entity);
                if (path.size() == 2) {
                    auto entity_name = full_entity_name(path[0]);
                    error(parser_.lexer, path[0]->declaration,
                        "Invalid recursive declaration '{}': '{}' refers to itself",
                        entity_name, entity_name);
                } else {
                    error(parser_.lexer, path[0]->declaration,
                            "Invalid recursive declaration '{}'", full_entity_name(path[0]));
                    for (usize i = 0; i < path.size() - 1; ++i) {
                        reported_entities.insert(path[i + 1]);
                        error(parser_.lexer, path[i]->declaration,
                                "'{}' refers to '{}'", full_entity_name(path[i]),
                                full_entity_name(path[i + 1]));
                    }
                }
            }

            path.clear();
        }
    }

    if (parser_.lexer.any_errors()) {
        return false;
    }

    for (auto entity : entities_) {
        if (entity->is<ProcedureEntity>() || has_flag(entity->flags, Entity::Flags::BUILTIN)) {
            continue;
        }

        calculate_size_and_alignment(entity->type);
        std::println("Entity: {}", full_entity_name(entity));
        std::println("Size: {}", entity->type->size);
        std::println("Alignment: {}", entity->type->align);
        std::println("----------------");
    }

    return parser_.lexer.any_errors();
}

// TODO: Replace this with proper evaluator that will evaluate expression of any type
s64 Typer::const_evaluate_integer(Scope *scope, Ast::Expression *expression) {
    switch (expression->kind) {
        using enum Ast::Expression::Kind;

        case BAD: panic("BAD expression");
        case INTEGER_LITERAL: {
            auto integer = expression->as<Ast::IntegerLiteralExpression>();
            return integer->value;
        }
        case CAST_OPERATOR: {
            panic("Not implemented");
        }
        case UNARY_OPERATOR: {
            auto unary = expression->as<Ast::UnaryOperatorExpression>();
            switch (unary->op.type) {
                using enum TokenType;

                case ampersand:
                case star: {
                    error(parser_.lexer, unary,
                          "Not a constant expression");
                    return 0;
                }
                case bang: {
                    error(parser_.lexer, unary, "Expression does not evaluate to integer");
                    return 0;
                }
                case plus: return const_evaluate_integer(scope, unary->right);
                case minus: return -const_evaluate_integer(scope, unary->right);
                case keyword_size_of: {
                    panic("Not implemented");
                    // get_scope_from_expression(scope, unary->right);
                }
                default: panic("Unary operator not handeled");
            }
        }
        case BINARY_OPERATOR: {
            auto binary = expression->as<Ast::BinaryOperatorExpression>();
            switch (binary->op.type) {
                using enum TokenType;

                case equals:
                case not_equals:
                case less:
                case greater:
                case less_equals:
                case greater_equals: {
                    error(parser_.lexer, binary,
                          "Expression does not evaluate to integer");
                    return 0;
                }

                case plus:
                    return const_evaluate_integer(scope, binary->left) +
                           const_evaluate_integer(scope, binary->right);
                case minus:
                    return const_evaluate_integer(scope, binary->left) -
                           const_evaluate_integer(scope, binary->right);
                case star:
                    return const_evaluate_integer(scope, binary->left) *
                           const_evaluate_integer(scope, binary->right);
                case divide:
                    return const_evaluate_integer(scope, binary->left) /
                           const_evaluate_integer(scope, binary->right);
                case modulo:
                    return const_evaluate_integer(scope, binary->left) %
                           const_evaluate_integer(scope, binary->right);
                default: panic("Binary operator not handeled");
            }
        }

        case BOOL_LITERAL:
        case STRING_LITERAL:
        case FLOAT_LITERAL: {
            error(parser_.lexer, expression,
                  "Expression does not evaluate to integer");
            return 0;
        }
        case IDENTIFIER: {
            auto identifier = expression->as<Ast::IdentifierExpression>();
            auto entity = look_up(scope, identifier->identifier);
            if (!entity) {
                return 0;
            }
            if (!(*entity)->is<ConstantEntity>()) {
                error(parser_.lexer, identifier, "{} is not a constant",
                      identifier->identifier->token.value);
                return 0;
            }
            auto constant = (*entity)->as<ConstantEntity>();
            if (!is_integer(constant->type)) {
                error(parser_.lexer, identifier, "Type of {} is not integer",
                      identifier->identifier->token.value);
                return 0;
            }
            return const_evaluate_integer(scope, constant->init_expression);
        }
        case CALL_OPERATOR: {
            error(parser_.lexer, expression,
                  "Procedure call at compile time is not supported");
            return 0;
        }
        case INDEX: {
            panic("Not implemented");
        }
        case SELECTOR: {
            panic("Not implemented");
        }
    }
    assert(false && "Shoud not trigger");
}

void Typer::calculate_size_and_alignment(Type *type) {
    if (has_flag(type->flags, Type::Flags::SIZED)) {
        return;
    }
    type->flags |= Type::Flags::SIZED;

    switch (type->kind) {
        using enum Type::Kind;

        case BAD: panic("BAD type");
        case PROCEDURE: panic("Attempted to calculate size of procedure");
        case STRING: {
            panic("Not implemented");
        }
        case POINTER:
        case INT:
        case ANY:
        case BOOL:
        case FLOAT: {
            panic("Size should be known");
        }
        case ARRAY: {
            auto array_type = type->as<ArrayType>();
            {
                auto count = const_evaluate_integer(
                    array_type->scope, array_type->count_expression);
                if (count <= 0) {
                    error(parser_.lexer, array_type->count_expression,
                        "Size of the array has to be greater than 0, got {}",
                        count);
                    count = 1;
                }
                array_type->count = static_cast<usize>(count);
            }
            calculate_size_and_alignment(array_type->type);
            array_type->size = array_type->count * array_type->type->size;
            array_type->align = array_type->type->align;
            return;
        }
        case STRUCT: {
            auto struct_type = type->as<StructType>();
            u64 alignment = 0;
            u64 size = 0;
            for (auto member : struct_type->members) {
                calculate_size_and_alignment(member->type);
                if (member->type->align > alignment) {
                    alignment = member->type->align;
                } 
                size = round(size, member->type->align);
                size += member->type->size;
            }
            if (size == 0) {
                size = 1;
                alignment = 1;
            } else {
                size = round(size, alignment);
            }
            struct_type->size = size;
            struct_type->align = alignment;
            return;
        }
        case NAMED: {
            auto named_type = type->as<NamedType>();
            calculate_size_and_alignment(named_type->type);
            named_type->size = named_type->type->size;
            named_type->align = named_type->type->align;
            return;
        }
    }
}

std::optional<Entity*> Typer::get_entity_by_ast_type(Ast::Type *ast_type) const {
    auto search = std::ranges::find_if(entities_, [ast_type](Entity *entity) {
        return entity->ast_type == ast_type;
    });
    if (search != entities_.end()) {
        return *search;
    }
    return {};
}

std::optional<NamedTypeEntity *> Typer::look_up_type(Scope *scope, Ast::TypePath path) {
    auto entity = look_up(scope, path);
    if (!entity) {
        return {};
    }
    if ((*entity)->is<NamedTypeEntity>()) {
        return (*entity)->as<NamedTypeEntity>();
    }
    error(parser_.lexer, path, "{} is not a type", type_path_to_string(path));
    return {};
}

std::optional<Entity*> Typer::look_up(Scope *scope, Ast::Identifier *identifier) {
    auto entity = scope->look_up(identifier);
    if (entity) {
        return entity;
    }
    not_declared_error(identifier);
    return {};
}

std::optional<Entity*> Typer::look_up(Scope *scope, Ast::TypePath path) {
    auto entity = scope->look_up(path);
    if (entity) {
        return entity;
    }
    not_declared_error(path);
    return {};
}

void Typer::not_declared_error(Ast::Identifier *identifier) {
    error(parser_.lexer, identifier->token, "{} is not declared", identifier->token.value);
}

void Typer::not_declared_error(Ast::TypePath path) {
    assert(path.size() > 0);
    error(parser_.lexer, path, "{} is not declared",
            type_path_to_string(path));
}

void Typer::redeclaration_error(Entity *old_entity, Entity *new_entity) {
    auto old_declaration = old_entity->declaration;
    auto new_declaration = new_entity->declaration;
    error(parser_.lexer, new_declaration,
            "Redeclaration of '{}'\n    at {}({}:{})",
            new_declaration->identifier->token.value,
            parser_.lexer.file_name(),
            old_declaration->start_token().start.line,
            old_declaration->start_token().start.column);
}

}