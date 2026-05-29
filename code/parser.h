#pragma once

#include <cstdio>
#include <format>
#include <functional>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "base.h"
#include "lexer.h"
#include "log.h"
#include "ast.h"

namespace Ast {

enum class Precedence {
    lowest,
    equals,     // ==, !=
    comparison, // <, <=, >, >=
    plus,       // +, -
    multiply,   // *, /, %
    prefix,     // -, !
    call,
};

std::string node_to_string(const Node *node, int tabs);

class Parser {
public:
    Parser(std::string_view input, ArenaAllocator *arena, FILE *log = stderr);

    // If error_count is not 0 then indentifiers might have incorrect values in
    // union
    Program parse_program();

private:
    Statement *parse_statement();

    Declaration *parse_declaration();
    DeclarationStatement *parse_declaration_statement(); 
    ConstDeclaration *parse_constant_declaration();
    VariableDeclaration *parse_variable_declaration();
    TypeDeclaration *parse_type_declaration();
    ProcedureDeclaration *parse_procedure_declaration();

    ExpressionStatement *parse_expression_statement();
    IfStatement *parse_if_statement();
    WhileStatement *parse_while_statement();
    AssignmentStatement *parse_assignment_statement();
    BlockStatement *parse_block_statement();
    ReturnStatement *parse_return_statement();

    Expression *parse_expression(Precedence precedence = Precedence::lowest);
    Expression *parse_unary_expression();
    Expression *parse_binary_expression(Expression *left);

    Type *parse_type();
    Identifier *parse_identifier();

    template <typename NodeType, std::invocable<std::vector<NodeType *> *> Func>
    std::span<NodeType *> parse_until_token(TokenType token, Func parse_func) {
        std::vector<NodeType *> temp;
        temp.reserve(16);
        while (!(next_token_is(token) || next_token_is(TokenType::invalid) ||
                 next_token_is(TokenType::eof))) {
            parse_func(&temp);
        }
        expect_token(token);
        return arena_->push_array(std::span(temp));
    }

    template <typename NodeType, typename... Args>
    NodeType *New(Args &&...args) {
        return arena_->push_item<NodeType>(std::forward<Args>(args)...);
    };
    
    const Token &expect_token(TokenType type);

    bool next_token_is(TokenType type);

    template <typename... Args>
    void report_error(const Token &token, std::format_string<Args...> fmt,
                      Args &&...args) {
        error_count_ += 1;
        log_diagnostics(log_, DiagnosticsLevel::Error, token, fmt,
                        std::forward<Args>(args)...);
    }

    Lexer lexer_;
    ArenaAllocator *arena_ = nullptr;
    FILE *log_ = nullptr;

    Token expected_token_;
    int error_count_ = 0;
};

}; // namespace Ast