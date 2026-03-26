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

using namespace Ast;

static bool is_declaration(Node *node) {
    if (node->type >= NodeType::declaration_variable &&
        node->type <= NodeType::declaration_type) {
        return true;
    }
    return false;
};

Parser::Parser(std::string_view input, Arena *arena, FILE *log)
    : lexer_{input}, arena_{arena}, log_{log} {
}

Program Parser::parse_program() {
    Program result;
    while (true) {
        const auto token = lexer_.next_token();
        if (lexer_.next_token().type == TokenType::invalid) {
            break;
        }

        auto statement = parse_statement();
        if (!is_declaration(statement)) {
            log_diagnostic(token, "Expected declaration.");
            statement->type = NodeType::invalid;
        }
        result.declarations.push_back(
            reinterpret_cast<Declaration *>(statement));
    }
    result.error_count = error_count_;
    return result;
}

Statement *Parser::parse_statement() {
    const auto token_type = lexer_.next_token().type;
    switch (token_type) {
        using enum TokenType;

        case identifier: {
            const auto next_token_type = lexer_.peek_token(1).type;
            if (next_token_type == static_cast<TokenType>(':')) {
                return parse_variable_declaration();
            }
            if (next_token_type == static_cast<TokenType>('=')) {
                return parse_assignment_statement();
            }
            break;
        };

        case keyword_proc: return parse_procedure_declaration();
        case keyword_const: return parse_constant_declaration();
        case keyword_type: return parse_type_declaration();
        case keyword_if: return parse_if_statement();
        case keyword_while: return parse_while_statement();
        case keyword_return: return parse_return_statement();
        case TokenType{'{'}: return parse_block_statement();
    }
    return parse_expression_statement();
}

ConstDeclaration *Parser::parse_constant_declaration() {
    auto declaration = New<ConstDeclaration>();
    expect_token(TokenType::keyword_const);
    declaration->identifier = expect_token(TokenType::identifier);
    expect_token(TokenType{':'});
    declaration->variable_type = parse_type();
    expect_token(TokenType{'='});
    declaration->value = parse_expression();
    expect_token(TokenType{';'});
    return declaration;
}

VariableDeclaration *Parser::parse_variable_declaration() {
    auto statement = New<VariableDeclaration>();
    statement->identifier = expect_token(TokenType::identifier);
    expect_token(TokenType{':'});
    statement->variable_type = parse_type();

    if (next_token_is(TokenType{'='})) {
        lexer_.eat_token();
        statement->value = parse_expression();
    }

    expect_token(TokenType{';'});
    return statement;
}

TypeDeclaration *Parser::parse_type_declaration() {
    auto declaration = New<TypeDeclaration>();
    expect_token(TokenType::keyword_type);
    declaration->identifier = expect_token(TokenType::identifier);
    expect_token(TokenType{'='});
    declaration->declared_type = parse_type();
    expect_token(TokenType{';'});
    return declaration;
}

ProcedureDeclaration *Parser::parse_procedure_declaration() {
    auto proc = New<ProcedureDeclaration>();
    expect_token(TokenType::keyword_proc);
    proc->identifier = expect_token(TokenType::identifier);
    expect_token(TokenType{'('});
    std::vector<ProcedureParameter *> temp;
    bool first_parameter = true;
    while (
        !(next_token_is(TokenType{')'}) || next_token_is(TokenType::invalid))) {
        auto parameter = New<ProcedureParameter>();
        if (!first_parameter) {
            expect_token(TokenType{','});
        } else {
            first_parameter = false;
        }
        parameter->identifier = expect_token(TokenType::identifier);
        expect_token(TokenType{':'});
        parameter->type = parse_type();
        temp.push_back(parameter);
    }
    expect_token(TokenType{')'});
    proc->parameters = arena_->push_array<ProcedureParameter *>(temp.size());
    std::ranges::copy(temp, proc->parameters.begin());
    expect_token(TokenType::return_arrow);
    proc->return_type = parse_type();
    proc->body = parse_block_statement();
    return proc;
}

