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

    bool is_loop = false;

    Maybe<Entity *> look_up_current(std::string_view name) const;
    Maybe<Entity *> look_up(std::string_view name) const;
    
    Maybe<Entity *> look_up_current(const Ast::Identifier *identifier) const;
    Maybe<Entity *> look_up(const Ast::Identifier *identifier) const;
};

struct TyperContext {
    Entity *entity = nullptr;
    Scope *scope = nullptr;
    std::vector<const Entity *> *decl_path = nullptr;
};

struct Operand {
    enum class Kind {
        INVALID,
        NO_VALUE,
        VALUE,
        CONSTANT,
        VARIABLE,
        TYPE,
    };

    Kind kind = Kind::INVALID;
    Typing::Type *type = bad_t;
    Value value;
    Ast::Expression *expr = nullptr;
};

inline void set_type_and_value(Ast::Expression *expression, Operand &operand) {
    expression->type = operand.type;
    expression->value = operand.value;
    operand.expr = expression;
}

struct Typer {
    constexpr Typer(Ast::Parser &parser, Allocator allocator)
        : entities_storage_{allocator}, scopes_storage_{allocator},
          parser_{parser}, file_scope_{create_scope(nullptr)}, reporter{parser.reporter} {
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
    
    bool do_typing();

    Scope *create_scope(Scope *parent);

    bool add_entity(Scope *scope, Entity *entity);
    bool add_entity(Scope *scope, Entity *entity, std::string_view name);

    void open_scope(TyperContext &context, Scope *&out_scope);
    void close_scope(TyperContext &context);

    void collect_entities(TyperContext &context, std::span<Ast::Statement *> statements);
    void collect_entities(TyperContext &context, std::span<Ast::DeclarationStatement *> statements);
    Entity *collect_entity(TyperContext &context, Ast::Declaration *declaration);
    VariableEntity *create_entity_variable(TyperContext &context, Ast::VariableDeclaration *ast_variable);

    void not_declared_error(const Ast::Identifier *identifier);
    void redeclaration_error(const Entity *old_entity, const Entity *new_entity);

    bool check_cycle(TyperContext &context, const Entity *entity);

    void check_entity_decl(TyperContext &context, Entity *entity);
    void check_variable_decl(TyperContext &context, VariableEntity *entity);
    void check_constant_decl(TyperContext &context, ConstantEntity *entity);
    void check_proc_decl(TyperContext &context, ProcedureEntity *entity);
    void check_type_decl(TyperContext &context, NamedTypeEntity *entity);

    Type *check_type(TyperContext &context, Ast::Type *type);

    Maybe<Entity *> check_identifier(TyperContext &context, Operand &operand, Ast::Identifier *identifier);
    Maybe<Entity *> check_selector(TyperContext &context, Operand &operand, Ast::SelectorExpression *selector);
    Maybe<Entity*> check_identifier_or_selector(TyperContext &context, Operand &operand, Ast::Expression *expression);
    Maybe<Entity *> lookup_field(Type *type, Ast::Identifier *identifier, bool is_type);

    bool check_representable_as_constant(Value &value, Type *to, Ast::Expression *expression);

    void check_expr_internal(TyperContext &context, Operand &operand, Ast::Expression *expression, Type *type_hint);
    void check_expr(TyperContext &context, Operand &operand, Ast::Expression *expression, Type *type_hint = nullptr);
    void check_unary_expr(TyperContext &context, Operand &operand, Ast::UnaryOperatorExpression *expression);
    void check_binary_expr(TyperContext &context, Operand &operand, Ast::BinaryOperatorExpression *expression);
    void check_deref_expr(TyperContext &context, Operand &operand, Ast::DerefExpression *expression);
    void check_index_expr(TyperContext &context, Operand &operand, Ast::IndexExpression *expression);
    void check_call_expr(TyperContext &context, Operand &operand, Ast::CallOperatorExpression *expression);
    void check_cast_expr(TyperContext &context, Operand &operand, Ast::CastOperatorExpression *expression);
    void check_compound_expr(TyperContext &context, Operand &operand, Ast::CompoundExpression *expression, Type *type_hint);
    void check_slice_expr(TyperContext &context, Operand &operand, Ast::SliceExpression *expression);

    bool check_unary_operator(TyperContext &context, const Operand &operand, const Token &token);
    bool check_binary_operator(TyperContext &context, const Operand &left, const Operand &right, const Token &token);

    u64 check_array_count(TyperContext &context, Operand &operand, Ast::Expression *expression);
    
    void check_init_variable(TyperContext &context, const Operand &operand, VariableEntity *entity);
    void check_init_constant(TyperContext &context, const Operand &operand, ConstantEntity *entity);

    bool check_assignment(TyperContext &context, const Operand &operand, Type *type);
    void check_procedure_body(TyperContext &context, ProcedureEntity *proc);
    void collect_and_check_local_entities(TyperContext &context, std::span<Ast::Statement *> statements);
    void check_statement(TyperContext &context, Ast::Statement *statement, Scope *block_scope = nullptr);
private:
    DynamicArena entities_storage_;
    DynamicArena scopes_storage_;

    Scope *file_scope_;
    
    Ast::Parser &parser_;
    std::vector<Entity*> entities_;

    std::vector<ProcedureEntity*> procedure_bodies_to_check_;

public:
    Reporter &reporter;
};

}

#endif