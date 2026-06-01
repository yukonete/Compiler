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

#include "base/arena.h"
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

class Parser {
public:
    constexpr Parser(std::string_view input, Allocator allocator, FILE *log = stderr)
        : lexer_{input}, arena_{allocator}, log_{log} {}

    Program parse_program();

private:
    Statement *parse_statement();

    Declaration *parse_declaration();
    ConstDeclaration *parse_constant_declaration();
    VariableDeclaration *parse_variable_declaration();
    TypeDeclaration *parse_type_declaration();
    ProcedureDeclaration *parse_procedure_declaration();

    IfStatement *parse_if_statement();
    WhileStatement *parse_while_statement();
    BlockStatement *parse_block_statement();
    ReturnStatement *parse_return_statement();
    std::span<Statement*> parse_statements_sequence();

    Expression *parse_expression(Precedence precedence = Precedence::lowest);
    Expression *parse_unary_expression();
    Expression *parse_binary_expression(Expression *left);

    Type *parse_type();
    TypeProcedure *parse_procedure_type(bool skip_identifier = false);
    Identifier *parse_identifier();

    template <typename NodeType, typename... Args>
    NodeType *New(Args &&...args) 
    requires std::derived_from<NodeType, Node>
    {
        return arena_.create<NodeType>(std::forward<Args>(args)...);
    };

    template<typename NodeType>
    std::span<NodeType*> NewArray(std::span<NodeType*> nodes)
    requires std::derived_from<NodeType, Node>
    {
        auto array = arena_.allocate<NodeType*>(nodes.size());
        for (usize i = 0; i < nodes.size(); ++i) {
            array[i] = nodes[i];
        }
        return std::span{array, nodes.size()};
    }

    const Token &expect_token(TokenType type);

    bool next_token_is(TokenType type);

    template <typename... Args>
    void report_error(const FileLocation &location, std::format_string<Args...> fmt,
                      Args &&...args) {
        error_count_ += 1;
        log_diagnostics(log_, DiagnosticsLevel::Error, location, fmt, std::forward<Args>(args)...);
    }

    Lexer lexer_;
    DynamicArena arena_;

    FILE *log_ = nullptr;
    std::vector<Statement*> statements_;

    u64 error_count_ = 0;
};

}; // namespace Ast