#pragma once

#include <cstdio>
#include <span>
#include <string>
#include <vector>
#include <utility>

#include "base/maybe.h"
#include "base/util.h"
#include "base/arena.h"
#include "base/allocator.h"
#include "base/concepts.h"
#include "token.h"
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
        : lexer{std::move(lexer)}, nodes_storage_{allocator} {}

    static Maybe<Parser> open(std::string &&path, Allocator allocator, FILE *log = stderr) {
        auto lexer = Lexer::open(std::move(path), log);
        if (!lexer) {
            return {};
        }
        return Parser{std::move(*lexer), allocator};
    }

    bool parse_program();

    template <typename NodeType, typename... Args>
    NodeType *New(Args &&...args) 
        requires TriviallyDestructible<NodeType>
    {
        return nodes_storage_.new_object<NodeType>(std::forward<Args>(args)...);
    };

    template<typename NodeType>
    std::span<NodeType*> NewArray(std::span<NodeType*> nodes) {
        auto array = nodes_storage_.allocate<NodeType*>(nodes.size());
        for (auto i : indices(nodes.size())) {
            array[i] = nodes[i];
        }
        return std::span{array, nodes.size()};
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

    Expression *parse_expression(Precedence precedence = Precedence::lowest, bool lhs = false);
    Expression *parse_unary_expression(bool lhs);
    Expression *parse_binary_expression(Expression *left, bool lhs);
    CompoundExpression *parse_compound_expression(Maybe<Type *> type = {});

    Type *parse_type(bool named = false);
    ProcedureType *parse_procedure_type(bool skip_identifier = false);
    
    Identifier *parse_identifier();
    Field *parse_field();

    Token expect_token(TokenType type);

    bool peek_token_is(TokenType type, int peek);
    bool next_token_is(TokenType type);
public:
    Lexer lexer;
    std::vector<Statement *> ast;
private:
    DynamicArena nodes_storage_;
};

}; // namespace Ast