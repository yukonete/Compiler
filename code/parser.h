#pragma once

#include <cstdio>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <utility>

#include "base/arena.h"
#include "base/allocator.h"
#include "base/concepts.h"
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
    constexpr Parser(Lexer &&lexer, Allocator allocator)
        : lexer_{std::move(lexer)}, nodes_storage_{allocator} {}

    static std::optional<Parser> open(std::string &&path,
                                      Allocator allocator, FILE *log = stderr) {
        return Lexer::open(std::move(path), log).transform([&](Lexer &&lexer) {
            return Parser{std::move(lexer), allocator};
        });
    }

    bool parse_program();

    auto&& lexer(this auto&& self) {
        return std::forward_like<decltype(self)>(self.lexer_);
    }

    auto&& ast(this auto&& self) {
        return std::forward_like<decltype(self)>(self.ast_);
    }

private:
    constexpr static bool REPORT_ALL_ERRORS = false;

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
    NodeType *New(Args &&...args) 
        requires TriviallyDestructible<NodeType>
    {
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
        if (!lexer_.any_errors() || REPORT_ALL_ERRORS) {
            auto start_pos = node->start_token().start;
            auto end_pos = node->end_token().end;
            lexer_.log_diagnostics(DiagnosticsLevel::Error, start_pos, end_pos,
                                   fmt, std::forward<Args>(args)...);
        }
    }

    template <typename... Args>
    void syntax_error(const Token &token, std::format_string<Args...> fmt,
                      Args &&...args) {
        if (!lexer_.any_errors() || REPORT_ALL_ERRORS) {
            lexer_.log_diagnostics(DiagnosticsLevel::Error, token.start,
                                      token.end, fmt,
                                      std::forward<Args>(args)...);
        }
    }

    Lexer lexer_;
    std::vector<Statement *> ast_;
    DynamicArena nodes_storage_;
};

}; // namespace Ast