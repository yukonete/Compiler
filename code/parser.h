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
    constexpr Parser(Lexer &&lexer, Allocator allocator, FILE *log = stderr)
        : lexer_{std::move(lexer)}, nodes_storage_{allocator}, log_{log} {}

    static std::optional<Parser> open(std::string_view path,
                                      Allocator allocator, FILE *log = stderr) {
        return Lexer::open(path, log).transform([&](Lexer &&lexer) {
            return Parser{std::move(lexer), allocator};
        });
    }

    bool parse_program();

    std::vector<Statement*> &ast() {
        return statements_;
    }

    const std::vector<Statement*> &ast() const {
        return statements_;
    }

    bool any_errors() const {
        return error_count_ != 0;
    }

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

    Expression *parse_expression(Precedence precedence = Precedence::lowest);
    Expression *parse_unary_expression();
    Expression *parse_binary_expression(Expression *left);

    Type *parse_type();
    TypeProcedure *parse_procedure_type(bool skip_identifier = false);
    
    Identifier *parse_identifier();
    Field *parse_field();

    template <typename NodeType, typename... Args>
    NodeType *New(Args &&...args) {
        return nodes_storage_.create<NodeType>(std::forward<Args>(args)...);
    };

    template<typename NodeType>
    std::span<NodeType*> NewArray(std::span<NodeType*> nodes) {
        auto array = nodes_storage_.allocate<NodeType*>(nodes.size());
        for (usize i = 0; i < nodes.size(); ++i) {
            array[i] = nodes[i];
        }
        return std::span{array, nodes.size()};
    }

    const Token &expect_token(TokenType type);

    bool next_token_is(TokenType type);

    template <typename... Args>
    void syntax_error(Node auto *node, std::format_string<Args...> fmt,
                      Args &&...args) {
        if (!any_errors()) {
            auto start_pos = node->start_token().start;
            auto end_pos = node->end_token().end;
            lexer_.log_diagnostics(DiagnosticsLevel::Error, start_pos, end_pos,
                                   fmt, std::forward<Args>(args)...);
        }
        error_count_ += 1;
    }

    template <typename... Args>
    void syntax_error(const Token &token, std::format_string<Args...> fmt,
                      Args &&...args) {
        if (!any_errors()) {
            lexer_.log_diagnostics(DiagnosticsLevel::Error, token.start,
                                   token.end, fmt, std::forward<Args>(args)...);
        }
        error_count_ += 1;
    }

    Lexer lexer_;
    DynamicArena nodes_storage_;
    std::vector<Statement *> statements_;
    u64 error_count_ = 0;

    FILE *log_;
};

}; // namespace Ast