#ifndef ENTITY_H
#define ENTITY_H

#include "base/down_cast.h"
#include "base/flags.h"
#include "ast.h"
#include "types.h"

namespace Typing {

struct Scope;

struct Entity {
    enum class Kind : u8 {
        VARIABLE,
        CONSTANT,
        PROCEDURE,
        NAMED_TYPE,
    };

    enum class State : u8 {
        UNRESOLVED,
        IN_PROGRESS,
        RESOLVED,
    };

    DEFINE_DOWNCAST_FUNCTIONS_FOR(Entity, kind, KIND);

    constexpr Entity(Kind kind, Scope *scope, Ast::Declaration *declaration,
                     Ast::Type *ast_type)
        : kind{kind}, scope{scope}, declaration{declaration}, ast_type{ast_type} {
    }

    std::string_view name() const;
    std::string full_name() const;

    Kind kind;
    State state = State::UNRESOLVED;
    Scope *scope;

    Maybe<Entity *> parent;

    // Those are nullptr for builtin types
    Ast::Declaration *declaration;
    Ast::Type *ast_type;

    // NamedType or BasicType for NamedTypeEntity
    Type *type = nullptr;
    Entity *aliased_of = nullptr;
};

struct VariableEntity : public Entity {
    static constexpr auto KIND = Kind::VARIABLE;

    constexpr VariableEntity(Scope *scope,
                             Ast::Declaration *declaration,
                             Ast::Type *ast_type,
                             Maybe<Ast::Expression *> init_expression)
        : Entity{KIND, scope, declaration, ast_type},
          init_expression{init_expression} {
    }

    Maybe<Ast::Expression *> init_expression;
    bool is_global = false;
};

struct ConstantEntity : public Entity {
    static constexpr auto KIND = Kind::CONSTANT;

    constexpr ConstantEntity(Scope *scope, Ast::ConstDeclaration *declaration,
                             Ast::Type *ast_type,
                             Ast::Expression *init_expression)
        : Entity{KIND, scope, declaration, ast_type},
          init_expression{init_expression} {
    }

    Value value;
    Ast::Expression *init_expression;
};

struct ProcedureEntity : public Entity {
    static constexpr auto KIND = Kind::PROCEDURE;

    constexpr ProcedureEntity(Scope *scope,
                              Ast::ProcedureDeclaration *declaration,
                              Ast::ProcedureType *ast_type, Scope *inner_scope)
        : Entity{KIND, scope, declaration, ast_type}, inner_scope{inner_scope} {
    }

    Scope *inner_scope;
};

struct NamedTypeEntity : public Entity {
    static constexpr auto KIND = Kind::NAMED_TYPE;

    constexpr NamedTypeEntity(Scope *scope, Ast::TypeDeclaration *declaration,
                              Ast::Type *ast_type)
        : Entity{KIND, scope, declaration, ast_type} {
    }

    bool is_alias = false;
};

} // namespace Typing

#endif