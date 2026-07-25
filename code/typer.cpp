#include <ranges>
#include <string_view>
#include <unordered_set>

#include "base/arena.h"
#include "base/tformat.h"
#include "base/panic.h"
#include "typer.h"
#include "ast.h"
#include "error.h"

namespace Typing {

static bool check_representable_as_constant_internal(Value &value, Type *to) {
    if (value.kind == Value::Kind::INVALID) {
        return false;
    }

    if (!to->is_basic()) {
        return false;
    }

    auto type = to->get_base_type()->as<BasicType>();
    if (type == bad_t) {
        return false;
    }
    if (type == bool_t) {
        return value.kind == Value::Kind::BOOL;
    }
    if (type->is_integer()) {
        auto v = value.as_int();
        if (v.kind == Value::Kind::INVALID) {
            return false;
        }
        value = v;
           
        s64 min = 0;
        s64 max = 0;
        if (type == s8_t) {
            min = std::numeric_limits<s8>::min();
            max = std::numeric_limits<s8>::max();
        } else if (type == s16_t) {
            min = std::numeric_limits<s16>::min();
            max = std::numeric_limits<s16>::max();
        } else if (type == s32_t) {
            min = std::numeric_limits<s32>::min();
            max = std::numeric_limits<s32>::max();
        } else if (type == s64_t) {
            min = std::numeric_limits<s64>::min();
            max = std::numeric_limits<s64>::max();
        } else if (type == int_t) {
            min = std::numeric_limits<s64>::min();
            max = std::numeric_limits<s64>::max();
        } else if (type == u8_t) {
            min = static_cast<s64>(std::numeric_limits<u8>::min());
            max = static_cast<s64>(std::numeric_limits<u8>::max());
        } else if (type == u16_t) {
            min = static_cast<s64>(std::numeric_limits<u16>::min());
            max = static_cast<s64>(std::numeric_limits<u16>::max());
        } else if (type == u32_t) {
            min = static_cast<s64>(std::numeric_limits<u32>::min());
            max = static_cast<s64>(std::numeric_limits<u32>::max());
        } else if (type == u64_t) {
            // temporary
            return false;
        } else if (type == uint_t) {
            // temporary
            return false;
        }
        
        return min <= value.int_value && value.int_value <= max;
    }
    if (type->is_float()) {
        auto v = value.as_float();
        if (v.kind == Value::Kind::INVALID) {
            return false;
        }
        value = v;
        
        return true;
    }
    return false;
}

bool Typer::check_representable_as_constant(Value &value, Type *to, Ast::Expression *expression) {
    if (!check_representable_as_constant_internal(value, to)) {
        error(reporter, expression, "Value is not representable as type {}", type_to_string(to));  
        return false;
    }
    return true;
}

void Typer::open_scope(TyperContext &context, Ast::StructType *ast_struct) {
    auto scope = create_scope(context.scope);
    scope->entity = context.entity;
    
    ast_struct->scope = scope;
    context.scope = scope;
}

void Typer::close_scope(TyperContext &context) {
    context.scope = *context.scope->parent;
}

// resulting string is stored in temp_allocator
static AllocatorString full_entity_name(const Entity *entity) {
    std::string_view entity_name;
    if (has_flag(entity->flags, Entity::Flags::BUILTIN)) {
        entity_name = entity->type->as<NamedType>()->name;
    } else {
        entity_name = entity->declaration->identifier->token.value;
    }

    auto scope_name = entity->scope->full_name();
    if (scope_name == "") {
        return tformat("{}", entity_name);
    }
    return tformat("{}.{}", entity->scope->full_name(), entity_name);
}

// resulting string is stored in temp_allocator
AllocatorString Scope::full_name() const {
    if (!entity || !entity->is<NamedTypeEntity>()) {
        return tformat("");
    }

    auto named_type = entity->type->as<NamedType>();
    auto parent_scope_name =
        parent
            .expect("since entity is set and it is NamedType, there should be "
                    "parent scope")
            ->full_name();
    if (parent_scope_name == "") {
        return tformat("{}", named_type->name);
    }

    return tformat("{}.{}", parent_scope_name, named_type->name);
}

Maybe<Entity *> Scope::look_up_current(std::string_view name) const {
    auto search = entities.find(name);
    if (search != entities.end()) {
        return search->second;
    }
    return {};
}

Maybe<Entity *> Scope::look_up(std::string_view name) const {
    auto e = look_up_current(name);
    if (!e && parent) {
        return parent->look_up(name);
    }
    return e;
}

Maybe<Entity *> Scope::look_up_current(const Ast::Identifier *identifier) const {
    return look_up_current(identifier->token.value);
}

Maybe<Entity *> Scope::look_up(const Ast::Identifier *identifier) const {
    return look_up(identifier->token.value);
}

bool Typer::add_entity(Scope *scope, Entity *entity) {
    return add_entity(scope, entity, entity->declaration->identifier->token.value);
}

bool Typer::add_entity(Scope *scope, Entity *entity, std::string_view name) {
    // TODO: might want to produce warning about shadowing here
    auto old_entity = scope->look_up_current(name);
    if (old_entity) {
        redeclaration_error(*old_entity, entity);
        return false;
    }

    entities_.push_back(entity);
    scope->entities[name] = entity;
    return true;
}

Scope *Typer::create_scope(Scope *parent) {
    auto allocator = scopes_storage_.create_allocator();
    auto scope = scopes_storage_.new_object<Scope>(allocator);
    if (parent != nullptr) {
        scope->parent = parent;
    }
    return scope;
}

bool Typer::collect_entity(TyperContext &context, Ast::Declaration *declaration) {
    if ((declaration->flags & Ast::Declaration::Flags::HANDLED) ==
        Ast::Declaration::Flags::HANDLED) {
        return false;
    }
    declaration->flags |= Ast::Declaration::Flags::HANDLED;

    switch (declaration->kind) {
        using enum Ast::Declaration::Kind;

        case VARIABLE: {
            auto ast_variable = declaration->as<Ast::VariableDeclaration>();
            Ast::Type *type = nullptr;
            if (ast_variable->type) {
                type = *ast_variable->type;
            }
            auto entity = create_entity<VariableEntity>(context.scope, ast_variable, type, ast_variable->value);
            declaration->entity = entity;
            add_entity(context.scope, entity);
            return true;
        }

        case CONSTANT: {
            auto ast_constant = declaration->as<Ast::ConstDeclaration>();
            Ast::Type *type = nullptr;
            if (ast_constant->type) {
                type = *ast_constant->type;
            }
            auto entity = create_entity<ConstantEntity>(context.scope, ast_constant, type, ast_constant->value);
            declaration->entity = entity;
            add_entity(context.scope, entity);
            return true;
        }

        case PROCEDURE: {
            auto ast_proc = declaration->as<Ast::ProcedureDeclaration>();
            auto entity =
                create_entity<ProcedureEntity>(context.scope, ast_proc, ast_proc->type, create_scope(context.scope));
            declaration->entity = entity;
            add_entity(context.scope, entity);
            return true;
        }

        case TYPE: {
            auto ast_type = declaration->as<Ast::TypeDeclaration>();
            auto entity = create_entity<NamedTypeEntity>(context.scope, ast_type, ast_type->type);
            declaration->entity = entity;
            add_entity(context.scope, entity);
            return true;
        }

        case FIELD: panic("Should be caught in parser");
    }
    panic("Ast::Declaration is not handled");
}

bool Typer::collect_entities(TyperContext &context, std::span<Ast::Statement *> statements) {
    auto collected = false; 
    for (auto statement : statements) {
        if (statement->is<Ast::DeclarationStatement>()) {
            collected |= collect_entity(context, statement->as<Ast::DeclarationStatement>()->declaration);
        } else {
            error(reporter, statement, "Expected declaration");
        }
    }
    return collected;
}

bool Typer::collect_entities(TyperContext &context, std::span<Ast::DeclarationStatement *> statements) {
    auto collected = false; 
    for (auto statement : statements) {
        collected |= collect_entity(context, statement->as<Ast::DeclarationStatement>()->declaration);
    }
    return collected;
}

void Typer::not_declared_error(const Ast::Identifier *identifier) {
    error(reporter, identifier->token, "{} is not declared", identifier->token.value);
}

void Typer::redeclaration_error(const Entity *old_entity, const Entity *new_entity) {
    auto old_declaration = old_entity->declaration;
    auto new_declaration = new_entity->declaration;
    auto old_declaration_token = old_declaration->start_token();
    error(reporter, new_declaration, "Redeclaration of '{}'\n    at {}",
          new_declaration->identifier->token.value,
          parser_.lexer.token_to_location_string(old_declaration_token));
}

bool Typer::check_cycle(TyperContext &context, const Entity *entity) {
    auto path = std::span{*context.decl_path};
    for (auto i : indices(path.size())) {
        if (path[i] == entity) {
            auto entity_name = full_entity_name(entity);
            if (i == path.size() - 1) {
                error(reporter, entity->declaration, "Declaration cycle of '{}': '{}' refers to itself", entity_name,
                      entity_name);
            } else {
                error(reporter, entity->declaration, "Declaration cycle of '{}'", entity_name);
                for (usize j = i; j < path.size() - 1; ++j) {
                    error(reporter, path[j]->declaration, "'{}' refers to '{}'", full_entity_name(path[j]),
                          full_entity_name(path[j + 1]));
                }
                error(reporter, path[path.size() - 1]->declaration, "'{}' refers to '{}'", full_entity_name(path[path.size() - 1]),
                      entity_name);
            }
            return true;
        }
    }
    return false;
}

Maybe<Entity *> Typer::check_identifier(TyperContext &context, Operand &operand, Ast::Identifier *identifier) {
    auto entity = context.scope->look_up(identifier);
    if (!entity) {
        not_declared_error(identifier);
        return {};
    }

    if (entity->state == Entity::State::UNRESOLVED) {
        check_entity_decl(context, *entity);
    }

    switch (entity->kind) {
        using enum Entity::Kind;

        case CONSTANT:
        case VARIABLE:
        case NAMED_TYPE: {
            if (check_cycle(context, *entity)) {
                assert(operand.kind == Operand::Kind::INVALID);
                return entity;
            }
            break;
        }
        default: break;
    }

    if (entity->type->is_bad()) {
        return entity;
    }

    switch (entity->kind) {
        using enum Entity::Kind;
        
        case CONSTANT: {
            auto constant = entity->as<ConstantEntity>();
            operand.kind = Operand::Kind::CONSTANT;
            operand.value = constant->value;
            break;
        }
        case VARIABLE: {
            operand.kind = Operand::Kind::VARIABLE;
            break;
        }
        case NAMED_TYPE: {
            operand.kind = Operand::Kind::TYPE;
            break;
        }
        case PROCEDURE: {
            operand.kind = Operand::Kind::VALUE;
            break;
        }
    }

    identifier->entity = *entity;
    operand.type = entity->type;
    return entity;
}

void Typer::check_expr(TyperContext &context, Operand &operand, Ast::Expression *expression, Type *type_hint) {
    check_expr_internal(context, operand, expression, type_hint);
    if (operand.kind == Operand::Kind::TYPE) {
        error(reporter, expression, "Expected expression, got type");
        operand = Operand{};
    }
}

void Typer::check_expr_internal(TyperContext &context, Operand &operand, Ast::Expression *expression, Type *type_hint) {
    switch (expression->kind) {
        using enum Ast::Expression::Kind;

        case BAD: {
            set_type_and_value(expression, operand);
            return;
        }

        case BOOL_LITERAL: {
            auto literal = expression->as<Ast::BoolLiteralExpression>();
            operand.kind = Operand::Kind::CONSTANT;
            operand.type = bool_t;
            operand.value = create_value_bool(literal->token.type == TokenType::keyword_true);
            set_type_and_value(expression, operand);
            return;
        }

        case INTEGER_LITERAL: {
            auto parse_s64 = [](std::string_view str) -> Maybe<s64> {
                u64 value = 0;
                for (auto digit : str) {
                    auto digit_value = static_cast<u8>(digit - '0');
                    auto old_value = value;
                    value = value * 10 + digit_value;
                    if (value < old_value || value > static_cast<u64>(std::numeric_limits<s64>::max())) {
                        // Overflowed
                        return {};
                    }
                }
                return static_cast<s64>(value);
            };

            auto literal = expression->as<Ast::IntegerLiteralExpression>();
            operand.kind = Operand::Kind::CONSTANT;
            auto value = parse_s64(literal->token.value);
            if (value) {
                operand.type = int_t;
                operand.value = create_value_int(*value);
            } else {
                error(reporter, literal->token, "Literal '{}' is bigger than max signed 64-bit integer, those are not supported for now",
                      literal->token.value);
            }
            set_type_and_value(expression, operand);
            return;
        }

        case FLOAT_LITERAL: {
             auto parse_f64 = [](std::string_view str) -> f64 {
                // TODO: Figure out how this approach affects precision
                f64 integer_part = 0;
                f64 decimal_part = 0;
                f64 decimal_divider = 1;
                bool encountered_dot = false;
                for (auto digit : str) {
                    if (digit == '.') {
                        encountered_dot = true;
                        continue;
                    }
                    auto digit_value = digit - '0';
                    if (!encountered_dot) {
                        integer_part = integer_part * 10 + digit_value;
                    } else {
                        decimal_divider *= 10;
                        decimal_part = decimal_part * 10 + digit_value;
                    }
                }
                return integer_part + decimal_part / decimal_divider;
            };

            auto literal = expression->as<Ast::FloatLiteralExpression>();
            operand.kind = Operand::Kind::CONSTANT;
            operand.type = f64_t;
            operand.value = create_value_float(parse_f64(literal->token.value)); 
            set_type_and_value(expression, operand);
            return;
        }

        case STRING_LITERAL: {
            auto literal = expression->as<Ast::StringLiteralExpression>();
            operand.kind = Operand::Kind::CONSTANT;
            operand.type = string_t;
            operand.value = create_value_string(literal->token.value);
            set_type_and_value(expression, operand);
            return;
        }
        
        case IDENTIFIER: {
            auto identifier = expression->as<Ast::IdentifierExpression>();
            check_identifier(context, operand, identifier->identifier);
            set_type_and_value(expression, operand);
            return;
        }

        case SELECTOR: {
            auto selector = expression->as<Ast::SelectorExpression>();
            check_selector(context, operand, selector);
            return;
        }

        case UNARY_OPERATOR: {
            auto unary = expression->as<Ast::UnaryOperatorExpression>();
            check_unary_expr(context, operand, unary);
            return;
        }
        
        case BINARY_OPERATOR: {
            auto binary = expression->as<Ast::BinaryOperatorExpression>();
            check_binary_expr(context, operand, binary);
            return;
        }
        
        case TYPE: {
            auto type_expression = expression->as<Ast::TypeExpression>();
            operand.kind = Operand::Kind::TYPE;
            operand.type = check_type(context, type_expression->type);
            set_type_and_value(expression, operand);
            return;
        }

        case DEREF: {
            auto deref = expression->as<Ast::DerefExpression>();
            check_deref_expr(context, operand, deref); 
            return;
        }

        case INDEX: {
            auto index = expression->as<Ast::IndexExpression>();
            check_index_expr(context, operand, index);
            return;
        }
        
        case CALL_OPERATOR: {
            auto call = expression->as<Ast::CallOperatorExpression>();
            check_call_expr(context, operand, call);
            return;
        } 
        
        case CAST_OPERATOR: {
            auto cast = expression->as<Ast::CastOperatorExpression>();
            check_cast_expr(context, operand, cast);
            return;
        }

        case COMPOUND: {
            auto compound = expression->as<Ast::CompoundExpression>();
            check_compound_expr(context, operand, compound, type_hint);
            return;
        }

        case SLICE: {
            auto slice = expression->as<Ast::SliceExpression>();
            check_slice_expr(context, operand, slice);
            return;
        }
    }
} 

bool Typer::check_unary_operator(TyperContext &context, const Operand &operand, const Token &token) {
    if (operand.kind == Operand::Kind::INVALID) {
        return false;
    }
    
    if (operand.kind == Operand::Kind::TYPE) {
        error(reporter, operand.expr, "Expected expression, got type");
        return false;
    }

    switch (token.type) {
        using enum TokenType;

        case minus:
        case plus: {
            if (operand.type->is_numeric()) {
                if (token.type == minus && operand.type->is_unsigned()) {
                    error(reporter, token, "Operator '{}' is not allowed with unsigned types", token.type);
                    return false;
                }
                return true;
            }
            error(reporter, token, "Operator '{}' is not allowed with {}", token.type,
                  Ast::expression_to_string(operand.expr));
            return false;
        }

        case bang: {
            if (operand.type->is_bool()) {
                return true;
            }
            error(reporter, token, "Operator '{}' is only allowed wih booleans", token.type);
            return false;
        }
        default: panic("Unary operator not handled");
    }
}

static Value apply_unary_operator(const Value &value, TokenType op) {
    if (value.kind == Value::Kind::INVALID) {
        return value;
    }

    switch (op) {
        using enum TokenType;

        case plus: {
            switch (value.kind) {
                using enum Value::Kind;
                case INTEGER:
                case FLOAT: return value;
                default: return Value{};
            }
        }
        case minus: {
            switch (value.kind) {
                using enum Value::Kind;
                case INTEGER: {
                    assert(value.int_value != std::numeric_limits<s64>::min());
                    return Value{.kind = INTEGER, .int_value = -value.int_value};
                }
                case FLOAT: {
                    return Value{.kind = FLOAT, .float_value = -value.float_value};
                }
                default: return Value{};
            }
        }
        case bang: {
            if (value.kind == Value::Kind::BOOL) {
                return Value{.kind = Value::Kind::BOOL, .bool_value = !value.bool_value};
            }
            return Value{};
        }
        default: panic("Unary operator not handled");
    }
}

void Typer::check_unary_expr(TyperContext &context, Operand &operand, Ast::UnaryOperatorExpression *expression) {
    check_expr(context, operand, expression->right);
    if (operand.kind == Operand::Kind::INVALID) {
        return;
    }
    
    if (expression->op.type == TokenType::ampersand) {
        if (operand.kind == Operand::Kind::VARIABLE) {
            operand.kind = Operand::Kind::VALUE;
            operand.type = create_type<PointerType>(operand.type);
            set_type_and_value(expression, operand);
        } else {
            error(reporter, expression->op, "Can not take an address of '{}'", Ast::expression_to_string(operand.expr));
            operand = Operand{};
        }
        return;
    }

    if (!check_unary_operator(context, operand, expression->op)) {
        operand = Operand{};
        return;
    }

    if (operand.kind == Operand::Kind::CONSTANT) {
        operand.value = apply_unary_operator(operand.value, expression->op.type);
        if (!check_representable_as_constant(operand.value, operand.type, expression)) {
            operand = Operand{};
            return;
        }
    }

    set_type_and_value(expression, operand);
}

bool Typer::check_binary_operator(TyperContext &context, const Operand &left, const Operand &right, const Token &token) {
    if (left.kind == Operand::Kind::INVALID || right.kind == Operand::Kind::INVALID) {
        return false;
    }

    if (left.kind == Operand::Kind::TYPE || right.kind == Operand::Kind::TYPE) {
        Ast::Expression *expr;
        if (right.kind == Operand::Kind::TYPE) {
            expr = right.expr;
        } else {
            assert(left.kind == Operand::Kind::TYPE);
            expr = left.expr;
        }
        error(reporter, expr, "Expected expression, got type");
        return false;
    }

    if (!are_types_the_same(left.type, right.type)) {
        error(reporter, token, "Can not apply binary operator '{}' on operands of different types ('{}' and '{}')",
              token.type, type_to_string(left.type), type_to_string(right.type));
        return false;
    }

    auto type = left.type->get_base_type();
    
    switch (token.type) {
        using enum TokenType;

        case equals:
        case not_equals: {
            if (type->is_bool() || type->is_numeric() || type->is_pointer()) {
                return true;
            }
            error(reporter, token, "Can not apply binary operator '{}' on types '{}' and '{}'", token.type,
                  type_to_string(left.type), type_to_string(right.type));
            return false;
        }
        case less:
        case greater:
        case less_equals:
        case greater_equals:
        case plus:
        case minus:
        case star:
        case divide: {
            if (type->is_numeric()) {
                return true;
            }
            error(reporter, token, "Can not apply binary operator '{}' on operands of non-numeric type ('{}' and '{}')",
                  token.type, type_to_string(left.type), type_to_string(right.type));
            return false;
        }

        case modulo: {
            if (type->is_integer()) {
                return true;
            }
            error(reporter, token, "Can not apply binary operator '{}' on operands of non-integer type ('{}' and '{}')",
                  token.type, type_to_string(left.type), type_to_string(right.type));
            return false;
        }
        default: panic("Binary operator not handled");
    }
}

#define APPLY_BINARY_COMPARISON_OPERATOR(op)                                                                              \
    {                                                                                                                  \
        switch (left.kind) {                                                                                           \
            using enum Value::Kind;                                                                                    \
                                                                                                                       \
            case INTEGER: {                                                                                            \
                return create_value_bool(((left.int_value)op(right.int_value)));                                                          \
            }                                                                                                          \
            case FLOAT: {                                                                                              \
                return create_value_bool(((left.float_value)op(right.float_value)));                                                      \
            }                                                                                                          \
            default: return Value{};                                                                                   \
        }                                                                                                              \
    }


#define APPLY_BINARY_NUMERIC_OPERATOR(op)                                                                              \
    {                                                                                                                  \
        switch (left.kind) {                                                                                           \
            using enum Value::Kind;                                                                                    \
                                                                                                                       \
            case INTEGER: {                                                                                            \
                return create_value_int(((left.int_value)op(right.int_value)));                                                          \
            }                                                                                                          \
            case FLOAT: {                                                                                              \
                return create_value_float(((left.float_value)op(right.float_value)));                                                      \
            }                                                                                                          \
            default: return Value{};                                                                                   \
        }                                                                                                              \
    }

// Does not check for overflow or underflow for integers with size bigger or equal to 64 bit 
static Value apply_binary_operator(const Value &left, const Value &right, TokenType op) {
    if (left.kind != right.kind) {
        return Value{};
    }

    switch (op) {
        using enum TokenType;

        case equals:
        case not_equals: {
            switch (left.kind) {
                using enum Value::Kind;

                case BOOL: {
                    return create_value_bool(left.bool_value == right.bool_value);
                }
                case INTEGER: {
                    return create_value_bool(left.int_value == right.int_value);
                }
                case FLOAT: {
                    return create_value_bool(left.float_value == right.float_value);
                }
                default: return Value{};
            }
        }
        
        case less: APPLY_BINARY_COMPARISON_OPERATOR(<)
        case greater: APPLY_BINARY_COMPARISON_OPERATOR(>)
        case less_equals: APPLY_BINARY_COMPARISON_OPERATOR(<=)
        case greater_equals: APPLY_BINARY_COMPARISON_OPERATOR(>=)

        case plus: APPLY_BINARY_NUMERIC_OPERATOR(+)
        case minus: APPLY_BINARY_NUMERIC_OPERATOR(-)
        case star: APPLY_BINARY_NUMERIC_OPERATOR(*)
        case divide: APPLY_BINARY_NUMERIC_OPERATOR(/)

        case modulo: {
            switch (left.kind) {
                using enum Value::Kind;
                case INTEGER: {
                    return create_value_int(left.int_value % right.int_value);
                }
                default: return Value{};
            }
        }
        default: panic("Binary operator not handled");
    }
}

void Typer::check_binary_expr(TyperContext &context, Operand &operand, Ast::BinaryOperatorExpression *expression) {
    auto operand_left = Operand{};
    auto operand_right = Operand{};
    check_expr(context, operand_left, expression->left);
    check_expr(context, operand_right, expression->right);

    if (operand_left.kind == Operand::Kind::INVALID || operand_right.kind == Operand::Kind::INVALID) {
        return;
    }

    if (!check_binary_operator(context, operand_left, operand_right, expression->op)) {
        return;
    }

    if (is_comparison_operator(expression->op.type)) {
        operand.type = bool_t;
    } else {
        operand.type = operand_left.type;
    }

    if (operand_left.kind == Operand::Kind::CONSTANT && operand_right.kind == Operand::Kind::CONSTANT) {
        operand.kind = Operand::Kind::CONSTANT;
        operand.value = apply_binary_operator(operand_left.value, operand_right.value, expression->op.type);
        if (!check_representable_as_constant(operand.value, operand.type, expression)) {
            operand = Operand{};
            return;
        }
    } else {
        operand.kind = Operand::Kind::VALUE;
    }

    set_type_and_value(expression, operand);
}

void Typer::check_deref_expr(TyperContext &context, Operand &operand, Ast::DerefExpression *expression) {
    check_expr(context, operand, expression->expression);
    if (operand.kind == Operand::Kind::INVALID) {
        return;
    }

    if (!operand.type->is_pointer()) {
        error(reporter, expression, "Expression is not a pointer");
        operand = Operand{}; 
        return;
    }

    set_type_and_value(expression, operand);
}

struct ApplyIndexOperatorResult {
    Value value;
    usize array_count = 0;
    s64 index = 0;
};

static ApplyIndexOperatorResult apply_index_operator(const Value &array, const Value &index) {
    auto a = array.as_compound();
    auto i = index.as_int();
    if (a.kind == Value::Kind::INVALID || i.kind == Value::Kind::INVALID) {
        return ApplyIndexOperatorResult{};
    }

    auto compound = a.compound_value;
    auto compound_index = i.int_value;

    if (compound_index < 0 || static_cast<usize>(compound_index) >= compound->values.size()) {
        return ApplyIndexOperatorResult{.value = {}, .array_count = compound->values.size(), .index = compound_index};
    }

    return ApplyIndexOperatorResult{.value = compound->values[compound_index]->value->value,
                                    .array_count = compound->values.size(),
                                    .index = compound_index};
}

void Typer::check_index_expr(TyperContext &context, Operand &operand, Ast::IndexExpression *expression) {
    auto operand_expr = Operand{};
    auto operand_index = Operand{};
    check_expr(context, operand_expr, expression->expression);
    check_expr(context, operand_index, expression->index);

    if (operand_expr.kind == Operand::Kind::INVALID || operand_index.kind == Operand::Kind::INVALID) {
        return;
    }

    if (!(operand_expr.type->is_array() || operand_expr.type->is_slice())) {
        error(reporter, expression->expression, "Expression it not an array or slice");
        operand = Operand{};
        return;
    }

    if (!operand_index.type->is_integer()) {
        error(reporter, expression->index, "Expression is not integer");
        operand = Operand{};
        return;
    }

    operand.type = operand_expr.type->get_core_type();
    bool is_const_expr = operand_expr.type->is_array() && operand_expr.kind == Operand::Kind::CONSTANT &&
                         operand_index.kind == Operand::Kind::CONSTANT;
    if (is_const_expr) {
        operand.kind = Operand::Kind::CONSTANT;
        auto result = apply_index_operator(operand_expr.value, operand_index.value);
        operand.value = result.value;
        if (operand.value.kind == Value::Kind::INVALID) {
            error(reporter, operand_index.expr, "Index out of range, array count is '{}' but index is '{}'", result.array_count, result.index);
            operand = Operand{};
            return;
        }
        if (!check_representable_as_constant(operand.value, operand.type, expression)) {
            operand = Operand{};
            return;
        } 
    } else {
        operand.kind = Operand::Kind::VALUE;
    }

    set_type_and_value(expression, operand);
}

void Typer::check_call_expr(TyperContext &context, Operand &operand, Ast::CallOperatorExpression *expression) {
    panic("TODO");
}

void Typer::check_cast_expr(TyperContext &context, Operand &operand, Ast::CastOperatorExpression *expression) {
    auto type = check_type(context, expression->cast_type);
    check_expr(context, operand, expression->expression);
    if (operand.kind == Operand::Kind::INVALID || type->is_bad()) {
        return; 
    }

    if (!operand.type->is_convertible_to(type)) {
        error(reporter, operand.expr, "Cannot convert '{}' to '{}'", type_to_string(operand.type), type_to_string(type));
        operand = Operand{};
        return;
    }

    operand.type = type;
    if (operand.kind == Operand::Kind::CONSTANT) {
        if (type->is_basic() && !check_representable_as_constant(operand.value, type, operand.expr)) {
            operand = Operand{};
            return;
        }
    }

    set_type_and_value(expression, operand);
}

void Typer::check_compound_expr(TyperContext &context, Operand &operand, Ast::CompoundExpression *expression, Type *type_hint) {
    panic("TODO");
}

void Typer::check_slice_expr(TyperContext &context, Operand &operand, Ast::SliceExpression *expression) {
    panic("TODO");
}

void Typer::check_init_variable(TyperContext &context, const Operand &operand, VariableEntity *entity) {
    if (entity->type == nullptr) {
        entity->type = operand.type;
    }

    check_assignment(context, operand, entity->type);
}

void Typer::check_init_constant(TyperContext &context, const Operand &operand, ConstantEntity *entity) {
    if (operand.kind == Operand::Kind::INVALID) {
        if (entity->type == nullptr) {
            entity->type = bad_t;
        }
        return;
    }

    if (operand.kind != Operand::Kind::CONSTANT) {
        if (entity->type == nullptr) {
            entity->type = bad_t;
        }
        error(reporter, entity->init_expression, "Expression is not constant");
        return;
    }

    if (entity->type == nullptr) {
        entity->type = operand.type;
    }

    if (!check_assignment(context, operand, entity->type)) {
        return;
    }

    entity->value = operand.value;
}

bool Typer::check_assignment(TyperContext &context, const Operand &operand, Type *type) {
    if (operand.kind == Operand::Kind::INVALID || type->is_bad()) {
        return false;
    }

    if (operand.kind == Operand::Kind::TYPE) {
        error(reporter, operand.expr, "Can not assign type");
        return false;
    }

    if (!are_types_the_same(operand.type, type)) {
        error(reporter, operand.expr, "Can not assign value '{}' of type '{}' to variable of type '{}'",
              Ast::expression_to_string(operand.expr), type_to_string(operand.type), type_to_string(type));
        return false;
    }

    return true;
}

Maybe<Entity *> Typer::check_selector(TyperContext &context, Operand &operand, Ast::SelectorExpression *selector) {
    auto operand_expr = Operand{};
    check_expr_internal(context, operand_expr, selector->expression, nullptr);
    if (operand_expr.kind == Operand::Kind::INVALID) {
        return {};
    }

    auto is_type = operand_expr.kind == Operand::Kind::TYPE;
    auto entity = lookup_field(operand_expr.type, selector->identifier, is_type); 
    if (!entity) {
        auto type_string = type_to_string(operand_expr.type);
        if (is_type) {
            error(reporter, selector, "Type {} has no declaration {}", type_string, selector->identifier->token.value);
        } else {
            error(reporter, selector, "Type {} has no field {}", type_string, selector->identifier->token.value);
        }
        return {};
    }

    if (entity->state == Entity::State::UNRESOLVED) {
        check_entity_decl(context, *entity);
    }

    switch (entity->kind) {
        using enum Entity::Kind;

        case CONSTANT:
        case VARIABLE:
        case NAMED_TYPE: {
            if (check_cycle(context, *entity)) {
                assert(operand.kind == Operand::Kind::INVALID);
                return entity;
            }
            break;
        }
        default: break;
    }

    if (entity->type->is_bad()) {
        return entity;
    }

    switch (entity->kind) {
        using enum Entity::Kind;
        
        case CONSTANT: {
            auto constant = entity->as<ConstantEntity>();
            operand.kind = Operand::Kind::CONSTANT;
            operand.value = constant->value;
            break;
        }
        case VARIABLE: {
            operand.kind = Operand::Kind::VARIABLE;
            break;
        }
        case NAMED_TYPE: {
            operand.kind = Operand::Kind::TYPE;
            break;
        }
        case PROCEDURE: {
            operand.kind = Operand::Kind::VALUE;
            break;
        }
    }

    operand.type = entity->type;
    set_type_and_value(selector, operand);
    selector->identifier->entity = *entity;
    return entity;
}

Maybe<Entity *> Typer::lookup_field(Type *type, Ast::Identifier *identifier, bool is_type) {
    type = type->get_base_type();

    if (is_type) {
        if (type->is<StructType>()) {
            auto struct_type = type->as<StructType>();
            return struct_type->inner_scope->look_up_current(identifier);
        }
        return {};
    }

    if (type->is<StructType>()) {
        auto struct_type = type->as<StructType>();
        for (auto member : struct_type->members) {
            if (member->declaration->identifier->token.value == identifier->token.value) {
                return member;
            }
        }
    }
    return {};
}

Maybe<Entity*> Typer::check_identifier_or_selector(TyperContext &context, Operand &operand, Ast::Expression *expression) {
    if (expression->is<Ast::IdentifierExpression>()) {
        auto result = check_identifier(context, operand, expression->as<Ast::IdentifierExpression>()->identifier);
        set_type_and_value(expression, operand);
        return result;
    }

    if (expression->is<Ast::SelectorExpression>()) {
        return check_selector(context, operand, expression->as<Ast::SelectorExpression>());
    }

    panic("expression is not Selector or Identifier");
}

u64 Typer::check_array_count(TyperContext &context, Operand &operand, Ast::Expression *expression) {
    check_expr(context, operand, expression);
    if (operand.kind == Operand::Kind::INVALID) {
        return 0;
    }
    if (operand.kind != Operand::Kind::CONSTANT) {
        error(reporter, expression, "Array size must be a constant expression");
        return 0;
    }

    if (operand.type->is_integer()) {
        switch (operand.value.kind) {
            using enum Value::Kind;
            
            case INTEGER: {
                if (operand.value.int_value <= 0) {
                    error(reporter, expression, "Array count must be a positive integer, got {}", operand.value.int_value);
                    return 0;
                }
                return operand.value.int_value;
            }
            case INVALID: {
                // Error happened somewhere else and was already reported
                return 0;
            }

            default: panic("Incorrect value");
        }
    }

    error(reporter, expression, "Array count must be a constant integer");
    return 0;
}

Type *Typer::check_type(TyperContext &context, Ast::Type *ast_type) {
    if (ast_type->type != nullptr) {
        return ast_type->type;
    }

    switch (ast_type->kind) {
        using enum Ast::Type::Kind;
        
        case BAD: {
            ast_type->type = bad_t;
            return ast_type->type;
        }

        case IDENTIFIER: {
            auto ast_identifier = ast_type->as<Ast::IdentifierType>();
            auto operand = Operand{};
            check_identifier_or_selector(context, operand, ast_identifier->expression);
            if (operand.kind != Operand::Kind::INVALID && operand.kind != Operand::Kind::TYPE) {
                error(reporter, ast_identifier, "Not a type");
                ast_type->type = bad_t;
            } else {
                ast_type->type = operand.type;
            }
            return ast_type->type;
        }
        
        case POINTER: {
            auto decl_path = std::vector<const Entity*>{};
            auto new_context = context;
            new_context.decl_path = &decl_path;

            auto ast_pointer = ast_type->as<Ast::PointerType>();
            auto type = check_type(new_context, ast_pointer->type);
            ast_type->type = create_type<PointerType>(type);
            return ast_type->type;
        }

        case ARRAY: {
            auto ast_array = ast_type->as<Ast::ArrayType>();
            auto type = check_type(context, ast_array->element_type);
            auto operand = Operand{};
            auto count = check_array_count(context, operand, ast_array->count);
            ast_type->type = create_type<ArrayType>(type, count, context.scope);
            return ast_type->type;
        }

        case PROCEDURE: {
            auto ast_proc = ast_type->as<Ast::ProcedureType>();
            auto parameters_temp = create_temp_vector<Type*>(ast_proc->parameters.size());
            for (auto ast_parameter : ast_proc->parameters) {
                auto type = check_type(context, ast_parameter->type);
                parameters_temp.push_back(type);
            }
            auto parameters = create_array(std::span{parameters_temp});
            auto return_type = Maybe<Type*>();
            if (ast_proc->return_type) {
                return_type = check_type(context, *ast_proc->return_type);
            }
            ast_type->type = create_type<ProcedureType>(parameters, return_type);
            return ast_type->type;
        }

        case STRUCT: {
            auto ast_struct = ast_type->as<Ast::StructType>();
            auto members_temp = create_temp_vector<VariableEntity *>(ast_struct->members.size());
            open_scope(context, ast_struct);
            for (auto ast_member : ast_struct->members) {
                auto member_entity = create_entity<VariableEntity>(context.scope, ast_member, ast_member->type,
                                                                   Maybe<Ast::Expression *>{});
                member_entity->variable_kind = VariableEntity::VariableKind::STRUCT_MEMBER;
                member_entity->flags |= Entity::Flags::RESOLVED;
                member_entity->type = check_type(context, ast_member->type);
                members_temp.push_back(member_entity);
            }
            collect_entities(context, ast_struct->declarations);
            auto members = create_array(std::span{members_temp});
            ast_type->type = create_type<StructType>(members, context.scope);
            close_scope(context);
            return ast_type->type;
        }

        case SLICE: {
            auto ast_slice = ast_type->as<Ast::SliceType>();
            auto type = check_type(context, ast_slice->element_type);
            ast_type->type = create_type<SliceType>(type);
            return ast_type->type;
        }
    }
    panic("Ast::Type is not handled");
}

void Typer::check_entity_decl(TyperContext &context, Entity *entity) {
    if (entity->state == Entity::State::RESOLVED) {
        return;
    }

    if (entity->state != Entity::State::UNRESOLVED) {
        error(reporter, entity->declaration, "Declaration cycle of {}", full_entity_name(entity));
        entity->state = Entity::State::RESOLVED;
        return;
    }
    
    bool push_decl_path = false;
    switch (entity->kind) {
        using enum Entity::Kind;
        
        case CONSTANT:
        case VARIABLE:
        case NAMED_TYPE: {
            push_decl_path = true;
            break;
        }
        default: break;
    }
    if (push_decl_path) {
        context.decl_path->push_back(entity);
    }

    auto new_context = context;
    new_context.scope = entity->scope;

    entity->state = Entity::State::IN_PROGRESS;
    switch (entity->kind) {
        using enum Entity::Kind;

        case VARIABLE: {
            check_global_variable_decl(new_context, entity->as<VariableEntity>());
            break;
        }
        case CONSTANT: {
            check_constant_decl(new_context, entity->as<ConstantEntity>());
            break;
        }
        case PROCEDURE: {
            check_proc_decl(new_context, entity->as<ProcedureEntity>());
            break;
        }
        case NAMED_TYPE: {
            check_type_decl(new_context, entity->as<NamedTypeEntity>());
            break;
        }
    }

    if (push_decl_path) {
        context.decl_path->pop_back();
    }

    entity->state = Entity::State::RESOLVED;
}

void Typer::check_global_variable_decl(TyperContext &context, VariableEntity *entity) {    
    if (entity->ast_type != nullptr) {
        entity->type = check_type(context, entity->ast_type);
    }

    if (!entity->init_expression) {
        assert(entity->ast_type != nullptr); // Should be caught in parser
        return;
    }

    auto operand = Operand{};
    check_expr(context, operand, *entity->init_expression, entity->type);
    check_init_variable(context, operand, entity);
}

void Typer::check_constant_decl(TyperContext &context, ConstantEntity *entity) {
    if (entity->ast_type != nullptr) {
        entity->type = check_type(context, entity->ast_type);
    }

    auto operand = Operand{};
    check_expr(context, operand, entity->init_expression, entity->type);
    check_init_constant(context, operand, entity);
}

void Typer::check_proc_decl(TyperContext &context, ProcedureEntity *entity) {
    entity->type = check_type(context, entity->ast_type);
    procedure_bodies_to_check_.push_back(entity);
}

void Typer::check_type_decl(TyperContext &context, NamedTypeEntity *entity) {
    auto declaration = entity->declaration->as<Ast::TypeDeclaration>();
    // Entity type should be set first
    entity->type = create_type<NamedType>(declaration->identifier->token.value, entity);
    entity->type->as<NamedType>()->type = check_type(context, entity->ast_type);
}

bool Typer::do_typing() {
    auto decl_path = std::vector<const Entity*>{};

    auto context = TyperContext{};
    context.scope = file_scope_;
    context.decl_path = &decl_path;

    bool added = false;
    for (usize i = 1; i < basic_types.size(); ++i) {
        auto basic_type = &basic_types[i];
        auto entity = create_entity<NamedTypeEntity>(context.scope, nullptr, nullptr);
        entity->state = Entity::State::RESOLVED;
        entity->type = basic_type;
        added |= add_entity(context.scope, entity, basic_type->name);
    }
    if (!added) {
        panic("Could not add builtin type to file scope");
    }

    assert(!parser_.reporter.any_errors());

    collect_entities(context, parser_.ast);
    if (parser_.reporter.any_errors()) {
        return false;
    }

    for (usize i = 0; i < entities_.size(); ++i) {
        check_entity_decl(context, entities_[i]);
    }

    if (parser_.reporter.any_errors()) {
        return false;
    }

    return parser_.reporter.any_errors();
}

} // namespace Typing