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

static void calculate_size_and_alignment(Type *type) {
    if (has_flag(type->flags, Type::Flags::SIZED)) {
        return;
    }

    type->flags |= Type::Flags::SIZED;
    switch (type->kind) {
        using enum Type::Kind;
        
        case BASIC:
        case PROCEDURE:
        case POINTER:
        case SLICE: panic("Size should be known");

        case NAMED: {
            auto named = type->as<NamedType>();
            calculate_size_and_alignment(named->type);
            named->size = named->type->size;
            named->align = named->type->align;
            return;
        } 
        case ARRAY: {
            auto array = type->as<ArrayType>();
            calculate_size_and_alignment(array->type);
            array->size = array->type->size * array->count;
            if (array->size != 0) {
                array->align = array->type->align;
            }
            return;
        }
        case STRUCT: {
            auto struct_type = type->as<StructType>();
            u64 size = 0;
            u64 max_align = 0;
            for (auto member : struct_type->members) {
                calculate_size_and_alignment(member->type);
                if (member->type->align != 0) {
                    if (type->align > max_align) {
                        max_align = type->align;
                    }
                    size = align_forward(size, member->type->align);
                    size += member->type->size;
                }
            }
            struct_type->size = size;
            struct_type->align = max_align;
            return;
        }
    }
    panic("Type not handled");
}

static u64 size_of_type(Type *type) {
    if (has_flag(type->flags, Type::Flags::SIZED)) {
        return type->size;
    }
    calculate_size_and_alignment(type);
    return type->size;
}

static u64 align_of_type(Type *type) {
    if (has_flag(type->flags, Type::Flags::SIZED)) {
        return type->align;
    }
    calculate_size_and_alignment(type);
    return type->align;
}

static void set_type_and_value(Ast::Expression *expression, Operand &operand) {
    expression->type = operand.type;
    expression->value = operand.value;
    operand.expr = expression;
}

// This will create new node each time there is access to a constant value that was created from empty literal and is not basic type
// Which is not perfect but fine for now
static Value create_default_value_for_type(Ast::Parser &parser, Type *type) {
    auto base = type->get_base_type();   
    if (base->is_array() || base->is_struct() || base->is_slice()) {
        auto new_compound_node = parser.New<Ast::CompoundExpression>();
        new_compound_node->type = type;
        auto new_compound_value = create_value_compound(new_compound_node);
        new_compound_node->value = new_compound_value;
        return new_compound_value;
    }
    if (base->is_integer()) {
        return create_value_int(big_int_create_from_s64(0));
    }
    if (base->is_bool()) {
        return create_value_bool(false);
    }
    if (base->is_float()) {
        return create_value_float(0.0);
    }
    if (base->is_string()) {
        return create_value_string("");
    }
    if (base->is_pointer()) {
        return create_value_pointer(0);
    }
    return Value{};
}

static bool check_representable_as_constant_internal(Value &value, Type *to) {
    if (value.kind == Value::Kind::INVALID) {
        return false;
    }

    if (value.kind == Value::Kind::COMPOUND) {
        return are_types_the_same(value.as_compound().compound_value->type, to);
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
        auto v = value.try_convert_to_int();
        if (v.kind == Value::Kind::INVALID) {
            return false;
        }
        value = v;

        u64 unsigned_max = 0;

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
            unsigned_max = static_cast<u64>(std::numeric_limits<u8>::max());
        } else if (type == u16_t) {
            unsigned_max = static_cast<u64>(std::numeric_limits<u16>::max());
        } else if (type == u32_t) {
            unsigned_max = static_cast<u64>(std::numeric_limits<u32>::max());
        } else if (type == u64_t) {
            unsigned_max = static_cast<u64>(std::numeric_limits<u64>::max());
        } else if (type == uint_t) {
            unsigned_max = static_cast<u64>(std::numeric_limits<u64>::max());
        }

        auto big_int_min = BigInt{};
        auto big_int_max = BigInt{};
        if (type->is_unsigned()) {
            big_int_min = big_int_create_from_u64(0);
            big_int_max = big_int_create_from_u64(unsigned_max);
        } else {
            big_int_min = big_int_create_from_s64(min);
            big_int_max = big_int_create_from_s64(max);
        }

        return big_int_greater_equal(value.int_value, big_int_min) && big_int_less_equal(value.int_value, big_int_max);
    }
    if (type->is_float()) {
        auto v = value.try_convert_to_float();
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
        error(reporter, expression, "Value '{}' is not representable as type '{}'", value_to_string(value), type_to_string(to));  
        return false;
    }
    return true;
}

void Typer::open_scope(TyperContext &context, Scope *&out_scope) {
    auto scope = create_scope(context.scope);
    scope->entity = context.entity;
    
    out_scope = scope;
    context.scope = scope;
}

