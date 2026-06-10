#ifndef TYPER_H
#define TYPER_H

#include <vector>
#include <concepts>
#include <span>
#include <utility>

#include "base/allocator.h"
#include "base/arena.h"
#include "base/concepts.h"
#include "parser.h"
#include "entity.h"
#include "ast.h"

struct Scope {
    constexpr Scope(Scope *parent) : parent{parent} {
    }
    
    Scope *parent;
    std::vector<Entity*> entities;
};

struct Typer {
    Typer(Ast::Parser &parser, Allocator allocator)
        : entities_storage_{allocator}, scopes_storage_{allocator},
          parser_{parser}, file_scope{create_scope(nullptr)} {
    }

    template <typename T, typename... Args>
    T *create_entity(Args &&...args)
        requires std::derived_from<T, Entity> && TriviallyDestructible<T>
    {
        return entities_storage_.create<T>(std::forward<Args>(args)...);
    };

    Scope *create_scope(Scope *parent) {
        scopes_.push_back(make_allocator_unique<Scope>(scopes_storage_, parent));
        return scopes_.back().get();
    }

    void add_entity(Scope *scope, Entity *entity) {
        entities_.push_back(entity);
        scope->entities.push_back(entity);
    }

    void collect_entities(Scope *scope, std::span<Ast::DeclarationStatement *> declarations);
    void collect_entities(Scope *scope, std::span<Ast::Statement *> statements);
    void collect_entity(Scope *scope, Ast::Declaration *declaration);

    bool do_typing();

private:
    DynamicArena entities_storage_;
    DynamicArena scopes_storage_;
    
    Ast::Parser &parser_;
    std::vector<Entity*> entities_;
    std::vector<AllocatorUniquePtr<Scope, DynamicArena>> scopes_;
    
    Scope *file_scope;
};

#endif