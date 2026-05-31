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

Program Parser::parse_program() {
    Program result;
    while (true) {
        if (lexer_.next_token().type == TokenType::eof) {
            break;
        }

        auto statement = parse_statement();
        if (!statement->is<DeclarationStatement>()) {
            report_error(statement->location, "Expected declaration.");
        }
        result.declarations.push_back(statement);
    }
    result.error_count = error_count_;
    return result;
}

Identifier *Parser::parse_identifier() {
    auto identifier_token = expect_token(TokenType::identifier);
    auto identifier = New<Identifier>(identifier_token.start);
    identifier->value = identifier_token.value;
    return identifier;
}

Statement *Parser::parse_statement() {
    auto token = lexer_.next_token();
    switch (token.type) {
        using enum TokenType;

        case keyword_var:
        case keyword_fn:
        case keyword_const:
        case keyword_type: {
            auto declaration = parse_declaration();
            auto declaration_statement = New<DeclarationStatement>(declaration->location);
            declaration_statement->declaration = declaration;
            return declaration_statement;
        }

        case keyword_break: {
            auto break_statement =
                New<BreakStatement>(expect_token(TokenType::keyword_break).start);
            expect_token(TokenType::semicolon);
            return break_statement;
        }

        case keyword_continue: {
            auto continue_statement =
                New<ContinueStatement>(expect_token(TokenType::keyword_continue).start);
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
        const auto &assignment_token = lexer_.next_token(); 
        auto assignment = New<AssignmentStatement>(assignment_token.start);
        assignment->assign_kind = assignment_token.type;
        lexer_.eat_token();
        assignment->assignee = expression;
        assignment->value = parse_expression();
        expect_token(TokenType::semicolon);
        return assignment;
    }
    expect_token(TokenType::semicolon);

    auto location = token.start;
    if (expression != nullptr) {
        location = expression->location;
    }
    auto statement = New<ExpressionStatement>(location);
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
    auto declaration = New<ConstDeclaration>(expect_token(TokenType::keyword_const).start);
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
    auto declaration = New<VariableDeclaration>(expect_token(TokenType::keyword_var).start);
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
        report_error(declaration->location, "Variable declaration has no type and value."
            " At least one should be specified.");
    }

    expect_token(TokenType::semicolon);
    return declaration;
}

TypeDeclaration *Parser::parse_type_declaration() {
    auto declaration = New<TypeDeclaration>(expect_token(TokenType::keyword_type).start);
    declaration->identifier = parse_identifier();
    expect_token(TokenType::assign);
    declaration->declared_type = parse_type();
    expect_token(TokenType::semicolon);
    return declaration;
}

TypeProcedure *Parser::parse_procedure_type(bool skip_identifier) {
    auto type = New<TypeProcedure>(expect_token(TokenType::keyword_fn).start);
    if (skip_identifier) {
        if (!next_token_is(TokenType::identifier)) {
            panic("Called parse_procedure_type with skip_identifier = true but "
                  "there was no identifier.");
        }
        lexer_.eat_token();
    }

    expect_token(TokenType::open_paren);
    bool first_parameter = true;
    auto parameters = std::vector<Field*>();
    while (!(next_token_is(TokenType::close_paren) ||
                next_token_is(TokenType::eof))) {
        if (!first_parameter) {
            expect_token(TokenType::comma);
        } else {
            first_parameter = false;
        }

        auto identifier = parse_identifier();
        auto field = New<Field>(identifier->location);
        field->identifier = identifier;
        expect_token(TokenType::colon);
        field->type = parse_type();
        parameters.push_back(field);
    }
    expect_token(TokenType::close_paren);

    type->parameters = NewArray(std::span{parameters});

    expect_token(TokenType::return_arrow);
    type->return_type = parse_type();
    return type;
}

ProcedureDeclaration *Parser::parse_procedure_declaration() {
    auto proc = New<ProcedureDeclaration>(expect_token(TokenType::keyword_fn).start);
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

    return NewArray(std::span{statements});
}

