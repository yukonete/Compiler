#ifndef ENTITY_H
#define ENTITY_H

#include <optional>

#include "base/down_cast.h"
#include "base/flags.h"
#include "ast.h"
#include "types.h"

struct Scope;

struct Entity {
    enum class Kind : u8 {
        VARIABLE,
        CONSTANT,
        PROCEDURE,
        NAMED_TYPE,
    };

    enum class Flags : u8 {
        NONE,
        RESOLVED = 1<<0,
        BUILTIN = 1<<1,
    };

    DEFINE_DOWNCAST_FUNCTIONS_FOR(Entity, kind, KIND);

    constexpr Entity(Kind kind, Scope *scope, Ast::Declaration *declaration,
                     Ast::Type *ast_type)
        : kind{kind}, scope{scope}, declaration{declaration}, ast_type{ast_type} {
    }

    Kind kind;
    Flags flags = Flags::NONE;
    Scope *scope;

    // Those are nullptr for builtin types
    Ast::Declaration *declaration;
    Ast::Type *ast_type;

    Type *type = nullptr; // NamedType for NamedTypeEntity
    Entity *aliased_of = nullptr;
};

DEFINE_ENUM_FLAG_OPERATORS(Entity::Flags);

struct VariableEntity : public Entity {
    static constexpr auto KIND = Kind::VARIABLE;

    constexpr VariableEntity(Scope *scope,
                             Ast::VariableDeclaration *declaration,
                             Ast::Type *ast_type,
                             std::optional<Ast::Expression *> init_expression)
        : Entity{KIND, scope, declaration, ast_type},
          init_expression{init_expression} {
    }

    std::optional<Ast::Expression *> init_expression;
};

struct ConstantEntity : public Entity {
    static constexpr auto KIND = Kind::CONSTANT;

    constexpr ConstantEntity(Scope *scope, Ast::ConstDeclaration *declaration,
                             Ast::Type *ast_type,
                             Ast::Expression *init_expression)
        : Entity{KIND, scope, declaration, ast_type},
          init_expression{init_expression} {
    }

    Ast::Expression *init_expression;
};

struct ProcedureEntity : public Entity {
    static constexpr auto KIND = Kind::PROCEDURE;

    constexpr ProcedureEntity(Scope *scope,
                              Ast::ProcedureDeclaration *declaration,
                              Ast::ProcedureType *ast_type)
        : Entity{KIND, scope, declaration, ast_type} {
    }

    Scope *inner_scope = nullptr;
};

struct NamedTypeEntity : public Entity {
    static constexpr auto KIND = Kind::NAMED_TYPE;

    constexpr NamedTypeEntity(Scope *scope, Ast::TypeDeclaration *declaration,
                              Ast::Type *ast_type, std::optional<Scope *> inner_scope)
        : Entity{KIND, scope, declaration, ast_type}, inner_scope{inner_scope} {
    }

    // Set olny for structs
    std::optional<Scope *> inner_scope;
};

#endif