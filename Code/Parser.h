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

namespace Ast {

enum class NodeType {
    invalid,

    // Always leave this as first declaration kind because it used in comparison
    // to determine if node is a declaration
    declaration_variable,
    declaration_const,
    declaration_procedure,
    // Always leave this as last declaration kind because it used in comparison
    // to determine if node is a declaration
    declaration_type,

    // Always leave this as first statement kind because it used in comparison
    // to determine if node is a statement
    statement_if,
    statement_while,
    statement_assingment,
    statement_block,
    statement_return,
    // Always leave this as last statement kind because it used in comparison to
    // determine if node is a statement
    statement_expression,

    expression_integer_literal,
    expression_bool_literal,
    expression_identifier,
    expression_unary_operator,
    expression_binary_operator,
    expression_call_operator,

    expression_cast,

    type_identifier,
    type_pointer,
    type_struct,
    type_array
};

struct Program;

struct Node;

using Identifier = Token;

struct Declaration;
struct VariableDeclaration;
struct ProcedureDeclaration;
struct ConstDeclaration;
struct TypeDeclaration;
struct ProcedureDeclaration;

struct Statement;
struct IfStatement;
struct WhileStatement;
struct AssignmentStatement;
struct BlockStatement;
struct ReturnStatement;
struct ExpressionStatement;

struct Expression;
struct IntegerLiteral;
struct UnaryOperator;
struct BinaryOperator;
struct BoolLiteral;
struct IdentifierExpression;

struct Type;
struct TypeIdentifier;
struct TypePointer;
struct TypeStruct;

struct Node {
    NodeType type;
    Node(NodeType type_) : type{type_} {
    }
};

struct Statement : public Node {
    Statement(NodeType type_) : Node(type_) {
    }
};

struct Expression : public Node {
    Expression(NodeType type_) : Node(type_) {
    }
};

struct Declaration : public Statement {
    Declaration(NodeType type_) : Statement(type_) {
    }
};

struct Type : public Node {
    Type(NodeType type_) : Node(type_) {
    }
};

struct TypeIdentifier : public Type {
    TypeIdentifier() : Type(NodeType::type_identifier) {
    }
    Identifier identifier;
};

struct TypePointer : public Type {
    TypePointer() : Type(NodeType::type_pointer) {
    }
    Type *points_to = nullptr;
};

struct TypeArray : public Type {
    TypeArray() : Type(NodeType::type_array) {
    }
    Type *elem_type = nullptr;
    s64 count = 0;
};

struct StructMember {
    Identifier identifier;
    Type *type = nullptr;
};

struct TypeStruct : public Type {
    TypeStruct() : Type(NodeType::type_struct) {
    }
    std::span<StructMember *> members;
};

struct VariableDeclaration : public Declaration {
    VariableDeclaration() : Declaration(NodeType::declaration_variable) {
    }
    Identifier identifier;
    Type *variable_type = nullptr;
    std::optional<Expression *> value;
};

struct ConstDeclaration : public Declaration {
    ConstDeclaration() : Declaration(NodeType::declaration_const) {
    }
    Identifier identifier;
    Type *variable_type = nullptr;
    Expression *value = nullptr;
};

struct ProcedureParameter {
    Identifier identifier;
    Type *type = nullptr;
};

struct ProcedureDeclaration : public Declaration {
    ProcedureDeclaration() : Declaration(NodeType::declaration_procedure) {
    }
    Identifier identifier;
    std::span<ProcedureParameter *> parameters;
    Type *return_type = nullptr;
    BlockStatement *body = nullptr;
};

struct TypeDeclaration : public Declaration {
    TypeDeclaration() : Declaration(NodeType::declaration_type) {
    }
    Identifier identifier;
    Type *declared_type = nullptr;
};

struct IfStatement : public Statement {
    IfStatement() : Statement(NodeType::statement_if) {
    }
    Expression *condition = nullptr;
    Statement *true_branch = nullptr;

    std::optional<Statement *> false_branch = nullptr;
};

struct WhileStatement : public Statement {
    WhileStatement() : Statement(NodeType::statement_while) {
    }
    Expression *condition = nullptr;
    Statement *body = nullptr;
};

struct BlockStatement : public Statement {
    BlockStatement() : Statement(NodeType::statement_block) {
    }
    std::span<Statement *> body;
};

struct ReturnStatement : public Statement {
    ReturnStatement() : Statement(NodeType::statement_return) {
    }
    Expression *value = nullptr;
};

struct AssignmentStatement : public Statement {
    AssignmentStatement() : Statement(NodeType::statement_assingment) {
    }
    Identifier identifier;
    Expression *value = nullptr;
};

struct ExpressionStatement : public Statement {
    ExpressionStatement() : Statement(NodeType::statement_expression) {
    }
    Expression *expression = nullptr;
};

struct IntegerLiteral : public Expression {
    IntegerLiteral() : Expression(NodeType::expression_integer_literal) {
    }
    Token value;
};

struct IdentifierExpression : public Expression {
    IdentifierExpression() : Expression(NodeType::expression_identifier) {
    }
    Identifier identifier;
};

struct UnaryOperator : public Expression {
    UnaryOperator() : Expression(NodeType::expression_unary_operator) {
    }
    TokenType op = TokenType::invalid;
    Expression *right = nullptr;
};

struct BinaryOperator : public Expression {
    BinaryOperator() : Expression(NodeType::expression_binary_operator) {
    }
    TokenType op = TokenType::invalid;
    Expression *left = nullptr;
    Expression *right = nullptr;
};

struct BoolLiteral : public Expression {
    BoolLiteral() : Expression(NodeType::expression_bool_literal) {
    }
    bool value = false;
};

struct CallOperator : public Expression {
    CallOperator() : Expression(NodeType::expression_call_operator) {
    }
    Expression *callable = nullptr;
    std::span<Expression *> arguments;
};

struct Program {
    std::vector<Declaration *> declarations;
    int error_count = 0;
};

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
    Parser(std::string_view input, Arena *arena, FILE *log = stderr);

    // If error_count is not 0 then indentifiers might have incorrect values in
    // union
    Program parse_program();

private:
    Statement *parse_statement();

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

    template <typename NodeType, std::invocable<std::vector<NodeType *> *> Func>
    std::span<NodeType *> parse_until_token(TokenType token, Func parse_func);

    Type *parse_type();

    template <typename NodeType> NodeType *New() {
        return arena_->push_item<NodeType>();
    };

    template <typename NodeType> NodeType *New(NodeType type) {
        return arena_->push_item<NodeType>(type);
    };

    const Token &expect_token(TokenType type);

    bool next_token_is(TokenType type);

    template <typename... Args>
    void report_error(const Token &token, std::format_string<Args...> fmt,
                        Args &&...args) {
        error_count_ += 1;
        std::print(log_, "({}, {}): ", token.start.line, token.start.column);
        std::println(log_, fmt, std::forward<Args>(args)...);
    }

    Lexer lexer_;
    Arena *arena_ = nullptr;
    FILE *log_ = nullptr;

    Token expected_token_;
    int error_count_ = 0;
};

}; // namespace Ast