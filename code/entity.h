#ifndef ENTITY_H
#define ENTITY_H

#include <optional>

#include "base/down_cast.h"
#include "ast.h"
#include "types.h"

struct Scope;

struct Entity {
    enum class Kind {
        VARIABLE,
        CONSTANT,
        PROCEDURE,
        NAMED_TYPE,
    };

    DEFINE_DOWNCAST_FUNCTIONS_FOR(Entity, kind, KIND);

    constexpr Entity(Kind kind, Scope *scope) : kind{kind}, scope{scope} {
    }

    Kind kind;
    Scope *scope;

    Type *type = nullptr;
    Entity *aliased_of = nullptr;
};

struct EntityVariable : public Entity {
    static constexpr auto KIND = Kind::VARIABLE;

    constexpr EntityVariable(Scope *scope,
                             Ast::VariableDeclaration *declaration,
                             std::optional<Ast::Type *> ast_type,
                             std::optional<Ast::Expression *> init_expression)
        : Entity{KIND, scope}, declaration{declaration}, ast_type{ast_type},
          init_expression{init_expression} {
    }

    Ast::VariableDeclaration *declaration;
    std::optional<Ast::Type *> ast_type;
    std::optional<Ast::Expression *> init_expression;
};

struct EntityConstant : public Entity {
    static constexpr auto KIND = Kind::CONSTANT;

    constexpr EntityConstant(Scope *scope, Ast::ConstDeclaration *declaration,
                             std::optional<Ast::Type *> ast_type,
                             Ast::Expression *init_expression)
        : Entity{KIND, scope}, declaration{declaration}, ast_type{ast_type},
          init_expression{init_expression} {
    }

    Ast::ConstDeclaration *declaration;
    std::optional<Ast::Type *> ast_type;
    Ast::Expression *init_expression;
};

struct EntityProcedure : public Entity {
    static constexpr auto KIND = Kind::PROCEDURE;

    constexpr EntityProcedure(Scope *scope,
                              Ast::ProcedureDeclaration *declaration,
                              Ast::TypeProcedure *ast_type)
        : Entity{KIND, scope}, declaration{declaration}, ast_type{ast_type} {
    }

    Ast::ProcedureDeclaration *declaration;
    Ast::TypeProcedure *ast_type;
};

struct EntityNamedType : public Entity {
    static constexpr auto KIND = Kind::NAMED_TYPE;

    constexpr EntityNamedType(Scope *scope, Ast::TypeDeclaration *declaration,
                              Ast::Type *ast_type)
        : Entity{KIND, scope}, declaration{declaration}, ast_type{ast_type} {
    }

    Ast::TypeDeclaration *declaration;
    Ast::Type *ast_type;

    std::optional<Scope *> inner_scope;
};

#endif