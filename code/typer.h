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
#include "base/util.h"
#include "parser.h"
#include "entity.h"
#include "ast.h"

namespace Typing {

struct Scope {
    constexpr Scope(StdAllocator<Entity*> allocator) : entities{allocator} {
    }

    Maybe<Scope *> parent;
    Maybe<Entity *> entity;
    AllocatorUnorderedMap<std::string_view, Entity *> entities;

    Maybe<Entity *> look_up(const Ast::Identifier *identifier) const;
    Maybe<Entity*> look_up(Ast::TypePath path) const;
    
    AllocatorString full_name() const;
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
        return entities_storage_.new_object<T>(std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    T *create_type(Args &&...args)
        requires std::derived_from<T, Type> && TriviallyDestructible<T>
    {
        return entities_storage_.new_object<T>(std::forward<Args>(args)...);
    }

    template <typename T>
    std::span<T> create_array(std::span<T> span)
        requires TriviallyDestructible<T>
    {
        auto array = entities_storage_.allocate<T>(span.size());
        for (auto i : indices(span.size())) {
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
    bool collect_entity(Scope *scope, Ast::Declaration *declaration, bool local);
    // Used to collect entities form local statements
    void collect_entities_from_statement(Scope *scope, Ast::Statement *statement, Scope *block_scope = nullptr);

    bool do_typing();

    s64 const_evaluate_integer(const Scope *scope, const Ast::Expression *expression);

    void calculate_size_and_alignment(Type *type);

    Maybe<Entity*> get_entity_by_ast_type(const Ast::Type *ast_type) const;
    Maybe<NamedTypeEntity *> look_up_type(const Scope *scope, Ast::TypePath path);
    Maybe<Entity*> look_up(const Scope *scope, Ast::TypePath path);
    Maybe<Entity*> look_up(const Scope *scope, const Ast::Identifier *identifier);

    void not_declared_error(Ast::TypePath path);
    void not_declared_error(const Ast::Identifier *identifier);
    void redeclaration_error(const Entity *old_entity, const Entity *new_entity);

private:
    DynamicArena entities_storage_;
    DynamicArena scopes_storage_;
    // Scopes use memory from scopes_storage_ for allocations
    // And are stored in the same arena themselfs
    // So dont have to destroy them
    std::vector<Scope*> scopes_;

    // Pointer to first scope in scopes_
    Scope *file_scope;
    
    Ast::Parser &parser_;
    std::vector<Entity*> entities_;
};

}

#endif