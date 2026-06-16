#include <algorithm>
#include <cstdio>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "base/panic.h"
#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "error.h"

namespace Ast {

bool Parser::parse_program() {
    while (true) {
        auto token = lexer.next_token();

        if (token.type == TokenType::eof) {
            break;
        }

        auto statement = parse_statement();
        if (!statement->is<DeclarationStatement>()) {
            syntax_error(lexer, statement, "Expected declaration");
        }
        ast.push_back(statement);
    }

    return !lexer.any_errors();
}

Identifier *Parser::parse_identifier() {
    return New<Identifier>(expect_token(TokenType::identifier));
}

Statement *Parser::parse_statement() {
    auto token = lexer.next_token();
    switch (token.type) {
        using enum TokenType;

        case keyword_var:
        case keyword_fn:
        case keyword_const:
        case keyword_type: {
            return New<DeclarationStatement>(parse_declaration());
        }

        case keyword_break: {
            auto break_statement =
                New<BreakStatement>(expect_token(TokenType::keyword_break));
            expect_token(TokenType::semicolon);
            return break_statement;
        }

        case keyword_continue: {
            auto continue_statement = New<ContinueStatement>(
                expect_token(TokenType::keyword_continue));
            expect_token(TokenType::semicolon);
            return continue_statement;
        }

        case keyword_if: return parse_if_statement();
        case keyword_while: return parse_while_statement();
        case keyword_return: return parse_return_statement();
        case open_brace: return parse_block_statement();

        // Operands
        case identifier:
        case keyword_true:
        case keyword_false:
        case integer:
        case string:
        case float_literal:
        // Unary operators
        case open_paren:
        case ampersand:
        case star:
        case plus:
        case minus:
        case bang: {
            // Those are essentially "simple" statements
            // Might move that to separate function
            auto expression = parse_expression();
            if (next_token_is(TokenType::assign) ||
                next_token_is(TokenType::plus_assign) ||
                next_token_is(TokenType::minus_assign) ||
                next_token_is(TokenType::divide_assign) ||
                next_token_is(TokenType::multiply_assign) ||
                next_token_is(TokenType::modulo_assign)) {
                auto assign_token = lexer.next_token();
                lexer.eat_token();
                auto value = parse_expression();
                expect_token(TokenType::semicolon);
                return New<AssignmentStatement>(expression, assign_token, value);    
            }

            expect_token(TokenType::semicolon);
            return New<ExpressionStatement>(expression);
        }
        
        case invalid:
        case divide:
        case modulo:
        case plus_assign:
        case minus_assign:
        case multiply_assign:
        case divide_assign:
        case modulo_assign:
        case dot:
        case assign:
        case equals:
        case not_equals:
        case less:
        case greater:
        case less_equals:
        case greater_equals:
        case return_arrow:
        case keyword_struct:
        case close_brace:
        case close_paren:
        case open_bracket:
        case close_bracket:
        case colon:
        case comma:
        case keyword_else:
        case keyword_for:
        case keyword_cast:
        case keyword_transmute: {
            auto token_string = token_type_to_string(token.type);
            if (token.type == TokenType::invalid) {
                token_string = token.value;
            }
            syntax_error(lexer, token, "Unexpected token {}", token_string);
            lexer.eat_token();
            return New<BadStatement>(token);
        }

        case semicolon: {
            lexer.eat_token();
            return New<EmptyStatement>(token);
        }

        case eof: panic("parse_statement called with eof")
    }

    panic("Value of the token.type is not TokenType");
}

Declaration *Parser::parse_declaration() {
    const auto token_type = lexer.next_token().type;
    switch (token_type) {
        using enum TokenType;

        case keyword_var: return parse_variable_declaration();

        case keyword_fn: return parse_procedure_declaration();

        case keyword_const: return parse_constant_declaration();

        case keyword_type: return parse_type_declaration();

        default: panic("parse_declaratin should be called with delcaration");
    }
}

ConstDeclaration *Parser::parse_constant_declaration() {
    auto const_token = expect_token(TokenType::keyword_const);
    auto identifier = parse_identifier();
    auto type = std::optional<Type*>{};
    if (next_token_is(TokenType::colon)) {
        lexer.eat_token();
        type = parse_type();
    }
    expect_token(TokenType::assign);
    auto value = parse_expression();
    expect_token(TokenType::semicolon);
    return New<ConstDeclaration>(const_token, identifier, type, value);
}

VariableDeclaration *Parser::parse_variable_declaration() {
    auto var_token = expect_token(TokenType::keyword_var);
    auto identifier = parse_identifier();

    auto type = std::optional<Type*>{};
    if (next_token_is(TokenType::colon)) {
        lexer.eat_token();
        type = parse_type();
    }

    auto value = std::optional<Expression*>{};
    if (next_token_is(TokenType::assign)) {
        lexer.eat_token();
        value = parse_expression();
    }

    expect_token(TokenType::semicolon);

    if (!type && !value) {
        syntax_error(lexer, var_token, "Variable declaration has no type and value."
            " At least one should be specified");
    }

    return New<VariableDeclaration>(var_token, identifier, type, value);;
}

TypeDeclaration *Parser::parse_type_declaration() {
    auto type_token = expect_token(TokenType::keyword_type);
    auto identifier = parse_identifier();
    expect_token(TokenType::assign);
    auto type = parse_type();
    expect_token(TokenType::semicolon);
    return New<TypeDeclaration>(type_token, identifier, type);
}

ProcedureType *Parser::parse_procedure_type(bool skip_identifier) {
    auto fn_token = expect_token(TokenType::keyword_fn);
    if (skip_identifier) {
        if (!next_token_is(TokenType::identifier) && !lexer.any_errors()) {
            panic("parse_procedure_type was called with skip_identifier = true but "
                  "there was no identifier.");
        }
        expect_token(TokenType::identifier);
    }

    auto open = expect_token(TokenType::open_paren);
    bool first_parameter = true;
    auto parameters_temp = std::vector<Field*>();
    while (!(next_token_is(TokenType::close_paren) ||
                next_token_is(TokenType::eof))) {
        if (!first_parameter) {
            expect_token(TokenType::comma);
        } else {
            first_parameter = false;
        }

        parameters_temp.push_back(parse_field());
    }
    auto parameters = NewArray(std::span{parameters_temp});
    auto close = expect_token(TokenType::close_paren);

    auto return_type = std::optional<Type *>{};
    if (next_token_is(TokenType::return_arrow)) {
        lexer.eat_token();
        return_type = parse_type();
    }

    return New<ProcedureType>(fn_token, open, parameters, close, return_type);
}

ProcedureDeclaration *Parser::parse_procedure_declaration() {
    expect_token(TokenType::keyword_fn);
    auto identifier = parse_identifier();
    lexer.uneat_token(); // Put identifier back (it is going to be skipped in
                          // parse_procedure_type)
    lexer.uneat_token(); // Put fn back
    auto type = parse_procedure_type(true);
    auto body = parse_block_statement();
    return New<ProcedureDeclaration>(identifier, type, body);
}

IfStatement *Parser::parse_if_statement() {
    auto if_token = expect_token(TokenType::keyword_if);
    auto condition = parse_expression();
    auto body = parse_block_statement();
    auto else_branch = std::optional<IfStatement::ElseBranch>{};
    if (next_token_is(TokenType::keyword_else)) {
        else_branch = IfStatement::ElseBranch{
            expect_token(TokenType::keyword_else),
            parse_statement(),
        };
    }
    return New<IfStatement>(if_token, condition, body, else_branch);
}

WhileStatement *Parser::parse_while_statement() {
    auto while_token = expect_token(TokenType::keyword_while);
    auto condition = parse_expression();
    auto body = parse_block_statement();
    return New<WhileStatement>(while_token, condition, body);
}

Field *Parser::parse_field() {
    auto identifier = parse_identifier();
    expect_token(TokenType::colon);
    auto type = parse_type();
    return New<Field>(identifier, type);
}

Type *Parser::parse_type() {
    const auto &token = lexer.next_token();
    lexer.eat_token();

    switch (token.type) {
        using enum TokenType;

        case identifier: {
            lexer.uneat_token();
            auto path_temp = std::vector<Identifier*>();
            while (true) {
                path_temp.push_back(parse_identifier());
                if (next_token_is(TokenType::dot)) {
                    lexer.eat_token();
                    continue;    
                }

                break;
            }
            auto path = NewArray(std::span{path_temp});
            return New<IdentifierType>(path);
        }

        case star: {
            auto pointer_token = token;
            auto type = parse_type();
            return New<PointerType>(pointer_token, type);
        }

        case keyword_struct: {
            auto struct_token = token;

            auto open = expect_token(TokenType::open_brace);
            auto members_temp = std::vector<Field*>();
            auto declarations_temp = std::vector<DeclarationStatement *>();
            while (!(next_token_is(TokenType::close_brace) ||
                     next_token_is(TokenType::eof))) {
                if (next_token_is(TokenType::identifier)) {
                    auto field = parse_field();
                    expect_token(TokenType::semicolon);
                    members_temp.push_back(field);
                    continue;
                }

                // Next token is not an identifier, which means that it is not a
                // member Which means it has to be a declaration, but not a
                // variable declaration
                auto statement = parse_statement();
                if (!statement->is<DeclarationStatement>()) {
                    syntax_error(lexer, statement,
                                 "Expected declaration or struct member");
                }
                declarations_temp.push_back(statement->as<DeclarationStatement>());
            }
            auto members = NewArray(std::span{members_temp});
            auto declarations = NewArray(std::span{declarations_temp});
            auto close = expect_token(TokenType::close_brace);

            return New<StructType>(struct_token, open, members, declarations,
                                   close);
        }

        case keyword_fn: {
            lexer.uneat_token();
            return parse_procedure_type();
        }

        case open_bracket: {
            auto open = token;
            auto count = parse_expression();
            auto close = expect_token(TokenType::close_bracket);
            auto element_type = parse_type();
            return New<ArrayType>(open, count, close, element_type);
        }

        default: {
            syntax_error(lexer, token, "Token \"{}\" can not be parsed as a type",
                         token.type);
            return New<BadType>(token);
        }
    }
}

ReturnStatement *Parser::parse_return_statement() {
    auto return_token = expect_token(TokenType::keyword_return);
    auto value = std::optional<Expression*>{};
    if (!next_token_is(TokenType::semicolon)) {
        value = parse_expression();
    }
    expect_token(TokenType::semicolon);
    return New<ReturnStatement>(return_token, value);
}

BlockStatement *Parser::parse_block_statement() {
    auto open = expect_token(TokenType::open_brace);
    auto statements_temp = std::vector<Statement *>();
    while (!(next_token_is(TokenType::close_brace) ||
             next_token_is(TokenType::eof))) {
        statements_temp.push_back(parse_statement());
    }
    auto statements = NewArray(std::span{statements_temp});
    auto close = expect_token(TokenType::close_brace);
    return New<BlockStatement>(open, statements, close);
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
    const auto &token = lexer.next_token();
    lexer.eat_token();

    switch (token.type) {
        using enum TokenType;

        case open_paren: {
            auto expression = parse_expression();
            expect_token(close_paren);
            return expression;
        }

        case identifier: {
            lexer.uneat_token();
            return New<IdentifierExpression>(parse_identifier());
        }

        case integer: {
            // TODO: Use my own parse integer implementaion
            // For now, this will temporary allocate new string with new
            auto value = std::stoll(std::string{token.value});
            return New<IntegerLiteralExpression>(token, value);
        }

        case float_literal: {
            // TODO: Use my own parse float implementaion
            // For now, this will temporary allocate new string
            auto value = std::stod(std::string{token.value});
            return New<FloatLiteralExpression>(token, value);
        }

        case keyword_true:
        case keyword_false: {
            auto value = (token.type == keyword_true);
            return New<BoolLiteralExpression>(token, value);
        }

        case plus:
        case minus:
        case bang:
        case ampersand:
        case star: {
            auto op = token;
            auto right = parse_expression(Precedence::prefix);
            return New<UnaryOperatorExpression>(op, right);
        }

        case string: return New<StringLiteralExpression>(token);

        default: {
            syntax_error(lexer, token,
                         "Token \"{}\" can not be parsed as a unary expression",
                         token.type);
            return New<BadExpression>(token);
        }
    }
}

Expression *Parser::parse_binary_expression(Expression *left) {
    const auto &token = lexer.next_token();
    lexer.eat_token();

    if (token.type == TokenType::open_paren) {
        auto open = token;
        auto arguments_temp = std::vector<Expression*>();
        bool first_argument = true;
        while (!(next_token_is(TokenType::close_paren) ||
                 next_token_is(TokenType::eof))) {
            if (!first_argument) {
                expect_token(TokenType::comma);
            } else {
                first_argument = false;
            }
            arguments_temp.push_back(parse_expression());
        }
        auto arguments = NewArray(std::span{arguments_temp});
        auto close = expect_token(TokenType::close_paren);
        return New<CallOperatorExpression>(left, open, arguments, close);;
    }

    if (token.type == TokenType::open_bracket) {
        auto open = token;
        auto index = parse_expression();
        auto close = expect_token(TokenType::close_bracket);
        return New<IndexExpression>(left, open, index, close);
    }

    auto op = token;
    auto right = parse_expression(token_type_to_precedense(token.type));
    return New<BinaryOperatorExpression>(left, op, right);
}

Expression *Parser::parse_expression(Precedence precedence) {
    auto *left = parse_unary_expression();

    while (precedence < token_type_to_precedense(lexer.next_token().type)) {
        left = parse_binary_expression(left);
    }

    return left;
}

const Token &Parser::expect_token(TokenType type) {
    const auto &token = lexer.next_token();
    lexer.eat_token();

    if (token.type != type) {
        syntax_error(lexer, token, "Expected {}, got {}", type, token.type);
    }

    return token;
}

bool Parser::next_token_is(TokenType type) {
    return lexer.next_token().type == type;
}

static std::string indent(u64 tabs) {
    constexpr auto tab_width = 4;
    return std::string(tab_width * tabs, ' ');
};

std::string type_to_string(const Type *type, u64 tabs, bool include_fn) {
    auto result = std::string{};
    switch (type->kind) {
        using enum Type::Kind;

        case BAD: {
            result += "BAD_TYPE";
            break;
        }

        case IDENTIFIER: {
            auto type_ident = type->as<IdentifierType>();
            result += type_ident->get_full_type_name();
            break;
        }

        case STRUCT: {
            auto type_struct = type->as<StructType>();
            result += "struct {\n";
            for (const auto &member : type_struct->members) {
                result += std::format("{}{}: {};\n", indent(tabs + 1),
                    member->identifier->token.value,
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
            auto type_pointer = type->as<PointerType>();
            result += std::format("*{}", type_to_string(type_pointer->type, tabs));
            break;
        }

        case ARRAY: {
            auto type_array = type->as<ArrayType>();
            result += std::format("[{}]{}", 
                expression_to_string(type_array->count, tabs),
                type_to_string(type_array->element_type, tabs));
            break;
        }

        case FUNCTION: {
            auto type_function = type->as<ProcedureType>();
            if (include_fn) {
                result += "fn";
            }
            result += "(";
            for (usize i = 0; i < type_function->parameters.size(); ++i) {
                const auto &parameter = type_function->parameters[i];
                result += std::format("{}: {}", parameter->identifier->token.value,
                                type_to_string(parameter->type, tabs));
                if (i != type_function->parameters.size() - 1) {
                    result += ", ";
                }
            }
            result += ')';
            if (type_function->return_type) {
                result += std::format(" -> {}", 
                    type_to_string(*type_function->return_type, tabs));
            }
            break;
        }
    }
    return result;
}

std::string statement_to_string(const Statement *type, u64 tabs, bool block_indent) {
    auto result = std::string{};
    if (!type->is<BlockStatement>() || block_indent) {
        result += indent(tabs);
    }
    switch (type->kind) {
        using enum Statement::Kind;

        case EMPTY: {
            result += ";\n";
            break;
        }

        case BAD: {
            result += "BAD_STATEMENT;";
            break;
        }

        case RETURN: {
            auto return_statement = type->as<ReturnStatement>();
            result += "return";
            if (return_statement->value) {
                result += std::format(" {}", expression_to_string(return_statement->value.value(), tabs));
            } 
            result += ';';
            break;
        }

        case IF: {
            auto if_statement = type->as<IfStatement>();
            result += std::format("if {} {}",
                expression_to_string(if_statement->condition, tabs), 
                statement_to_string(if_statement->body, tabs, false));
            if (if_statement->else_branch) {
                result += std::format(" else {}",
                     statement_to_string(if_statement->else_branch.value().body, tabs, false));
            }
            break;
        }

        case WHILE: {
            auto while_statement = type->as<WhileStatement>();
            result += std::format("while {} {}",
                expression_to_string(while_statement->condition, tabs),
                statement_to_string(while_statement->body, tabs, false));
            break;
        }

        case ASSIGNMENT: {
            auto assignment_statement = type->as<AssignmentStatement>();
            result += std::format("{} {} {};", 
                expression_to_string(assignment_statement->expression, tabs),
                assignment_statement->assign.type,
                expression_to_string(assignment_statement->value, tabs));
            break;
        }

        case BLOCK: {
            auto block_statement = type->as<BlockStatement>();
            result += "{\n";
            for (auto statement : block_statement->body) {
                result += statement_to_string(statement, tabs + 1);
                result += '\n';
            }
            result += indent(tabs);
            result += "}";
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

std::string expression_to_string(const Expression *expression, u64 tabs) {
    auto result = std::string{};
    switch (expression->kind) {
        using enum Expression::Kind;
        
        case BAD: {
            result += "BAD_EXPRESSION";
            break;
        }

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
            result += identifier->identifier->token.value;
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
            result += std::format("({} {} {}",
                expression_to_string(binary_operator->left, tabs),
                binary_operator->op.type,
                expression_to_string(binary_operator->right, tabs));
            if (binary_operator->op.type == TokenType::open_bracket) {
                result += ']';
            }
            result += ')';
            break;
        }

        case INDEX: {
            auto index = expression->as<IndexExpression>();
            result += std::format("{}[{}]",
                expression_to_string(index->expression, tabs),
                expression_to_string(index->index, tabs));
            break;
        }

        case CALL_OPERATOR: {
            auto call_operator = expression->as<CallOperatorExpression>();
            result += std::format("{}(", expression_to_string(call_operator->expression, tabs));
            for (usize i = 0; i < call_operator->arguments.size(); ++i) {
                auto arg = call_operator->arguments[i];
                result += expression_to_string(arg, tabs);

                if (i != call_operator->arguments.size() - 1) {
                    result += ", ";
                }
            }
            result += ')';
            break;
        }

        case STRING_LITERAL: {
            auto string = expression->as<StringLiteralExpression>();
            result += std::format("\"{}\"", string->token.value);
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

std::string declaration_to_string(const Declaration *decl, u64 tabs) {
    auto result = std::string{};
    switch (decl->kind) {
        using enum Declaration::Kind;

        case VARIABLE: {
            auto variable = decl->as<VariableDeclaration>();
            result += std::format("var {}", variable->identifier->token.value);
            if (variable->type) {
                result += std::format(": {}", type_to_string(variable->type.value(), tabs));
            }
        
            if (variable->value) {
                result += std::format(" = {}", 
                    expression_to_string(variable->value.value(), tabs));
            }
            result += ";";
            break;
        }

        case CONSTANT: {
            auto constant = decl->as<ConstDeclaration>();
            result += std::format("const {}", constant->identifier->token.value);
            if (constant->type) {
                result += std::format(": {}", type_to_string(constant->type.value(), tabs));
            }

            result += std::format(" = {}",
                expression_to_string(constant->value, tabs));
            result += ";";
            break;
        }

        case FUNCTION: {
            auto function = decl->as<ProcedureDeclaration>();
            result += std::format("fn {}{} {}", 
                function->identifier->token.value, 
                type_to_string(function->type, tabs, false),
                statement_to_string(function->body, tabs));
            break;
        }

        case TYPE: {
            auto type = decl->as<TypeDeclaration>();
            result += std::format("type {} = {};",
                type->identifier->token.value,
                type_to_string(type->type, tabs));
            break;
        }
    }
    return result;
}
};