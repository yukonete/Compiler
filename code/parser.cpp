#include <algorithm>
#include <cstdio>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "base.h"
#include "lexer.h"
#include "parser.h"
#include "ast.h"

namespace Ast {

static bool is_declaration(Node node) {
    if (std::holds_alternative<Declaration*>(node)) {
        return true;
    }

    auto statement = std::get_if<Statement*>(&node);
    if (statement == nullptr) {
        return false;
    }

    return (*statement)->is<DeclarationStatement>();
};

Parser::Parser(std::string_view input, ArenaAllocator *arena, FILE *log)
    : lexer_{input}, arena_{arena}, log_{log} {
}

Program Parser::parse_program() {
    Program result;
    while (true) {
        const auto token = lexer_.next_token();
        if (lexer_.next_token().type == TokenType::eof) {
            break;
        }

        auto statement = parse_statement();
        if (!is_declaration(statement)) {
            report_error(token, "Expected declaration.");
        }
        result.declarations.push_back(statement);
    }
    result.error_count = error_count_;
    return result;
}

Identifier *Parser::parse_identifier() {
    auto identifier = New<Identifier>(expect_token(TokenType::identifier));
    return identifier;
}

Statement *Parser::parse_statement() {
    const auto token_type = lexer_.next_token().type;
    switch (token_type) {
        using enum TokenType;

        case keyword_var:
        case keyword_fn:
        case keyword_const:
        case keyword_type: {
            auto declaration_statement = New<DeclarationStatement>();
            declaration_statement->declaration = parse_declaration();
            return declaration_statement;
        }

        case keyword_break: {
            auto break_statement =
                New<BreakStatement>(expect_token(TokenType::keyword_break));
            expect_token(TokenType::semicolon);
            return break_statement;
        }

        case keyword_continue: {
            auto continue_statement =
                New<ContinueStatement>(expect_token(TokenType::keyword_continue));
            expect_token(TokenType::semicolon);
            return continue_statement;    
        }

        case keyword_if: return parse_if_statement();
        case keyword_while: return parse_while_statement();
        case keyword_return: return parse_return_statement();
        case open_brace: return parse_block_statement();
        default: break;
    }

    auto expression = parse_expression();
    if (next_token_is(TokenType::assign) ||
        next_token_is(TokenType::plus_assign) ||
        next_token_is(TokenType::minus_assign) ||
        next_token_is(TokenType::divide_assign) ||
        next_token_is(TokenType::multiply_assign) ||
        next_token_is(TokenType::modulo_assign)) {
        auto assignment = New<AssignmentStatement>(lexer_.next_token());
        lexer_.eat_token();
        assignment->assignee = expression;
        assignment->value = parse_expression();
        expect_token(TokenType::semicolon);
        return assignment;
    }
    expect_token(TokenType::semicolon);

    auto statement = New<ExpressionStatement>();
    statement->expression = expression;
    return statement;
}

Declaration *Parser::parse_declaration() {
    Declaration* declaration = nullptr;
    const auto token_type = lexer_.next_token().type;
    switch (token_type) {
        using enum TokenType;

        case keyword_var: {
            declaration = parse_variable_declaration();
            break;
        }

        case keyword_fn: {
            declaration = parse_procedure_declaration();
            break;
        }

        case keyword_const: {
            declaration = parse_constant_declaration();
            break;
        }

        case keyword_type: {
            declaration = parse_type_declaration();
            break;
        }
        
        default: panic("Not a declaration");
    }

    return declaration;
}

ConstDeclaration *Parser::parse_constant_declaration() {
    auto declaration = New<ConstDeclaration>(expect_token(TokenType::keyword_const));
    declaration->identifier = parse_identifier();
    if (next_token_is(TokenType::colon)) {
        lexer_.eat_token();
        declaration->variable_type = parse_type();
    }
    expect_token(TokenType::assign);
    declaration->value = parse_expression();
    expect_token(TokenType::semicolon);
    return declaration;
}

VariableDeclaration *Parser::parse_variable_declaration() {
    auto declaration = New<VariableDeclaration>(expect_token(TokenType::keyword_var));
    declaration->identifier = parse_identifier();
    if (next_token_is(TokenType::colon)) {
        lexer_.eat_token();
        declaration->variable_type = parse_type();
    }

    if (next_token_is(TokenType::assign)) {
        lexer_.eat_token();
        declaration->value = parse_expression();
    }

    if (!declaration->variable_type.has_value() 
        && !declaration->value.has_value()) {
        report_error(declaration->var, "Variable declaration has no type and value."
            " At least one should be specified.");
    }

    expect_token(TokenType::semicolon);
    return declaration;
}

TypeDeclaration *Parser::parse_type_declaration() {
    auto declaration = New<TypeDeclaration>(expect_token(TokenType::keyword_type));
    declaration->identifier = parse_identifier();
    expect_token(TokenType::assign);
    declaration->declared_type = parse_type();
    expect_token(TokenType::semicolon);
    return declaration;
}

ProcedureDeclaration *Parser::parse_procedure_declaration() {
    auto proc = New<ProcedureDeclaration>();
    // For now parse type here, but it should be separate function,
    // so that it can be called when type declaration
    proc->type = New<TypeProcedure>(expect_token(TokenType::keyword_fn));
    proc->identifier = parse_identifier();
    expect_token(TokenType::open_paren);
    bool first_parameter = true;
    proc->type->parameters = parse_until_token<ProcedureParameter>(
        TokenType::close_paren, [this, &first_parameter](auto temp) {
            auto parameter = New<ProcedureParameter>();
            if (!first_parameter) {
                expect_token(TokenType::comma);
            } else {
                first_parameter = false;
            }
            parameter->identifier = parse_identifier();
            expect_token(TokenType::colon);
            parameter->type = parse_type();
            temp->push_back(parameter);
        });
    expect_token(TokenType::return_arrow);
    proc->type->return_type = parse_type();
    proc->body = parse_block_statement();
    return proc;
}

IfStatement *Parser::parse_if_statement() {
    auto statement = New<IfStatement>(expect_token(TokenType::keyword_if));
    statement->condition = parse_expression();
    statement->true_branch_body = parse_statement();
    if (!next_token_is(TokenType::keyword_else)) {
        return statement;
    }

    statement->false_branch = IfStatement::ElseBranch {
        .else_token = expect_token(TokenType::keyword_else),
        .branch_body = parse_statement(),
    };
    return statement;
}

WhileStatement *Parser::parse_while_statement() {
    auto statement = New<WhileStatement>(expect_token(TokenType::keyword_while));
    statement->condition = parse_expression();
    statement->body = parse_statement();
    return statement;
}

Type *Parser::parse_type() {
    Type *type = nullptr;
    const auto &token = lexer_.next_token();
    lexer_.eat_token();

    switch (token.type) {
        using enum TokenType;

        case identifier: {
            auto ident = New<TypeIdentifier>();
            lexer_.uneat_token();
            ident->identifier = parse_identifier();
            type = ident;
            break;
        }

        case TokenType::star: {
            auto pointer = New<TypePointer>(token);
            pointer->points_to = parse_type();
            type = pointer;
            break;
        }

        case keyword_struct: {
            auto type_struct = New<TypeStruct>(token);
            type_struct->open_brace = expect_token(TokenType::open_brace);

            auto members = std::vector<StructMember *>();
            auto declarations = std::vector<Declaration *>();
            while (!(next_token_is(TokenType::close_brace) ||
                     next_token_is(TokenType::eof))) {
                if (next_token_is(TokenType::identifier)) {
                    auto member = New<StructMember>();
                    member->identifier = parse_identifier();
                    expect_token(TokenType::colon);
                    member->type = parse_type();
                    expect_token(TokenType::semicolon);
                    members.push_back(member);
                    continue;
                }
                // Next token is not an identifier, which means that it is not a member
                // Which means it has to be a declaration
                declarations.push_back(parse_declaration());
                auto var_decl = declarations.back()->get_if<VariableDeclaration>();
                if (var_decl.has_value()) {
                    report_error(var_decl.value()->var, 
                        "Variable declarations are not allowed in a struct.");
                }
            }
            type_struct->close_brace = expect_token(TokenType::close_brace);
            
            type_struct->members = arena_->push_array(std::span(members));
            type_struct->declarations = arena_->push_array(std::span(declarations));
            
            type = type_struct;
            break;
        }

        case open_bracket: {
            auto array = New<TypeArray>(token);
            array->element_count = parse_expression();
            array->close_bracket = expect_token(TokenType::close_bracket);
            array->element_type = parse_type();
            type = array;
            break;
        }

        default: {
            type = New<Type>();
            report_error(token, "Token \"{}\" can not be parsed as a type.",
                         token.type);
            break;
        }
    }
    return type;
}

ReturnStatement *Parser::parse_return_statement() {
    auto return_statement = New<ReturnStatement>(expect_token(TokenType::keyword_return));
    if (!next_token_is(TokenType::semicolon)) {
        return_statement->value = parse_expression();
    }
    expect_token(TokenType::semicolon);
    return return_statement;
}

BlockStatement *Parser::parse_block_statement() {
    auto block = New<BlockStatement>(expect_token(TokenType::open_brace));
    block->body =
        parse_until_token<Statement>(TokenType::close_brace, [this](auto temp) {
            const auto statement = parse_statement();
            temp->push_back(statement);
        });
    block->close_brace = lexer_.peek_token(-1);
    return block;
}

// Returns precedence of a binary operator.
static Precedence token_type_to_precedense(TokenType type) {
    switch (type) {
        using enum TokenType;
        case equals:
        case not_equals: return Precedence::equals;

        case less:
        case greater:
        case less_equals:
        case greater_equals: return Precedence::comparison;

        case plus:
        case minus: return Precedence::plus;

        case star:
        case divide:
        case modulo: return Precedence::multiply;

        case dot:
        case open_bracket:
        case open_paren: return Precedence::call;

        default: return Precedence::lowest;
    }
}

Expression *Parser::parse_unary_expression() {
    Expression *expression = nullptr;
    const auto &token = lexer_.next_token();
    lexer_.eat_token();

    switch (token.type) {
        using enum TokenType;

        case open_paren: {
            auto expr = parse_expression();
            expect_token(close_paren);
            expression = expr;
            break;
        }

        case identifier: {
            auto ident = New<IdentifierExpression>();
            lexer_.uneat_token();
            ident->identifier = parse_identifier();;
            expression = ident;
            break;
        }

        case integer: {
            auto integer_literal = New<IntegerLiteralExpression>(token);
            // TODO: Use my own parse integer implementaion
            // For now, this will temporary allocate new string
            integer_literal->value = std::stoll(std::string{token.value});
            expression = integer_literal;
            break;
        }

        case float_literal: {
            auto float_literal = New<FloatLiteralExpression>(token);
            // TODO: Use my own parse float implementaion
            // For now, this will temporary allocate new string
            float_literal->value = std::stod(std::string{token.value});
            expression = float_literal;
            break;
        }

        case keyword_true:
        case keyword_false: {
            auto bool_literal = New<BoolLiteralExpression>(token);
            bool_literal->value = (token.type == keyword_true);
            expression = bool_literal;
            break;
        }

        case minus:
        case bang:
        case ampersand:
        case star: {
            auto unary_operator = New<UnaryOperatorExpression>(token);
            unary_operator->right = parse_expression(Precedence::prefix);
            expression = unary_operator;
            break;
        }

        case string: {
            auto string_literal = New<StringLiteralExpression>(token);
            expression = string_literal;
            break;
        }

        default: {
            expression = New<Expression>();
            report_error(
                token, "Token \"{}\" can not be parsed as a unary expression.",
                token.type);
            break;
        }
    }
    return expression;
}

Expression *Parser::parse_binary_expression(Expression *left) {
    const auto &token = lexer_.next_token();
    lexer_.eat_token();

    if (token.type == TokenType::open_paren) {
        auto call = New<CallOperatorExpression>(token);
        call->callable = left;
        bool first_argument = true;
        call->arguments = parse_until_token<Expression>(
            TokenType::close_paren, [this, &first_argument](auto temp) {
                if (!first_argument) {
                    expect_token(TokenType::comma);
                } else {
                    first_argument = false;
                }
                auto argument = parse_expression();
                temp->push_back(argument);
            });
        call->close_paren = lexer_.peek_token(-1);
        return call;
    }

    if (token.type == TokenType::open_bracket) {
        auto subscript = New<ArraySubscriptExpression>(token);
        subscript->array = left;
        subscript->index = parse_expression();
        subscript->close_bracket = expect_token(TokenType::close_bracket);
        return subscript;
    }

    auto binary_operator = New<BinaryOperatorExpression>(token);
    binary_operator->left = left;
    binary_operator->right =
        parse_expression(token_type_to_precedense(token.type));
    return binary_operator;
}

Expression *Parser::parse_expression(Precedence precedence) {
    auto *left = parse_unary_expression();

    while (precedence < token_type_to_precedense(lexer_.next_token().type)) {
        left = parse_binary_expression(left);
    }

    return left;
}

const Token &Parser::expect_token(TokenType type) {
    const auto &token = lexer_.next_token();
    lexer_.eat_token();
    if (token.type != type) {
        report_error(token, "Expected {}, got {}.", type, token.type);
    }
    return token;
}

bool Parser::next_token_is(TokenType type) {
    return lexer_.next_token().type == type;
}

static std::string indent(int tabs) {
    constexpr auto tab_width = 4;
    return std::string(tab_width * tabs, ' ');
};

std::string node_to_string(Node node, int tabs) {
    return std::visit(Overloaded{
    [&](Statement const *statement) {
        return statement_to_string(statement, tabs);
    },
    [&](Expression const *expression) {
        return expression_to_string(expression, tabs);
    },
    [&](Type const *type) { 
        return type_to_string(type, tabs); 
    },
    [&](Declaration const *declaration) {
        return declaration_to_string(declaration, tabs);
    },
    }, node);
}

std::string type_to_string(const Type *type, int tabs) {
    auto result = std::string{};
    std::visit(Overloaded{
    [&](const TypeIdentifier *type_ident) {
        result = type_ident->identifier->value();
    },
    [&](const TypeStruct *type_struct) {
        result = std::format("{} {{\n", type_struct->struct_token.type);
        for (auto member : type_struct->members) {
            result += std::format("{}{}: {};\n", indent(tabs + 1),
                member->identifier->value(),
                type_to_string(member->type, tabs + 1));
        }
        for (auto declaration : type_struct->declarations) {
            result += std::format("{}{}\n", indent(tabs+1), 
                declaration_to_string(declaration, tabs+1));
        }
        result += indent(tabs);
        result += "}";
    },
    [&](const TypePointer *type_pointer) {
        result = std::format("*{}", type_to_string(type_pointer->points_to, tabs));
    },
    [&](const TypeArray *type_array) {
        result = std::format("[{}]{}", 
            expression_to_string(type_array->element_count, tabs),
            type_to_string(type_array->element_type, tabs));
    },
    [&](const TypeProcedure *type_proc) {
        result += "(";
        for (int i = 0; i < std::ssize(type_proc->parameters); ++i) {
            auto parameter = type_proc->parameters[i];
            result += std::format("{}: {}", parameter->identifier->value(),
                            type_to_string(parameter->type, tabs));
            if (i != std::ssize(type_proc->parameters) - 1) {
                result += ", ";
            }
        }
        result += std::format(") -> {}", 
            type_to_string(type_proc->return_type, tabs));
    },
    }, type->variant);
    return result;
}

std::string statement_to_string(Statement const *type, int tabs) {
    auto result = std::string{};
    std::visit(Overloaded{
    [&](ReturnStatement const *return_statement) {
        result = std::format("{} {};", return_statement->return_token.type,
            expression_to_string(return_statement->value, tabs));
    },
    [&](IfStatement const *if_statement) {
        result = std::format("{} {} {}", if_statement->if_token.type,
            expression_to_string(if_statement->condition, tabs),
            statement_to_string(if_statement->true_branch_body, tabs));
        if (if_statement->false_branch.has_value()) {
            result += std::format(" {} {}",
                if_statement->false_branch.value().else_token.type,
                statement_to_string(if_statement->false_branch.value().branch_body, tabs));
        }
    },
    [&](WhileStatement const *while_statement) {
        result = std::format("{} {} {}", while_statement->while_token.type,
            expression_to_string(while_statement->condition, tabs),
            statement_to_string(while_statement->body, tabs));
    },
    [&](AssignmentStatement const *assignment_statement) {
        result = std::format("{} {} {};", 
            expression_to_string(assignment_statement->assignee, tabs),
            assignment_statement->assign.type,
            expression_to_string(assignment_statement->value, tabs));
    },
    [&](BlockStatement const *block_statement) {
        result = "{\n";
        for (auto statement : block_statement->body) {
            result += std::format("{}{}\n", indent(tabs + 1),
                statement_to_string(statement, tabs + 1));
        }
        result += indent(tabs);
        result += "}";
    },
    [&](ExpressionStatement const *expression_statement) {
        result = std::format("{};", 
            expression_to_string(expression_statement->expression, tabs));
    },
    [&](DeclarationStatement const *declaration_statement) {
        result = std::format("{}", 
            declaration_to_string(declaration_statement->declaration, tabs));
    },
    [&](BreakStatement const *break_statement) {
        result = std::format("{};", break_statement->token.type);
    },
    [&](ContinueStatement const *continue_statement) {
        result = std::format("{};", continue_statement->token.type);
    },
    }, type->variant);
    return result;
}

std::string expression_to_string(Expression const *type, int tabs) {
    auto result = std::string{};
    std::visit(Overloaded{
    [&](IntegerLiteralExpression const *integer_literal) {
        result = std::format("{}", integer_literal->value);
    },
    [&](BoolLiteralExpression const *bool_literal) {
        result = std::format("{}", bool_literal->value);
    },
    [&](IdentifierExpression const *identifier) {
        result = identifier->identifier->value();
    },
    [&](UnaryOperatorExpression const *unary_operator) {
        result = std::format("({}{})", unary_operator->op.type,
            expression_to_string(unary_operator->right, tabs));
    },
    [&](BinaryOperatorExpression const *binary_operator) {
        result = std::format("({}{}{})",
            expression_to_string(binary_operator->left, tabs),
            binary_operator->op.type,
            expression_to_string(binary_operator->right, tabs));
    },
    [&](CallOperatorExpression const *call_operator) {
        auto callable = std::get<IdentifierExpression*>(call_operator->callable->variant);
        result = std::format("{}(", callable->identifier->value());
        for (int i = 0; i < std::ssize(call_operator->arguments); ++i) {
            auto arg = call_operator->arguments[i];
            result += expression_to_string(arg, tabs);

            if (i != std::ssize(call_operator->arguments) - 1) {
                result += ", ";
            }
        }
        result += ')';
    },
    [&](ArraySubscriptExpression const *subscript) {
        result += std::format("{}{}{}{}", 
            expression_to_string(subscript->array, tabs),
            subscript->open_bracket.type,
            expression_to_string(subscript->index, tabs),
            subscript->close_bracket.type);
    },
    [&](StringLiteralExpression const *string) {
        result += std::format("\"{}\"", string->string.value);
    },
    [&](FloatLiteralExpression const *float_literal) {
        result += std::format("{}", float_literal->value);
    },
    }, type->variant);
    return result;
}

std::string declaration_to_string(Declaration const *decl, int tabs) {
    auto result = std::string{};
    std::visit(Overloaded{
    [&](VariableDeclaration const *var_decl) -> void {
        result = std::format("{} {}", var_decl->var.type, var_decl->identifier->value());
        if (var_decl->variable_type.has_value()) {
            result += std::format(": {}", type_to_string(var_decl->variable_type.value(), tabs));
        }
       
        if (var_decl->value.has_value()) {
            result += std::format(" = {}", 
                expression_to_string(var_decl->value.value(), tabs));
        }
        result += ";";
    },
    [&](ConstDeclaration const *const_decl) -> void {
        result = std::format("{} {}", const_decl->const_token.type, const_decl->identifier->value());
        if (const_decl->variable_type.has_value()) {
            result += std::format(": {}", type_to_string(const_decl->variable_type.value(), tabs));
        }

        result += std::format(" = {}",
            expression_to_string(const_decl->value, tabs));
        result += ";";
    },
    [&](ProcedureDeclaration const *procedure_decl) {
        result = std::format("{} {}{} {}", procedure_decl->type->fn.type, procedure_decl->identifier->value(), 
                        type_to_string(procedure_decl->type, tabs),
                        statement_to_string(procedure_decl->body, tabs));
    },
    [&](TypeDeclaration const *type_decl) {
        result = std::format("{} {} = {};", type_decl->type_token.type, type_decl->identifier->value(),
                        type_to_string(type_decl->declared_type, tabs));
    },
    }, decl->variant);
    return result;
}
};