void Typer::close_scope(TyperContext &context) {
    context.scope = *context.scope->parent;
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
    entity->parent = scope->entity;
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

VariableEntity *Typer::create_entity_variable(TyperContext &context, Ast::VariableDeclaration *ast_variable) {
    Ast::Type *type = nullptr;
    if (ast_variable->type) {
        type = *ast_variable->type;
    }
    auto entity = create_entity<VariableEntity>(context.scope, ast_variable, type, ast_variable->value);
    ast_variable->entity = entity;
    return entity;
}

Entity *Typer::collect_entity(TyperContext &context, Ast::Declaration *declaration) {
    if ((declaration->flags & Ast::Declaration::Flags::HANDLED) ==
    Ast::Declaration::Flags::HANDLED) {
        return declaration->entity;
    }
    declaration->flags |= Ast::Declaration::Flags::HANDLED;
    
    switch (declaration->kind) {
        using enum Ast::Declaration::Kind;
        
        case VARIABLE: {
            auto ast_variable = declaration->as<Ast::VariableDeclaration>();
            auto entity = create_entity_variable(context, ast_variable);
            entity->is_global = true;
            add_entity(context.scope, entity);
            return entity;
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
            return entity;
        }

        case PROCEDURE: {
            auto ast_proc = declaration->as<Ast::ProcedureDeclaration>();
            auto entity =
                create_entity<ProcedureEntity>(context.scope, ast_proc, ast_proc->type, create_scope(context.scope));
            entity->inner_scope->entity = entity;
            declaration->entity = entity;
            add_entity(context.scope, entity);
            return entity;
        }

        case TYPE: {
            auto ast_type = declaration->as<Ast::TypeDeclaration>();
            auto entity = create_entity<NamedTypeEntity>(context.scope, ast_type, ast_type->type);
            declaration->entity = entity;
            add_entity(context.scope, entity);
            return entity;
        }

        case FIELD: panic("Should be caught in parser");
    }
    panic("Ast::Declaration is not handled");
}

void Typer::collect_entities(TyperContext &context, std::span<Ast::DeclarationStatement *> statements) {
    for (auto statement : statements) {
        collect_entity(context, statement->as<Ast::DeclarationStatement>()->declaration);
    }
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
            auto entity_name = entity->full_name();
            if (i == path.size() - 1) {
                error(reporter, entity->declaration, "Declaration cycle of '{}': '{}' refers to itself", entity_name,
                      entity_name);
            } else {
                error(reporter, entity->declaration, "Declaration cycle of '{}'", entity_name);
                for (usize j = i; j < path.size() - 1; ++j) {
                    error(reporter, path[j]->declaration, "'{}' refers to '{}'", path[j]->full_name(),
                          path[j + 1]->full_name());
                }
                error(reporter, path[path.size() - 1]->declaration, "'{}' refers to '{}'", path[path.size() - 1]->full_name(),
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
                operand = Operand{};
                return {};
            }
            break;
        }
        default: break;
    }

    if (entity->type->get_base_type() != nullptr && entity->type->is_bad()) {
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
            auto variable = entity->as<VariableEntity>();
            // If the variable is local then entity has to be the same, which means we can only access it from the
            // procedure where it is declared
            if (!variable->is_global) {
                if (context.entity != entity->parent.expect("since variable is local there has to be a parent")) {
                    not_declared_error(identifier);
                    operand = Operand{};
                    return {};
                }
            }
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

void Typer::check_type_expr(TyperContext &context, Operand &operand, Ast::Expression *expression) {
    check_expr_internal(context, operand, expression, nullptr);
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
            auto literal = expression->as<Ast::IntegerLiteralExpression>();
            operand.kind = Operand::Kind::CONSTANT;

            auto value = big_int_create_from_string(literal->token.value);
            assert(!big_int_is_negative(value));
            if (big_int_less_equal(value, BIG_INT_MAX_S64)) {
                operand.type = int_t;
                operand.value = create_value_int(value);
            } else if (big_int_less_equal(value, BIG_INT_MAX_U64)) {
                operand.type = uint_t;
                operand.value = create_value_int(value);
            } else {
                error(reporter, literal->token,
                      "Literal '{}' is larger then max unsigned 64-bit integer, those are not supported for now",
                      literal->token.value);
                operand = Operand{};
                return;
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

        case SIZE_OF: {
            auto size_of = expression->as<Ast::SizeOfExpression>();
            check_size_of_expression(context, operand, size_of);
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

        case AUTO_CAST_OPERATOR: {
            auto auto_cast = expression->as<Ast::AutoCastOperatorExpression>();
            check_auto_cast_expr(context, operand, auto_cast, type_hint);
            return;
        }
        
        case TRANSMUTE_OPERATOR: {
            auto transmute = expression->as<Ast::TransmuteOperatorExpression>();
            check_transmute_expr(context, operand, transmute);
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

void Typer::check_size_of_expression(TyperContext &context, Operand &operand, Ast::SizeOfExpression *size_of) {
    auto operand_expression = Operand{};
    check_type_expr(context, operand_expression, size_of->expression);
    if (operand_expression.kind == Operand::Kind::INVALID) {
        operand = Operand{};
        return;
    }

    operand.kind = Operand::Kind::CONSTANT;
    operand.type = uint_t;
    operand.value = create_value_u64(size_of_type(operand_expression.type));

    set_type_and_value(size_of, operand);
}

bool Typer::check_unary_operator(TyperContext &/*context*/, const Operand &operand, const Token &token) {
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
                    auto result = big_int_create();
                    big_int_negate(&result, value.int_value);
                    return create_value_int(result);   
                }
                case FLOAT: {
                    return create_value_float(-value.float_value);
                }
                default: return Value{};
            }
        }
        case bang: {
            if (value.kind == Value::Kind::BOOL) {
                return create_value_bool(!value.bool_value);
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

bool Typer::check_binary_operator(TyperContext &/*context*/, const Operand &left, const Operand &right, const Token &token) {
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

        case plus_assign:
        case minus_assign:
        case multiply_assign:
        case divide_assign:
        case modulo_assign:
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

#define APPLY_BINARY_COMPARISON_OPERATOR(op, op_func)                                                                  \
    {                                                                                                                  \
        switch (left.kind) {                                                                                           \
            using enum Value::Kind;                                                                                    \
                                                                                                                       \
            case INTEGER: {                                                                                            \
                return create_value_bool(op_func(left.int_value, right.int_value));                                    \
            }                                                                                                          \
            case FLOAT: {                                                                                              \
                return create_value_bool(left.float_value op right.float_value);                                       \
            }                                                                                                          \
            default: return Value{};                                                                                   \
        }                                                                                                              \
    }

#define APPLY_BINARY_NUMERIC_OPERATOR(op, op_func)                                                                     \
    {                                                                                                                  \
        switch (left.kind) {                                                                                           \
            using enum Value::Kind;                                                                                    \
                                                                                                                       \
            case INTEGER: {                                                                                            \
                auto result = big_int_create();                                                                        \
                op_func(&result, left.int_value, right.int_value);                                                      \
                return create_value_int(result);                                                                       \
            }                                                                                                          \
            case FLOAT: {                                                                                              \
                return create_value_float(left.float_value op right.float_value);                                      \
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
                    return create_value_bool(big_int_cmp(left.int_value, right.int_value));
                }
                case FLOAT: {
                    return create_value_bool(left.float_value == right.float_value);
                }
                default: return Value{};
            }
        }
        
        case less: APPLY_BINARY_COMPARISON_OPERATOR(<, big_int_less)
        case greater: APPLY_BINARY_COMPARISON_OPERATOR(>, big_int_greater)
        case less_equals: APPLY_BINARY_COMPARISON_OPERATOR(<=, big_int_less_equal)
        case greater_equals: APPLY_BINARY_COMPARISON_OPERATOR(>=, big_int_greater_equal)

        case plus: APPLY_BINARY_NUMERIC_OPERATOR(+, big_int_add)
        case minus: APPLY_BINARY_NUMERIC_OPERATOR(-, big_int_sub)
        case star: APPLY_BINARY_NUMERIC_OPERATOR(*, big_int_mul)

        case divide: {
            switch (left.kind) {
                using enum Value::Kind;

                case INTEGER: {
                    auto quotient = big_int_create();
                    big_int_div(&quotient, nullptr, left.int_value, right.int_value);
                    return create_value_int(quotient);
                }
                case FLOAT: {
                    return create_value_float(left.float_value / right.float_value);
                }
                default: return Value{};
            }
        }

        case modulo: {
            switch (left.kind) {
                using enum Value::Kind;
                case INTEGER: {
                    if (big_int_is_zero(right.int_value)) {
                        return Value{};
                    }
                    auto remainder = big_int_create();
                    big_int_div(nullptr, &remainder, left.int_value, right.int_value);
                    return create_value_int(remainder);
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

    operand.kind = Operand::Kind::VARIABLE;
    operand.type = operand.type->get_core_type();

    set_type_and_value(expression, operand);
}

static Value apply_index_operator(Ast::Parser &parser, const Value &array, u64 index) {
    auto a = array.as_compound_or_invalid();
    if (a.kind == Value::Kind::INVALID) {
        return Value{};
    }

    auto compound = a.compound_value;
    auto compound_index = index;

    if (!compound->values.empty()) {
        if (compound_index >= compound->values.size()) {
            return Value{};
        }
        return compound->values[compound_index]->value->value;
    }

    auto type = compound->type->get_base_type();
    if (type->is_array()) {
        auto array_type = type->as<ArrayType>();
        auto array_count = array_type->count;
        if (compound_index >= array_count) {
            return Value{};
        }
        return create_default_value_for_type(parser, array_type->get_core_type());
    }
    if (type->is_struct()) {
        auto struct_type = type->as<StructType>();
        auto member_count = struct_type->members.size();
        if (compound_index >= member_count) {
            return Value{};
        }
        return create_default_value_for_type(parser, struct_type->members[compound_index]->type);
    }
    return Value{};
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
        if (operand_index.value.as_int().kind == Value::Kind::INVALID) {
            operand = Operand{};
            return;
        }

        auto index_big = operand_index.value.int_value;
        auto array_type = operand_expr.type->get_base_type()->as<ArrayType>();
        auto array_count = array_type->count;

        if (big_int_is_negative(index_big)) {
            error(reporter, operand_index.expr, "Index is negative ('{}')", big_int_to_string(index_big));
            operand = Operand{};
            return;
        }
        auto index = big_int_to_u64(index_big);
        if (index >= array_count) {
            error(reporter, operand_index.expr, "Index out of range, array count is '{}' but index is '{}'",
                  array_count, index);
            operand = Operand{};
            return;
        }

        operand.kind = Operand::Kind::CONSTANT;
        operand.value = apply_index_operator(parser_, operand_expr.value, index);
        if (!check_representable_as_constant(operand.value, operand.type, expression)) {
            operand = Operand{};
            return;
        }
    } else {
        if (operand_expr.kind == Operand::Kind::CONSTANT) {
            // Array is constant, but index is not
            operand.kind = Operand::Kind::VALUE;
        } else {
            operand.kind = operand_expr.kind;
        }
    }

    set_type_and_value(expression, operand);
}

void Typer::check_call_expr(TyperContext &context, Operand &operand, Ast::CallOperatorExpression *expression) {
    auto operand_callable = Operand{};
    check_expr(context, operand_callable, expression->expression);
    if (operand_callable.kind == Operand::Kind::INVALID) {
        operand = Operand{};
        return;
    }     

    if (!operand_callable.type->is_procedure()) {
        error(reporter, expression->expression, "Expression is not callable");
        operand = Operand{};
        return;
    }

    bool err = false;

    auto proc_type = operand_callable.type->get_base_type()->as<ProcedureType>();
    auto parameter_count = proc_type->parameters.size();
    auto argument_count = expression->arguments.size();
    if (parameter_count != argument_count) {
        error(reporter, expression->expression, "Expected {} arguments for procedure call, got {}", parameter_count,
              argument_count);
        err = true;
    }

    for (auto i : indices(expression->arguments.size())) {
        auto argument = expression->arguments[i];

        Type *parameter_type = nullptr;
        if (i < parameter_count) {
            parameter_type = proc_type->parameters[i];
        }

        auto operand_argument = Operand{};
        check_expr(context, operand_argument, argument, parameter_type);
        if (operand_argument.kind == Operand::Kind::INVALID) {
            err = true;
            continue;
        }

        if (parameter_type != nullptr) {
            if (!are_types_the_same(operand_argument.type, parameter_type)) {
                error(reporter, argument, "Procedure takes parameter of type {}, got argument of type {}",
                      type_to_string(parameter_type), type_to_string(operand_argument.type));
                err = true;
            }
        }
    }

    if (err) {
        operand = Operand{};
        return;
    }

    operand.kind = Operand::Kind::VALUE;
    if (proc_type->return_type) {
        operand.type = proc_type->return_type;
    } else {
        operand.type = void_t;
    }
    set_type_and_value(expression, operand);
}

void Typer::check_transmute_expr(TyperContext &context, Operand &operand, Ast::TransmuteOperatorExpression *expression) {
    auto type = check_type(context, expression->transmute_type);
    check_expr(context, operand, expression->expression);
    if (operand.kind == Operand::Kind::INVALID) {
        operand = Operand{};
        return;
    }

    auto operand_type_size = size_of_type(operand.type);
    auto type_dest_size = size_of_type(type);
    if (operand_type_size != type_dest_size) {
        error(reporter, expression,
              "Cannot transmute '{}' to '{}' because types have different sizes ('{}' vs '{}' bytes)",
              type_to_string(operand.type), type_to_string(type), operand_type_size, type_dest_size);
        operand = Operand{};
        return;
    }

    operand.kind = Operand::Kind::VALUE;
    operand.type = type;
    operand.value = Value{};

    set_type_and_value(expression, operand);
}

void Typer::check_auto_cast_expr(TyperContext &context, Operand &operand, Ast::AutoCastOperatorExpression *expression, Type *type_hint) {
    check_expr(context, operand, expression->expression, nullptr);
    if (operand.kind == Operand::Kind::INVALID) {
        operand = Operand{};
        return;
    }

    if (type_hint == nullptr) {
        error(reporter, expression, "Can not determine type to auto_cast to");
        operand = Operand{};
        return;
    }

    if (!operand.type->is_convertible_to(type_hint)) {
        error(reporter, operand.expr, "Cannot convert '{}' to '{}'", type_to_string(operand.type), type_to_string(type_hint));
        operand = Operand{};
        return;
    }

    operand.type = type_hint;
    if (operand.kind == Operand::Kind::CONSTANT) {
        if (!check_representable_as_constant(operand.value, type_hint, operand.expr)) {
            operand = Operand{};
            return;
        }
    }

    set_type_and_value(expression, operand);
}   

void Typer::check_cast_expr(TyperContext &context, Operand &operand, Ast::CastOperatorExpression *expression) {
    auto type = check_type(context, expression->cast_type);
    check_expr(context, operand, expression->expression);
    if (operand.kind == Operand::Kind::INVALID) {
        operand = Operand{};
        return; 
    }

    if (!operand.type->is_convertible_to(type)) {
        error(reporter, operand.expr, "Cannot convert '{}' to '{}'", type_to_string(operand.type), type_to_string(type));
        operand = Operand{};
        return;
    }

    operand.type = type;
    if (operand.kind == Operand::Kind::CONSTANT) {
        if (!check_representable_as_constant(operand.value, type, operand.expr)) {
            operand = Operand{};
            return;
        }
    }

    set_type_and_value(expression, operand);
}

void Typer::check_compound_expr(TyperContext &context, Operand &operand, Ast::CompoundExpression *expression, Type *type_hint) {
    auto type = type_hint;
    if (expression->compound_type) {
        type = check_type(context, *expression->compound_type);
    }

    if (type == nullptr) {
        error(reporter, expression, "Can not determine type of a compound");
        operand = Operand{};
        return;
    }

    if (type->is_bad()) {
        operand = Operand{};
        return;
    }

    bool err = false;
    bool is_const = true;
    auto base = type->get_base_type();
    if (base->is_array()) {
        auto array = base->as<ArrayType>();
        auto core_type = array->get_core_type();
        auto compound_count = expression->values.size();
        if (compound_count != 0 && array->count != compound_count) {
            error(reporter, expression, "Expected {} values for this array literal, got {}", array->count,
                  compound_count);
            err = true;
        }
        for (auto value : expression->values) {
            if (value->identifier) {
                error(reporter, value->identifier->token, "Did not expect field name for array literal");
                err = true;
            }

            auto operand_value = Operand{};
            check_expr(context, operand_value, value->value, core_type);
            if (operand_value.kind == Operand::Kind::INVALID) {
                err = true;
            } else {
                if (operand_value.kind != Operand::Kind::CONSTANT) {
                    is_const = false;
                }
                check_assignment(context, operand_value, core_type);
            }
        }
    } else if (base->is_struct()) {
        auto struct_type = base->as<StructType>();
        auto compound_count = expression->values.size();
        auto struct_member_count = struct_type->members.size();
        if (compound_count != 0 && struct_member_count != compound_count) {
            error(reporter, expression, "Expected {} values for this struct literal, got {}", struct_member_count,
                  compound_count);
            err = true;
        }
        auto set_members = std::unordered_set<Entity *>{};
        for (auto i : indices(compound_count)) {
            auto value = expression->values[i];

            Entity *member = nullptr;
            // Only one path here will always be taken because of the check in parser
            if (value->identifier) {
                auto entity = lookup_field(struct_type, *value->identifier, false);
                if (entity) {
                    member = *entity;
                    if (set_members.contains(member)) {
                        error(reporter, value->identifier->token, "Duplicate field '{}' in struct literal",
                              value->identifier->token.value);
                        err = true;
                    } else {
                        set_members.insert(member);
                    }
                } else {
                    error(reporter, value->identifier->token, "Unknown field '{}' in struct literal",
                          value->identifier->token.value);
                    err = true;
                }
            } else {
                // If compound has more values than fields in a struct, we do not have a member, but still need to check expression
                if (i < struct_member_count) {
                    member = struct_type->members[i];
                }
            }

            Type *member_type = bad_t;
            if (member != nullptr) {
                member_type = member->type;
            }

            auto operand_value = Operand{};
            check_expr(context, operand_value, value->value, member_type);
            if (operand_value.kind == Operand::Kind::INVALID) {
                err = true;
            } else {
                if (operand_value.kind != Operand::Kind::CONSTANT) {
                    is_const = false;
                }
                check_assignment(context, operand_value, member_type);
            }
        }
    } else {
        if (expression->values.size() != 0) {
            error(reporter, expression, "{} can not be used as a compound literal with fields", type_to_string(type));
            operand = Operand{};
            return;
        }
        operand.kind = Operand::Kind::CONSTANT;
        operand.value = create_default_value_for_type(parser_, type);
        operand.type = type;
        set_type_and_value(expression, operand);
        return;
    }

    if (err) {
        operand = Operand{};
        return;
    }

    if (is_const) {
        operand.kind = Operand::Kind::CONSTANT;
        operand.value = create_value_compound(expression);
        operand.type = type;
    } else {
        operand.kind = Operand::Kind::VALUE;
        operand.type = type;
    }

    set_type_and_value(expression, operand);
}

void Typer::check_slice_expr(TyperContext &context, Operand &operand, Ast::SliceExpression *expression) {
    auto operand_slice = Operand{};
    check_expr(context, operand_slice, expression->expression, nullptr);
    if (operand_slice.kind == Operand::Kind::INVALID) {
        operand = Operand{};
        return;
    }

    if (!operand_slice.type->is_array() && !operand_slice.type->is_slice()) {
        error(reporter, expression->expression, "Expression is not an array or slice");
        operand = Operand{};
        return;
    }

    auto operand_open = Maybe<Operand>{};
    auto operand_close = Maybe<Operand>{};
    if (expression->interval_open) {
        auto temp = Operand{};
        check_expr(context, temp, *expression->interval_open);
        operand_open = temp;
    }
    if (expression->interval_close) {
        auto temp = Operand{};
        check_expr(context, temp, *expression->interval_close);
        operand_close = temp;
    }

    operand.kind = Operand::Kind::VALUE;
    operand.type = create_type<SliceType>(operand_slice.type->get_core_type());

    auto count = Maybe<u64>{};
    if (operand_slice.kind == Operand::Kind::CONSTANT) {
        count = operand_slice.type->get_base_type()->as<ArrayType>()->count;
    }

    auto open = Maybe<u64>{};
    auto close = Maybe<u64>{};
    bool err = false;
    if (operand_open) {
        if (!operand_open->type->is_integer()) {
            error(reporter, *expression->interval_open, "Expected integer");
            err = true;
        } else if (operand_open->kind == Operand::Kind::CONSTANT &&
                   operand_open->value.as_int_or_invalid().kind == Value::Kind::INTEGER) {
            auto open_big = operand_open->value.int_value;
            if (big_int_is_negative(open_big)) {
                error(reporter, operand_open->expr, "Integer for slicing can not be negative, got '{}'",
                      big_int_to_string(open_big));
                err = true;
            } else {
                open = big_int_to_u64(open_big);
                if (count && *open > *count) {
                    error(reporter, *expression->interval_open,
                          "Index out of range, array count is '{}' but index is '{}'", *count, *open);
                    err = true;
                }
            }
        }
    }
    if (operand_close) {
        if (!operand_close->type->is_integer()) {
            error(reporter, *expression->interval_close, "Expected integer");
            err = true;
        } else if (operand_close->kind == Operand::Kind::CONSTANT &&
                   operand_close->value.as_int().kind == Value::Kind::INTEGER) {
            auto close_big = operand_close->value.int_value;
            if (big_int_is_negative(close_big)) {
                error(reporter, operand_close->expr, "Integer for slicing can not be negative, got '{}'",
                      big_int_to_string(close_big));
                err = true;
            } else {
                close = big_int_to_u64(close_big);
                if (count && *close > *count) {
                    error(reporter, *expression->interval_close,
                          "Index out of range, array count is '{}' but index is '{}'", *count, *close);
                    err = true;
                }
            }
        }
    }

    if (open && close) {
        if (*open >= *close) {
            error(reporter, expression, "Slice start has to be less than slice end, but {} >= {}", *open, *close);
            err = true;
        }
    }

    if (err) {
        operand = Operand{};
        return;
    }
    
    set_type_and_value(expression, operand);
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

bool Typer::check_assignment(TyperContext &/*context*/, const Operand &operand, Type *type) {
    if (operand.kind == Operand::Kind::INVALID || type->is_bad()) {
        return false;
    }

    if (operand.kind == Operand::Kind::TYPE) {
        error(reporter, operand.expr, "Can not assign type");
        return false;
    }

    if (!are_types_the_same(operand.type, type)) {
        error(reporter, operand.expr, "Can not assign value '{}' of type '{}' to type '{}'",
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

    if (operand_expr.kind == Operand::Kind::CONSTANT) {
        auto index = operand_expr.type->get_base_type()->as<StructType>()->index_of_field(entity->as<VariableEntity>());
        operand.kind = Operand::Kind::CONSTANT;
        operand.value = apply_index_operator(parser_, operand_expr.value,
                                             index.expect("we found entity earlier, so it has to be there"));
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
        if (operand.value.as_int().kind == Value::Kind::INVALID) {
            return 0;
        }

        if (big_int_is_negative(operand.value.int_value)) {
            error(reporter, expression, "Array count must be a positive integer, got {}",
                  big_int_to_string(operand.value.int_value));
            return 0;
        }
        // NOTE: For now, count is guranteed to fit in u64 because this is the largest type, but in the future when constants are
        // going to be untyped there will have to be a check for that here (the same is true for check_index_expr and
        // check_slice_expr)
        return big_int_to_u64(operand.value.int_value);
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
            Type *return_type = void_t;
            if (ast_proc->return_type) {
                return_type = check_type(context, *ast_proc->return_type);
            }
            ast_type->type = create_type<ProcedureType>(parameters, return_type);
            return ast_type->type;
        }

        case STRUCT: {
            auto ast_struct = ast_type->as<Ast::StructType>();
            auto members_temp = create_temp_vector<VariableEntity *>(ast_struct->members.size());
            open_scope(context, ast_struct->scope);
            for (auto ast_member : ast_struct->members) {
                auto member_entity = create_entity<VariableEntity>(context.scope, ast_member, ast_member->type,
                                                                   Maybe<Ast::Expression *>{});
                member_entity->state = Entity::State::RESOLVED;
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
        error(reporter, entity->declaration, "Declaration cycle of {}", entity->full_name());
        entity->state = Entity::State::RESOLVED;
        return;
    }
    
    context.decl_path->push_back(entity);

    auto new_context = context;
    new_context.scope = entity->scope;
    new_context.entity = entity;

    entity->state = Entity::State::IN_PROGRESS;
    switch (entity->kind) {
        using enum Entity::Kind;

        case VARIABLE: {
            auto variable = entity->as<VariableEntity>();
            assert(variable->is_global);
            check_variable_decl(new_context, variable);
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

    context.decl_path->pop_back();
    entity->state = Entity::State::RESOLVED;
}

void Typer::check_variable_decl(TyperContext &context, VariableEntity *entity) {    
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

bool Typer::check_cycle_alias(TyperContext &context, const Type *type) {
    if (type == nullptr) {
        return false;
    }
    switch (type->kind) {
        using enum Type::Kind;
        
        case STRUCT:
        case PROCEDURE:
        case BASIC: return false;
        
        case NAMED: {
            auto named = type->as<NamedType>();
            if (check_cycle(context, named->entity)) {
                return true;
            }
            context.decl_path->push_back(named->entity);
            auto found = check_cycle_alias(context, named->type);
            context.decl_path->pop_back();
            return found;
        }
        case ARRAY: {
            auto array = type->as<ArrayType>();
            return check_cycle_alias(context, array->type);
        }
        case SLICE: {
            auto slice = type->as<SliceType>();
            return check_cycle_alias(context, slice->type);
        }
        case POINTER: {
            auto pointer = type->as<PointerType>();
            return check_cycle_alias(context, pointer->type);
        }
    }
    panic("Type not handled");
}

void Typer::check_type_decl(TyperContext &context, NamedTypeEntity *entity) {
    auto declaration = entity->declaration->as<Ast::TypeDeclaration>();
    // Entity type should be set first
    entity->type = create_type<NamedType>(declaration->identifier->token.value, entity);
    auto type = check_type(context, entity->ast_type);
    auto named_type = entity->type->as<NamedType>();
    named_type->type = type;
    if (!type->is<StructType>()) {
        entity->is_alias = true;
        if (check_cycle_alias(context, named_type->type)) {
            named_type->type = bad_t;
        }
    }
}

void Typer::collect_and_check_local_entities(TyperContext &context, std::span<Ast::Statement *> statements) {
    auto entities = create_temp_vector<Entity *>(16);
    for (auto statement : statements) {
        if (statement->is<Ast::DeclarationStatement>()) {
            auto decl = statement->as<Ast::DeclarationStatement>()->declaration;
            switch (decl->kind) {
                using enum Ast::Declaration::Kind;

                case VARIABLE: break;

                case PROCEDURE:
                case CONSTANT:
                case TYPE: {
                    entities.push_back(collect_entity(context, decl));
                    break;
                }

                case FIELD: panic("Should not have a field here"); 
            }
        }
    }

    for (auto entity : entities) {
        check_entity_decl(context, entity);
    }
}

void Typer::check_procedure_body(TyperContext &context, ProcedureEntity *proc) {
    auto new_context = context;
    new_context.entity = proc;
    new_context.scope = proc->inner_scope;

    auto ast_proc = proc->ast_type;
    for (auto ast_parameter : ast_proc->as<Ast::ProcedureType>()->parameters) {
        if (ast_parameter->identifier != nullptr) {
            auto variable = create_entity<VariableEntity>(new_context.scope, ast_parameter, ast_parameter->type,
                                                          Maybe<Ast::Expression *>{});
            variable->type = check_type(new_context, ast_parameter->type);
            add_entity(new_context.scope, variable); 
        }
    }
    
    auto body = proc->declaration->as<Ast::ProcedureDeclaration>()->body;
    auto always_returns = check_statement(new_context, body, proc->inner_scope);
    if (!are_types_the_same(proc->type->get_base_type()->as<ProcedureType>()->return_type, void_t)) {
        if (!always_returns) {
            error(reporter, proc->declaration, "Procedure not always returns");
        }
    }
}

// Returns whether statement always returns
bool Typer::check_statement(TyperContext &context, Ast::Statement *statement, Scope *block_scope) {
    switch (statement->kind) {
        using enum Ast::Statement::Kind;
        
        case BAD:
        case EMPTY:
            return false;
        
        case IF: {
            auto if_statement = statement->as<Ast::IfStatement>();
            
            open_scope(context, if_statement->scope);
            auto operand_if_condition = Operand{};
            check_expr(context, operand_if_condition, if_statement->condition);
            if (operand_if_condition.kind != Operand::Kind::INVALID) {
                if (!are_types_the_same(operand_if_condition.type, bool_t)) {
                    error(reporter, if_statement->condition, "Expression does not evaluate to bool");
                }
            }
            auto true_branch_always_returns = check_statement(context, if_statement->body, if_statement->scope);
            close_scope(context);
            
            auto false_branch_always_returns = false;
            if (if_statement->else_branch) {
                auto &else_branch = *if_statement->else_branch;
                open_scope(context, else_branch.scope);
                false_branch_always_returns = check_statement(context, else_branch.body, else_branch.scope);
                close_scope(context);
            }
            return true_branch_always_returns && false_branch_always_returns;
        }
        case WHILE: {
            auto while_statement = statement->as<Ast::WhileStatement>();
            
            open_scope(context, while_statement->scope);
            while_statement->scope->is_loop = true;
            auto operand_while_condition = Operand{};
            check_expr(context, operand_while_condition, while_statement->condition);
            if (operand_while_condition.kind != Operand::Kind::INVALID) {
                if (!are_types_the_same(operand_while_condition.type, bool_t)) {
                    error(reporter, while_statement->condition, "Expression does not evaluate to bool");
                }
            }
            auto always_returns = check_statement(context, while_statement->body, while_statement->scope);
            close_scope(context);
            
            return always_returns;
        }
        case BLOCK: {
            auto block = statement->as<Ast::BlockStatement>();
            if (block_scope == nullptr) {
                open_scope(context, block->scope);
            } else {
                block->scope = block_scope;
            }

            collect_and_check_local_entities(context, block->body);
            auto always_returns = false;
            for (auto s : block->body) {
                always_returns |= check_statement(context, s);
                // TODO: if we found a statement that always returns and there are more statements produce warning
            }

            if (block_scope == nullptr) {
                close_scope(context);
            }
            return always_returns;
        }
        
        case DECLARATION: {
            auto decl = statement->as<Ast::DeclarationStatement>()->declaration;
            switch (decl->kind) {
                using enum Ast::Declaration::Kind;

                case VARIABLE: {
                    auto ast_variable = decl->as<Ast::VariableDeclaration>();                    
                    auto entity = create_entity_variable(context, ast_variable);
                    check_variable_decl(context, entity);
                    entity->state = Entity::State::RESOLVED;
                    add_entity(context.scope, entity);
                    return false;   
                }

                case PROCEDURE:
                case CONSTANT:
                case TYPE: return false;

                case FIELD: panic("Should not have a field here");
            }
            panic("Declaration not handled");
        }

        case ASSIGNMENT: {
            auto assignment = statement->as<Ast::AssignmentStatement>();

            auto operand_left = Operand{};
            auto operand_right = Operand{};
            check_expr(context, operand_left, assignment->expression);
            check_expr(context, operand_right, assignment->value, operand_left.type);
            if (operand_left.kind == Operand::Kind::INVALID || operand_right.kind == Operand::Kind::INVALID) {
                return false;
            } 

            if (operand_left.kind == Operand::Kind::CONSTANT) {
                error(reporter, assignment->expression, "Can not assign to a constant");
            }
            if (operand_left.kind == Operand::Kind::VALUE) {
                error(reporter, assignment->expression, "Can not assign to an rvalue");
            }

            if (assignment->assign.type != TokenType::assign) {
                if (!check_binary_operator(context, operand_left, operand_right, assignment->assign)) {
                    return false;
                }
            }

            check_assignment(context, operand_right, operand_left.type);
            return false;
        }
        
        case RETURN: {
            auto return_statement = statement->as<Ast::ReturnStatement>();
            auto current_proc = context.entity->as<ProcedureEntity>();
            auto current_proc_return_type = current_proc->type->get_base_type()->as<ProcedureType>()->return_type;

            Type *return_type = void_t;
            if (return_statement->value) {
                auto operand = Operand{};
                check_expr(context, operand, *return_statement->value, current_proc_return_type);
                if (operand.kind == Operand::Kind::INVALID) {
                    return true;
                }
                return_type = operand.type;
            }
            if (!are_types_the_same(return_type, current_proc_return_type)) {
                error(reporter, return_statement, "Procedure returns value of type {}, got value of type {}",
                      type_to_string(current_proc_return_type), type_to_string(return_type));
            }
            return true;
        }

        case EXPRESSION: {
            auto expression = statement->as<Ast::ExpressionStatement>()->expression;
            auto operand = Operand{};
            check_expr(context, operand, expression);
            return false;
        }
        
        case CONTINUE: 
        case BREAK: {
            if (!context.scope->is_loop) {
                error(reporter, statement, "{} is not allowed outside of a loop", statement->start_token().type); 
            }
            return false;
        }
    }
    panic("Statement not handled");
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

    for (auto statement : parser_.ast) {
        if (statement->is<Ast::DeclarationStatement>()) {
            collect_entity(context, statement->as<Ast::DeclarationStatement>()->declaration);
        } else {
            error(reporter, statement, "Expected declaration");
        }
    }
    if (parser_.reporter.any_errors()) {
        return false;
    }

    for (usize i = 0; i < entities_.size(); ++i) {
        check_entity_decl(context, entities_[i]);
    }

    if (parser_.reporter.any_errors()) {
        return false;
    }

    for (usize i = 0; i < procedure_bodies_to_check_.size(); ++i) {
        auto proc = procedure_bodies_to_check_[i];
        check_procedure_body(context, proc);        
    }

    return parser_.reporter.any_errors();
}

} // namespace Typing