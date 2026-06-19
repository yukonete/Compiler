#ifndef TYPER_H
#define TYPER_H

#include <vector>
#include <concepts>
#include <span>
#include <utility>
#include <algorithm>
#include <cassert>
#include <string_view>

#include "base/panic.h"
#include "base/allocator.h"
#include "base/arena.h"
#include "base/concepts.h"
#include "parser.h"
#include "entity.h"
#include "ast.h"

namespace Typing {

struct Scope {
    std::optional<Scope *> parent;
    std::optional<Entity *> entity;
    std::unordered_map<std::string_view, Entity*> entities;

    std::optional<Entity*> look_up(Ast::Identifier *identifier) const;
    std::optional<Entity*> look_up(Ast::TypePath path) const;
    
    std::string full_name() const;
};

struct Typer {
    constexpr Typer(Ast::Parser &parser, Allocator allocator)
        : entities_storage_{allocator}, scopes_storage_{allocator},
          parser_{parser}, file_scope{create_scope(nullptr)} {
    }

    template <typename T, typename... Args>
    T *create_entity(Args &&...args)
        requires std::derived_from<T, Entity> && TriviallyDestructible<T>
    {
        return entities_storage_.create<T>(std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    T *create_type(Args &&...args)
        requires std::derived_from<T, Type> && TriviallyDestructible<T>
    {
        return entities_storage_.create<T>(std::forward<Args>(args)...);
    }

    template <typename T>
    std::span<T> create_array(std::span<T> span)
        requires TriviallyDestructible<T>
    {
        auto array = entities_storage_.allocate<T>(span.size());
        for (usize i = 0; i < span.size(); ++i) {
            array[i] = span[i];
        }
        return std::span{array, span.size()};
    }

    Scope *create_scope(Scope *parent);

    bool add_entity(Scope *scope, Entity *entity);
    bool add_entity(Scope *scope, Entity *entity, std::string_view name);
    void resolve_entity(Entity *entity);
    Type *ast_type_to_type(Scope *scope, Ast::Type *ast_type);
    void resolve_alias(NamedTypeEntity *entity);

    bool collect_entities(Scope *scope, std::span<Ast::DeclarationStatement *> declarations);
    bool collect_entities(Scope *scope, std::span<Ast::Statement *> statements);
    bool collect_entity(Scope *scope, Ast::Declaration *declaration,
                        bool local);
    void collect_entities_from_statement(Scope *scope,
                                         Ast::Statement *statement);

    bool check_for_recursive_type(Scope *scope, const Type *type,
                                  std::vector<const Entity *> &path);
    bool check_for_recursive_declaration(const Entity *entity,
                                         std::vector<const Entity *> &path);
    bool check_for_recursive_expression(Scope *scope,
                                        Ast::Expression *expression,
                                        std::vector<const Entity *> &path);
    bool check_for_recursive_statement(Scope *scope, Ast::Statement *statement,
                                       std::vector<const Entity *> &path);

    bool do_typing();

    s64 const_evaluate_integer(Scope *scope, Ast::Expression *expression);

    void calculate_size_and_alignment(Type *type);

    std::optional<Entity*> get_entity_by_ast_type(Ast::Type *ast_type) const;
    std::optional<NamedTypeEntity *> look_up_type(Scope *scope, Ast::TypePath path);
    std::optional<Entity*> look_up(Scope *scope, Ast::TypePath path);
    std::optional<Entity*> look_up(Scope *scope, Ast::Identifier *identifier);

    void not_declared_error(Ast::TypePath path);
    void not_declared_error(Ast::Identifier *identifier);
    void redeclaration_error(Entity *old_entity, Entity *new_entity);

private:
    DynamicArena entities_storage_;
    DynamicArena scopes_storage_;
    std::vector<AllocatorUniquePtr<Scope, DynamicArena>> scopes_;
    
    Ast::Parser &parser_;
    std::vector<Entity*> entities_;
    std::vector<ArrayType*> arrays_without_size_;

    // Maybe it is better just store scopes directly in Ast::BlockStatements instead?
    std::unordered_map<Ast::BlockStatement *, Scope *> block_scopes_;

    Scope *file_scope;
};

}

#endif