IfStatement *Parser::parse_if_statement() {
    auto statement = New<IfStatement>(expect_token(TokenType::keyword_if).start);
    statement->condition = parse_expression();
    statement->true_branch_body = parse_statements_sequence();
    if (!next_token_is(TokenType::keyword_else)) {
        return statement;
    }

    statement->else_branch = IfStatement::ElseBranch {
        .else_location = expect_token(TokenType::keyword_else).start,
        .body = parse_statements_sequence(),
    };
    return statement;
}

WhileStatement *Parser::parse_while_statement() {
    auto statement = New<WhileStatement>(expect_token(TokenType::keyword_while).start);
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
            auto ident = New<TypeIdentifier>(token.start);
            lexer_.uneat_token();
            auto path = std::vector<Identifier*>();
            while (true) {
                path.push_back(parse_identifier());
                if (next_token_is(TokenType::dot)) {
                    lexer_.eat_token();
                    continue;    
                }
                
                break;
            }
            ident->identifier = NewArray(std::span{path});
            type = ident;
            break;
        }

        case TokenType::star: {
            auto pointer = New<TypePointer>(token.start);
            pointer->points_to = parse_type();
            type = pointer;
            break;
        }

        case keyword_struct: {
            auto type_struct = New<TypeStruct>(token.start);
            expect_token(TokenType::open_brace);

            auto members = std::vector<Field*>();
            auto declarations = std::vector<DeclarationStatement *>();
            while (!(next_token_is(TokenType::close_brace) ||
                     next_token_is(TokenType::eof))) {
                if (next_token_is(TokenType::identifier)) {
                    auto identifier = parse_identifier();
                    auto field = New<Field>(identifier->location);
                    field->identifier = identifier;
                    expect_token(TokenType::colon);
                    field->type = parse_type();
                    expect_token(TokenType::semicolon);
                    members.push_back(field);
                    continue;
                }
                // Next token is not an identifier, which means that it is not a member
                // Which means it has to be a declaration, but not a variable declaration
                auto statement = parse_statement();
                if (!statement->is<DeclarationStatement>()) {
                    report_error(statement->location, "Expected declaration or struct member.");
                } else {
                    declarations.push_back(statement->as<DeclarationStatement>());
                    auto decl = declarations.back()->declaration;
                    if (decl->is<VariableDeclaration>()) {
                        report_error(decl->as<VariableDeclaration>()->location, 
                            "Variable declarations are not allowed inside of a struct.");
                    }
                }
            }
            
            expect_token(TokenType::close_brace);
            type_struct->members = NewArray(std::span(members));
            type_struct->declarations = NewArray(std::span(declarations));
            
            type = type_struct;
            break;
        }

        case keyword_fn: {
            lexer_.uneat_token();
            type = parse_procedure_type();
            break;
        }

        case open_bracket: {
            auto array = New<TypeArray>(token.start);
            array->element_count = parse_expression();
            expect_token(TokenType::close_bracket);
            array->element_type = parse_type();
            type = array;
            break;
        }

        default: {
            report_error(token.start, "Token \"{}\" can not be parsed as a type.",
                         token.type);
            break;
        }
    }
    return type;
}

ReturnStatement *Parser::parse_return_statement() {
    auto return_statement = New<ReturnStatement>(expect_token(TokenType::keyword_return).start);
    if (!next_token_is(TokenType::semicolon)) {
        return_statement->value = parse_expression();
    }
    expect_token(TokenType::semicolon);
    return return_statement;
}

