#ifndef TYPER_H
#define TYPER_H

#include <vector>

#include "base/arena.h"
#include "parser.h"
#include "entity.h"

struct Scope {
    Scope *parent = nullptr;
    std::vector<Entity*> entities;
};

struct Typer {
    Typer(Ast::Parser &parser, Allocator allocator)
        : entities_storage_{allocator}, scopes_storage_{allocator},
          parser_{parser}, file_scope{create_scope(nullptr)} {
    }

    template <std::derived_from<Entity> T, typename... Args>
    T *create_entity(Args &&...args) {
        return entities_storage_.create<T>(std::forward<Args>(args)...);
    };

    Scope *create_scope(Scope *parent) {
        // TODO: Scopes desctructors are not executed for now
        auto new_scope = scopes_storage_.create<Scope>();
        new_scope->parent = parent;
        return new_scope;
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
    
    Scope *file_scope;
};

#endif