ExpressionStatement *Parser::parse_expression_statement() {
    auto statement = New<ExpressionStatement>();
    statement->expression = parse_expression();
    expect_token(TokenType{';'});
    return statement;
}

IfStatement *Parser::parse_if_statement() {
    auto statement = New<IfStatement>();
    expect_token(TokenType::keyword_if);
    statement->condition = parse_expression();
    statement->true_branch = parse_statement();
    if (!next_token_is(TokenType::keyword_else)) {
        return statement;
    }

    lexer_.eat_token();
    statement->false_branch = parse_statement();
    return statement;
}

WhileStatement *Parser::parse_while_statement() {
    auto statement = New<WhileStatement>();
    expect_token(TokenType::keyword_while);
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
            ident->identifier = token;
            type = ident;
            break;
        }
        case TokenType{'*'}: {
            auto pointer = New<TypePointer>();
            pointer->points_to = parse_type();
            type = pointer;
            break;
        }
        case keyword_struct: {
            auto st = New<TypeStruct>();
            expect_token(TokenType{'{'});
            std::vector<StructMember *> temp;
            temp.reserve(16);
            while (!(next_token_is(TokenType{'}'}) ||
                     next_token_is(TokenType::invalid))) {
                auto member = New<StructMember>();
                member->identifier = expect_token(TokenType::identifier);
                expect_token(TokenType{':'});
                member->type = parse_type();
                expect_token(TokenType{';'});
                temp.push_back(member);
            }
            expect_token(TokenType{'}'});
            st->members = arena_->push_array<StructMember *>(temp.size());
            std::ranges::copy(temp, st->members.begin());
            type = st;
            break;
        }
        default: {
            type = New<Type>(NodeType::invalid);
            log_diagnostic(token, "Token \"{}\" can not be parsed as a type.",
                           token.type);
            break;
        }
    }
    return type;
}

AssignmentStatement *Parser::parse_assignment_statement() {
    auto statement = New<AssignmentStatement>();
    statement->identifier = expect_token(TokenType::identifier);
    expect_token(TokenType{'='});
    statement->value = parse_expression();
    expect_token(TokenType{';'});
    return statement;
}

ReturnStatement *Parser::parse_return_statement() {
    auto ret = New<ReturnStatement>();
    expect_token(TokenType::keyword_return);
    if (!next_token_is(TokenType{';'})) {
        ret->value = parse_expression();
    }
    expect_token(TokenType{';'});
    return ret;
}

BlockStatement *Parser::parse_block_statement() {
    auto block = New<BlockStatement>();
    expect_token(TokenType{'{'});
    std::vector<Statement *> temp;
    temp.reserve(16);
    while (
        !(next_token_is(TokenType{'}'}) || next_token_is(TokenType::invalid))) {
        const auto statement = parse_statement();
        temp.push_back(statement);
    }
    expect_token(TokenType{'}'});
    block->body = arena_->push_array<Statement *>(temp.size());
    std::ranges::copy(temp, block->body.begin());
    return block;
}

// Returns precedence of a binary operator.
static Precedence TokenTypeToPrecedense(TokenType type) {
    switch (type) {
        using enum TokenType;
        case equals:
        case not_equals: return Precedence::equals;

        case TokenType{'<'}:
        case TokenType{'>'}:
        case less_equals:
        case greater_equals: return Precedence::comparison;

        case TokenType{'+'}:
        case TokenType{'-'}: return Precedence::plus;

        case TokenType{'*'}:
        case TokenType{'/'}:
        case TokenType{'%'}: return Precedence::multiply;

        case TokenType{'('}: return Precedence::call;

        default: return Precedence::lowest;
    }
}