BlockStatement *Parser::parse_block_statement() {
    auto block = New<BlockStatement>(expect_token(TokenType::open_brace).start);
    block->body = parse_statements_sequence();
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
            lexer_.uneat_token();
            auto ident = New<IdentifierExpression>(token.start);
            ident->identifier = parse_identifier();;
            expression = ident;
            break;
        }

        case integer: {
            auto integer_literal = New<IntegerLiteralExpression>(token.start);
            // TODO: Use my own parse integer implementaion
            // For now, this will temporary allocate new string
            integer_literal->value = std::stoll(std::string{token.value});
            expression = integer_literal;
            break;
        }

        case float_literal: {
            auto float_literal = New<FloatLiteralExpression>(token.start);
            // TODO: Use my own parse float implementaion
            // For now, this will temporary allocate new string
            float_literal->value = std::stod(std::string{token.value});
            expression = float_literal;
            break;
        }

        case keyword_true:
        case keyword_false: {
            auto bool_literal = New<BoolLiteralExpression>(token.start);
            bool_literal->value = (token.type == keyword_true);
            expression = bool_literal;
            break;
        }

        case minus:
        case bang:
        case ampersand:
        case star: {
            auto unary_operator = New<UnaryOperatorExpression>(token.start);
            unary_operator->op = token.type;
            unary_operator->right = parse_expression(Precedence::prefix);
            expression = unary_operator;
            break;
        }

        case string: {
            auto string_literal = New<StringLiteralExpression>(token.start);
            string_literal->string = token.value;
            expression = string_literal;
            break;
        }

        default: {
            report_error(
                token.start, "Token \"{}\" can not be parsed as a unary expression.",
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
        auto call = New<CallOperatorExpression>(token.start);
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
            arguments.push_back(parse_expression());
        }

        expect_token(TokenType::close_paren);
        call->arguments = NewArray(std::span{arguments});
        return call;
    }

    if (token.type == TokenType::open_bracket) {
        auto subscript = New<BinaryOperatorExpression>(token.start);
        subscript->op = token.type;
        subscript->left = left;
        subscript->right = parse_expression();
        expect_token(TokenType::close_bracket);
        return subscript;
    }

    auto binary_operator = New<BinaryOperatorExpression>(token.start);
    binary_operator->op = token.type;
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
        report_error(token.start, "Expected {}, got {}.", type, token.type);
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

std::string type_to_string(const Type *type, int tabs, bool include_fn) {
    auto result = std::string{};
    switch (type->kind) {
        using enum Type::Kind;

        case IDENTIFIER: {
            auto type_ident = type->as<TypeIdentifier>();
            result += type_ident->get_full_type_name();
            break;
        }

        case STRUCT: {
            auto type_struct = type->as<TypeStruct>();
            result += "struct {\n";
            for (const auto &member : type_struct->members) {
                result += std::format("{}{}: {};\n", indent(tabs + 1),
                    member->identifier->value,
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
                const auto &parameter = type_function->parameters[i];
                result += std::format("{}: {}", parameter->identifier->value,
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
                assignment_statement->assign_kind,
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
            result += identifier->identifier->value;
            break;
        }

        case UNARY_OPERATOR: {
            auto unary_operator = expression->as<UnaryOperatorExpression>();
            result += std::format("({}{})", unary_operator->op,
                expression_to_string(unary_operator->right, tabs));
            break;
        }

        case BINARY_OPERATOR: {
            auto binary_operator = expression->as<BinaryOperatorExpression>();
            result += std::format("({}{}{}",
                expression_to_string(binary_operator->left, tabs),
                binary_operator->op,
                expression_to_string(binary_operator->right, tabs));
            if (binary_operator->op == TokenType::open_bracket) {
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
            result += std::format("\"{}\"", string->string);
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
            result += std::format("var {}", variable->identifier->value);
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
            result += std::format("const {}", constant->identifier->value);
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
                function->identifier->value, 
                type_to_string(function->type, tabs, false),
                statements_to_string(function->body, tabs));
            break;
        }

        case TYPE: {
            auto type = decl->as<TypeDeclaration>();
            result += std::format("type {} = {};",
                type->identifier->value,
                type_to_string(type->declared_type, tabs));
            break;
        }
    }
    return result;
}
};