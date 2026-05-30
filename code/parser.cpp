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
        if (!statement->is<DeclarationStatement>()) {
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

TypeProcedure *Parser::parse_procedure_type(bool skip_identifier) {
    auto type = New<TypeProcedure>(expect_token(TokenType::keyword_fn));
    if (skip_identifier) {
        if (!next_token_is(TokenType::identifier)) {
            panic("Called parse_procedure_type with skip_identifier = true but "
                  "there was no identifier.");
        }
        lexer_.eat_token();
    }

    expect_token(TokenType::open_paren);
    bool first_parameter = true;
    auto parameters = std::vector<ProcedureParameter*>();
    while (!(next_token_is(TokenType::close_paren) ||
                next_token_is(TokenType::eof))) {
        auto parameter = New<ProcedureParameter>();
        if (!first_parameter) {
            expect_token(TokenType::comma);
        } else {
            first_parameter = false;
        }
        parameter->identifier = parse_identifier();
        expect_token(TokenType::colon);
        parameter->type = parse_type();
        parameters.push_back(parameter);
    }
    expect_token(TokenType::close_paren);

    type->parameters = arena_->push_array(std::span{parameters});

    expect_token(TokenType::return_arrow);
    type->return_type = parse_type();
    return type;
}

ProcedureDeclaration *Parser::parse_procedure_declaration() {
    auto proc = New<ProcedureDeclaration>();
    expect_token(TokenType::keyword_fn);
    proc->identifier = parse_identifier();
    lexer_.uneat_token(); // Put identifier back (it is going to be skipped in
                           // parse_procedure_type)
    lexer_.uneat_token(); // Put fn back
    proc->type = parse_procedure_type(true);
    proc->body = parse_statements_sequence();
    return proc;
}

std::span<Statement*> Parser::parse_statements_sequence() {
    expect_token(TokenType::open_brace);

    auto statements = std::vector<Statement *>();
    while (!(next_token_is(TokenType::close_brace) ||
             next_token_is(TokenType::eof))) {
        auto statement = parse_statement();
        statements.push_back(statement);
    }
    expect_token(TokenType::close_brace);

    return arena_->push_array(std::span{statements});
}

IfStatement *Parser::parse_if_statement() {
    auto statement = New<IfStatement>(expect_token(TokenType::keyword_if));
    statement->condition = parse_expression();
    statement->true_branch_body = parse_statements_sequence();
    if (!next_token_is(TokenType::keyword_else)) {
        return statement;
    }

    statement->else_branch = IfStatement::ElseBranch {
        .else_token = expect_token(TokenType::keyword_else),
        .body = parse_statements_sequence(),
    };
    return statement;
}

WhileStatement *Parser::parse_while_statement() {
    auto statement = New<WhileStatement>(expect_token(TokenType::keyword_while));
    statement->condition = parse_expression();
    statement->body = parse_statements_sequence();
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
            auto declarations = std::vector<DeclarationStatement *>();
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
                // Which means it has to be a declaration, but not a variable declaration
                auto statement = parse_statement();
                if (!statement->is<DeclarationStatement>()) {
                    // TODO: I need a token here but i can not get it,
                    // so i need to store location in a base class 
                    report_error({}, "Expected declaration or struct member.");
                } else {
                    declarations.push_back(statement->as<DeclarationStatement>());
                    auto decl = declarations.back()->declaration;
                    if (decl->is<VariableDeclaration>()) {
                        report_error(decl->as<VariableDeclaration>()->var, 
                            "Variable declarations are not allowed inside of a struct.");
                    }
                }
            }
            type_struct->close_brace = expect_token(TokenType::close_brace);
            
            type_struct->members = arena_->push_array(std::span(members));
            type_struct->declarations = arena_->push_array(std::span(declarations));
            
            type = type_struct;
            break;
        }

        case keyword_fn: {
            lexer_.uneat_token();
            type = parse_procedure_type();
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
    block->body = parse_statements_sequence();
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

        auto arguments = std::vector<Expression*>();
        bool first_argument = true;
        while (!(next_token_is(TokenType::close_paren) ||
                 next_token_is(TokenType::eof))) {
            if (!first_argument) {
                expect_token(TokenType::comma);
            } else {
                first_argument = false;
            }
            auto argument = parse_expression();
            arguments.push_back(argument);
        }
        call->close_paren = expect_token(TokenType::close_paren);
        call->arguments = arena_->push_array(std::span{arguments});
        return call;
    }

    if (token.type == TokenType::open_bracket) {
        auto subscript = New<BinaryOperatorExpression>(token);
        subscript->left = left;
        subscript->right = parse_expression();
        expect_token(TokenType::close_bracket);
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
        return statement_to_string(statement, tabs);;
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

std::string type_to_string(const Type *type, int tabs, bool include_fn) {
    auto result = std::string{};
    switch (type->kind) {
        using enum Type::Kind;

        case IDENTIFIER: {
            auto type_ident = type->as<TypeIdentifier>();
            result += type_ident->identifier->value();
            break;
        }

        case STRUCT: {
            auto type_struct = type->as<TypeStruct>();
            result += std::format("{} {{\n", type_struct->struct_token.type);
            for (auto member : type_struct->members) {
                result += std::format("{}{}: {};\n", indent(tabs + 1),
                    member->identifier->value(),
                    type_to_string(member->type, tabs + 1));
            }
            for (auto declaration : type_struct->declarations) {
                result += std::format("{}\n", statement_to_string(declaration, tabs+1));
            }
            result += indent(tabs);
            result += "}";
            break;
        }

        case POINTER: {
            auto type_pointer = type->as<TypePointer>();
            result += std::format("*{}", type_to_string(type_pointer->points_to, tabs));
            break;
        }

        case ARRAY: {
            auto type_array = type->as<TypeArray>();
            result += std::format("[{}]{}", 
                expression_to_string(type_array->element_count, tabs),
                type_to_string(type_array->element_type, tabs));
            break;
        }

        case FUNCTION: {
            auto type_function = type->as<TypeProcedure>();
            if (include_fn) {
                result += "fn";
            }
            result += "(";
            for (int i = 0; i < std::ssize(type_function->parameters); ++i) {
                auto parameter = type_function->parameters[i];
                result += std::format("{}: {}", parameter->identifier->value(),
                                type_to_string(parameter->type, tabs));
                if (i != std::ssize(type_function->parameters) - 1) {
                    result += ", ";
                }
            }
            result += std::format(") -> {}", 
                type_to_string(type_function->return_type, tabs));
            break;
        }
    }
    return result;
}

static std::string statements_to_string(std::span<Statement*> statements, int tabs) {
    auto result = std::string{"{\n"};
    for (auto statement : statements) {
        result += statement_to_string(statement, tabs + 1);
        result += '\n';
    }
    result += indent(tabs);
    result += "}";
    return result;
} 

std::string statement_to_string(const Statement *type, int tabs) {
    auto result = indent(tabs);
    switch (type->kind) {
        using enum Statement::Kind;

        case RETURN: {
            auto return_statement = type->as<ReturnStatement>();
            result += std::format("return {};", 
                expression_to_string(return_statement->value, tabs));
            break;
        }

        case IF: {
            auto if_statement = type->as<IfStatement>();
            result += std::format("if {} {}",
                expression_to_string(if_statement->condition, tabs), 
                statements_to_string(if_statement->true_branch_body, tabs));
            if (if_statement->else_branch.has_value()) {
                result += std::format(" else {}",
                     statements_to_string(if_statement->else_branch.value().body, tabs));
            }
            break;
        }

        case WHILE: {
            auto while_statement = type->as<WhileStatement>();
            result += std::format("while {} {}",
                expression_to_string(while_statement->condition, tabs),
                statements_to_string(while_statement->body, tabs));
            break;
        }

        case ASSIGNMENT: {
            auto assignment_statement = type->as<AssignmentStatement>();
            result += std::format("{} {} {};", 
                expression_to_string(assignment_statement->assignee, tabs),
                assignment_statement->assign.type,
                expression_to_string(assignment_statement->value, tabs));
            break;
        }

        case BLOCK: {
            auto block_statement = type->as<BlockStatement>();
            result += statements_to_string(block_statement->body, tabs);
            break;
        }
    
        case EXPRESSION: {
            auto expression_statement = type->as<ExpressionStatement>();
            result += std::format("{};", 
                expression_to_string(expression_statement->expression, tabs));
            break;
        }

        case DECLARATION: {
            auto declaration_statement = type->as<DeclarationStatement>();
            result += std::format("{}", 
                declaration_to_string(declaration_statement->declaration, tabs));
            break;
        }

        case BREAK: {
            result += "break;";
            break;
        }

        case CONTINUE: {
            result += "continue;";
            break;
        }
    }
    return result;
}

std::string expression_to_string(const Expression *expression, int tabs) {
    auto result = std::string{};
    switch (expression->kind) {
        using enum Expression::Kind;
        
        case INTEGER_LITERAL: {
            auto integer_literal = expression->as<IntegerLiteralExpression>();
            result += std::format("{}", integer_literal->value);
            break;
        }

        case BOOL_LITERAL: {
            auto bool_literal = expression->as<BoolLiteralExpression>();
            result += std::format("{}", bool_literal->value);
            break;
        }

        case IDENTIFIER: {
            auto identifier = expression->as<IdentifierExpression>();
            result += identifier->identifier->value();
            break;
        }

        case UNARY_OPERATOR: {
            auto unary_operator = expression->as<UnaryOperatorExpression>();
            result += std::format("({}{})", unary_operator->op.type,
                expression_to_string(unary_operator->right, tabs));
            break;
        }

        case BINARY_OPERATOR: {
            auto binary_operator = expression->as<BinaryOperatorExpression>();
            result += std::format("({}{}{}",
                expression_to_string(binary_operator->left, tabs),
                binary_operator->op.type,
                expression_to_string(binary_operator->right, tabs));
            if (binary_operator->op.type == TokenType::open_bracket) {
                result += ']';
            }
            result += ')';
            break;
        }

        case CALL_OPERATOR: {
            auto call_operator = expression->as<CallOperatorExpression>();
            result += std::format("{}(", expression_to_string(call_operator->callable, tabs));
            for (int i = 0; i < std::ssize(call_operator->arguments); ++i) {
                auto arg = call_operator->arguments[i];
                result += expression_to_string(arg, tabs);

                if (i != std::ssize(call_operator->arguments) - 1) {
                    result += ", ";
                }
            }
            result += ')';
            break;
        }

        case STRING_LITERAL: {
            auto string = expression->as<StringLiteralExpression>();
            result += std::format("\"{}\"", string->string.value);
            break;
        }

        case FLOAT_LITERAL: {
            auto float_literal = expression->as<FloatLiteralExpression>();
            result += std::format("{}", float_literal->value);
            break;
        }
    }
    return result;
}

std::string declaration_to_string(const Declaration *decl, int tabs) {
    auto result = std::string{};
    switch (decl->kind) {
        using enum Declaration::Kind;

        case VARIABLE: {
            auto variable = decl->as<VariableDeclaration>();
            result += std::format("{} {}", variable->var.type, variable->identifier->value());
            if (variable->variable_type.has_value()) {
                result += std::format(": {}", type_to_string(variable->variable_type.value(), tabs));
            }
        
            if (variable->value.has_value()) {
                result += std::format(" = {}", 
                    expression_to_string(variable->value.value(), tabs));
            }
            result += ";";
            break;
        }

        case CONSTANT: {
            auto constant = decl->as<ConstDeclaration>();
            result += std::format("{} {}", constant->const_token.type, constant->identifier->value());
            if (constant->variable_type.has_value()) {
                result += std::format(": {}", type_to_string(constant->variable_type.value(), tabs));
            }

            result += std::format(" = {}",
                expression_to_string(constant->value, tabs));
            result += ";";
            break;
        }

        case FUNCTION: {
            auto function = decl->as<ProcedureDeclaration>();
            result += std::format("fn {}{} {}", 
                function->identifier->value(), 
                type_to_string(function->type, tabs, false),
                statements_to_string(function->body, tabs));
            break;
        }

        case TYPE: {
            auto type = decl->as<TypeDeclaration>();
            result += std::format("type {} = {};",
                type->identifier->value(),
                type_to_string(type->declared_type, tabs));
            break;
        }
    }
    return result;
}
};