Expression *Parser::parse_unary_expression() {
    Expression *expression = nullptr;
    const auto &token = lexer_.next_token();
    lexer_.eat_token();
    switch (token.type) {
        using enum TokenType;

        case TokenType{'('}: {
            auto expr = parse_expression();
            expect_token(TokenType{')'});
            expression = expr;
            break;
        }

        case identifier: {
            auto ident = New<IdentifierExpression>();
            ident->identifier = token;
            expression = ident;
            break;
        }

        case integer: {
            auto integer_literal = New<IntegerLiteral>();
            integer_literal->value = token;
            expression = integer_literal;
            break;
        }

        case keyword_true:
        case keyword_false: {
            auto bool_literal = New<BoolLiteral>();
            bool_literal->value = (token.type == keyword_true);
            expression = bool_literal;
            break;
        }

        case TokenType{'-'}:
        case TokenType{'!'}:
        case TokenType{'&'}:
        case TokenType{'*'}: {
            auto unary_operator = New<UnaryOperator>();
            unary_operator->op = token.type;
            unary_operator->right = parse_expression(Precedence::prefix);
            expression = unary_operator;
            break;
        }

        default: {
            expression = New<Expression>(NodeType::invalid);
            log_diagnostic(
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

    if (token.type == TokenType{'('}) {
        auto call = New<CallOperator>();
        call->callable = left;
        std::vector<Expression *> temp;
        temp.reserve(8);
        bool first_argument = true;
        while (!(next_token_is(TokenType{')'}) ||
                 next_token_is(TokenType::invalid))) {
            if (!first_argument) {
                expect_token(TokenType{','});
            } else {
                first_argument = false;
            }
            auto argument = parse_expression();
            temp.push_back(argument);
        }
        expect_token(TokenType{')'});
        call->arguments = arena_->push_array(std::span(temp));
        return call;
    }

    auto binary_operator = New<BinaryOperator>();
    binary_operator->op = token.type;
    binary_operator->left = left;
    binary_operator->right =
        parse_expression(TokenTypeToPrecedense(token.type));
    return binary_operator;
}

Expression *Parser::parse_expression(Precedence precedence) {
    auto *left = parse_unary_expression();

    while (precedence < TokenTypeToPrecedense(lexer_.next_token().type)) {
        left = parse_binary_expression(left);
    }

    return left;
}

const Token &Parser::expect_token(TokenType type) {
    const auto &token = lexer_.next_token();
    lexer_.eat_token();
    if (token.type != type) {
        log_diagnostic(token, "Expected {}, got {}.", type, token.type);
        error_count_ += 1;
    }
    return token;
}

bool Parser::next_token_is(TokenType type) {
    return lexer_.next_token().type == type;
}

std::string Ast::node_to_string(const Node *node, int tabs) {
    auto Indent = [](int tabs) {
        constexpr auto tab_width = 4;
        return std::string(tab_width * tabs, ' ');
    };

    std::string result;
    switch (node->type) {
        using enum Ast::NodeType;

        case declaration_procedure: {
            auto proc = reinterpret_cast<const ProcedureDeclaration *>(node);
            result = std::format("proc {}(", proc->identifier.identifier);
            for (int i = 0; i < proc->parameters.size(); ++i) {
                auto parameter = proc->parameters[i];
                result +=
                    std::format("{}: {}", parameter->identifier.identifier,
                                node_to_string(parameter->type, tabs));
                if (i != proc->parameters.size() - 1) {
                    result += ", ";
                }
            }
            result += std::format(") -> {} {}",
                                  node_to_string(proc->return_type, tabs),
                                  node_to_string(proc->body, tabs));
            break;
        }

        case declaration_type: {
            auto decl = reinterpret_cast<const TypeDeclaration *>(node);
            result = std::format("type {} = {};", decl->identifier.identifier,
                                 node_to_string(decl->declared_type, tabs));
            break;
        }

        case type_identifier: {
            auto type_ident = reinterpret_cast<const TypeIdentifier *>(node);
            result = type_ident->identifier.identifier;
            break;
        }

        case type_struct: {
            auto st = reinterpret_cast<const TypeStruct *>(node);
            result = "struct {\n";
            for (auto member : st->members) {
                result += std::format("{}{}: {};\n", Indent(tabs + 1),
                                      member->identifier.identifier,
                                      node_to_string(member->type, tabs + 1));
            }
            result += Indent(tabs);
            result += "}";
            break;
        }

        case type_pointer: {
            auto type_pointer = reinterpret_cast<const TypePointer *>(node);
            result = std::format("*{}",
                                 node_to_string(type_pointer->points_to, tabs));
            break;
        }

        case declaration_const: {
            auto decl = reinterpret_cast<const ConstDeclaration *>(node);
            result =
                std::format("const {}: {} = {};", decl->identifier.identifier,
                            node_to_string(decl->variable_type, tabs),
                            node_to_string(decl->value, tabs));
            break;
        }

        case declaration_variable: {
            auto decl = reinterpret_cast<const VariableDeclaration *>(node);
            result = std::format("{}: {}", decl->identifier.identifier,
                                 node_to_string(decl->variable_type, tabs));
            if (decl->value) {
                result += std::format(
                    " = {}", node_to_string(decl->value.value(), tabs));
            }
            result += ";";
            break;
        }
        case statement_return: {
            auto ret = reinterpret_cast<const ReturnStatement *>(node);
            result =
                std::format("return {};", node_to_string(ret->value, tabs));
            break;
        }
        case statement_if: {
            auto if_statement = reinterpret_cast<const IfStatement *>(node);
            result = std::format(
                "if {} {}", node_to_string(if_statement->condition, tabs),
                node_to_string(if_statement->true_branch, tabs));
            if (if_statement->false_branch) {
                result += std::format(
                    " else {}",
                    node_to_string(*(if_statement->false_branch), tabs));
            }
            break;
        }
        case statement_while: {
            auto while_statement =
                reinterpret_cast<const WhileStatement *>(node);
            result = std::format(
                "while {} {}", node_to_string(while_statement->condition, tabs),
                node_to_string(while_statement->body, tabs));
            break;
        }
        case statement_assingment: {
            auto assingment =
                reinterpret_cast<const AssignmentStatement *>(node);
            result = std::format("{} = {};", assingment->identifier.identifier,
                                 node_to_string(assingment->value, tabs));
            break;
        }
        case statement_block: {
            auto block = reinterpret_cast<const BlockStatement *>(node);
            result = "{\n";
            for (auto statement : block->body) {
                result += std::format("{}{}\n", Indent(tabs + 1),
                                      node_to_string(statement, tabs + 1));
            }
            result += Indent(tabs);
            result += "}";
            break;
        }
        case statement_expression: {
            auto statement =
                reinterpret_cast<const ExpressionStatement *>(node);
            result =
                std::format("{};", node_to_string(statement->expression, tabs));
            break;
        }

        case expression_integer_literal: {
            auto integer = reinterpret_cast<const IntegerLiteral *>(node);
            result = std::format("{}", integer->value.integer_value);
            break;
        }
        case expression_bool_literal: {
            auto boolean = reinterpret_cast<const BoolLiteral *>(node);
            result = std::format("{}", boolean->value);
            break;
        };
        case expression_identifier: {
            auto ident = reinterpret_cast<const IdentifierExpression *>(node);
            result = ident->identifier.identifier;
            break;
        };
        case expression_unary_operator: {
            auto unary_operator = reinterpret_cast<const UnaryOperator *>(node);
            result = std::format("({}{})", unary_operator->op,
                                 node_to_string(unary_operator->right, tabs));
            break;
        };
        case expression_binary_operator: {
            auto binary_operator =
                reinterpret_cast<const BinaryOperator *>(node);
            result = std::format("({} {} {})",
                                 node_to_string(binary_operator->left, tabs),
                                 binary_operator->op,
                                 node_to_string(binary_operator->right, tabs));
            break;
        };
        case expression_call_operator: {
            auto call = reinterpret_cast<const CallOperator *>(node);
            Assert(call->callable->type == expression_identifier);
            auto callable =
                reinterpret_cast<const IdentifierExpression *>(call->callable);
            result = std::format("{}(", callable->identifier.identifier);
            for (int i = 0; i < call->arguments.size(); ++i) {
                auto arg = call->arguments[i];
                result += node_to_string(arg, tabs);
                if (i != call->arguments.size() - 1) {
                    result += ", ";
                }
            }
            result += ')';
            break;
        }
        case invalid: {
            result = "invalid;";
            break;
        }

        default: {
            result = "unknown;";
            break;
        }
    }

    return result;
}