#ifndef AST_H
#define AST_H

#include <cassert>
#include <optional>
#include <span>
#include <concepts>
#include <string>

#include "base/flags.h"
#include "base/down_cast.h"
#include "base/types.h"
#include "lexer.h"

namespace Ast {

struct Program;

struct Statement;
struct IfStatement;
struct WhileStatement;
struct AssignmentStatement;
struct BlockStatement;
struct ReturnStatement;
struct ExpressionStatement;
struct DeclarationStatement;
struct ContinueStatement;
struct BreakStatement;

struct Declaration;
struct VariableDeclaration;
struct ProcedureDeclaration;
struct ConstDeclaration;
struct TypeDeclaration;

struct Type;
struct IdentifierType;
struct PointerType;
struct StructType;
struct ProcedureType;
struct ArrayType;

struct Expression;
struct IntegerLiteralExpression;
struct UnaryOperatorExpression;
struct BinaryOperatorExpression;
struct BoolLiteralExpression;
struct IdentifierExpression;
struct CallOperatorExpression;
struct FloatLiteralExpression;
struct StringLiteralExpression;

#define DEFINE_AST_NODE_DOWNCAST_FUNCTIONS_FOR(type) DEFINE_DOWNCAST_FUNCTIONS_FOR(type, kind, KIND)

// AST conventions:
// 1) Pointers in AST are not allowed to be nullptr.
// 2) If a field of a node is optional, it is represented as std::optional<Node*>
// field.
// 3) Nodes can not have non-trival destructors.
// 4) Even though AST has BAD nodes, later stages of compiler should
// assume those do not exist because they are created only when
// there is an error in a source code.

struct Identifier {
    constexpr Identifier(const Token &token) : token{token} {
    }

    Token token;
};

struct Statement {
    enum class Kind : u8 {
        BAD,
        EMPTY,
        IF,
        WHILE,
        ASSIGNMENT,
        BLOCK,
        RETURN,
        DECLARATION,
        CONTINUE,
        BREAK,
        EXPRESSION,
    };

    DEFINE_AST_NODE_DOWNCAST_FUNCTIONS_FOR(Statement);

    constexpr Statement(Kind kind) : kind{kind} {
    }
    Token start_token() const;
    Token end_token() const;

    Kind kind;
};

struct BadStatement : public Statement {
    static constexpr auto KIND = Kind::BAD;

    constexpr BadStatement(const Token &token) 
        : Statement{KIND}, token{token} {
    }
     
    Token token;
};

struct EmptyStatement : public Statement {
    static constexpr auto KIND = Kind::EMPTY;

    constexpr EmptyStatement(const Token &token) 
        : Statement{KIND}, token{token} {
    }

    Token token;
};

struct IfStatement : public Statement {
    static constexpr auto KIND = Kind::IF;

    struct ElseBranch {
        constexpr ElseBranch(const Token &token, Statement *body)
            : token{token}, body{body} {
        }
        Token token;
        Statement *body;
    };

    constexpr IfStatement(const Token &token, Expression *condition,
                          Statement *body,
                          const std::optional<ElseBranch> &else_branch)
        : Statement{KIND}, token{token}, condition{condition}, body{body},
          else_branch{else_branch} {
    }

    Token token;
    Expression *condition;
    Statement *body;
    std::optional<ElseBranch> else_branch;
};

struct WhileStatement : public Statement {
    static constexpr auto KIND = Kind::WHILE;

    constexpr WhileStatement(const Token &token, Expression *condition,
                             Statement *body)
        : Statement{KIND}, token{token}, condition{condition}, body{body} {
    }

    Token token;
    Expression *condition;
    Statement *body;
};

struct BlockStatement : public Statement {
    static constexpr auto KIND = Kind::BLOCK;

    constexpr BlockStatement(const Token &open, std::span<Statement *> body,
                             const Token &close)
        : Statement{KIND}, open{open}, close{close}, body{body} {
    }

    Token open;
    Token close;
    std::span<Statement *> body;
};

struct ReturnStatement : public Statement {
    static constexpr auto KIND = Kind::RETURN;

    constexpr ReturnStatement(const Token &token,
                              std::optional<Expression *> value)
        : Statement{KIND}, token{token}, value{value} {
    }

    Token token;
    std::optional<Expression *> value;
};

struct AssignmentStatement : public Statement {
    static constexpr auto KIND = Kind::ASSIGNMENT;

    constexpr AssignmentStatement(Expression *expression, const Token &assign,
                                  Expression *value)
        : Statement{KIND}, assign{assign}, expression{expression}, value{value} {
    }

    Token assign;
    Expression *expression;
    Expression *value;
};

struct ExpressionStatement : public Statement {
    static constexpr auto KIND = Kind::EXPRESSION;

    constexpr ExpressionStatement(Expression *expression)
        : Statement{KIND}, expression{expression} {
    }

    Expression *expression;
};

struct DeclarationStatement : public Statement {
    static constexpr auto KIND = Kind::DECLARATION;

    constexpr DeclarationStatement(Declaration *declaration)
        : Statement{KIND}, declaration{declaration} {
    }

    Declaration *declaration;
};

struct BreakStatement : public Statement {
    static constexpr auto KIND = Kind::BREAK;

    constexpr BreakStatement(const Token &token)
        : Statement{KIND}, token{token} {
    }

    Token token;
};

struct ContinueStatement : public Statement {
    static constexpr auto KIND = Kind::CONTINUE;

    constexpr ContinueStatement(const Token &token)
        : Statement{KIND}, token{token} {
    }

    Token token;
};

struct Declaration {
    enum class Kind : u8 {
        VARIABLE,
        FUNCTION,
        CONSTANT,
        TYPE,
        FIELD,
    };

    enum class Flags : u8 {
        NONE,
        HANDLED,
    };

    DEFINE_AST_NODE_DOWNCAST_FUNCTIONS_FOR(Declaration);

    constexpr Declaration(Kind kind, Identifier *identifier)
        : kind{kind}, identifier{identifier} {
    }
    Token start_token() const;
    Token end_token() const;

    Kind kind;
    Flags flags = Flags::NONE;
    Identifier *identifier;
};

DEFINE_ENUM_FLAG_OPERATORS(Declaration::Flags);

struct Field : public Declaration {
    static constexpr auto KIND = Kind::FIELD;

    constexpr Field(Identifier *identifier, Type *type)
        : Declaration{KIND, identifier}, type{type} {
    }

    Type *type;
};

struct VariableDeclaration : public Declaration {
    static constexpr auto KIND = Kind::VARIABLE;

    constexpr VariableDeclaration(const Token &token, Identifier *identifier,
                                  std::optional<Type *> type,
                                  std::optional<Expression *> value)
        : Declaration{KIND, identifier}, token{token}, type{type},
          value{value} {
    }

    Token token;
    std::optional<Type *> type;
    std::optional<Expression *> value;
};

struct ConstDeclaration : public Declaration {
    static constexpr auto KIND = Kind::CONSTANT;

    constexpr ConstDeclaration(const Token &token, Identifier *identifier,
                               std::optional<Type *> type, Expression *value)
        : Declaration{KIND, identifier}, token{token}, type{type},
          value{value} {
    }

    Token token;
    std::optional<Type *> type;
    Expression *value;
};

struct ProcedureDeclaration : public Declaration {
    static constexpr auto KIND = Kind::FUNCTION;

    constexpr ProcedureDeclaration(Identifier *identifier, ProcedureType *type,
                                   BlockStatement *body)
        : Declaration{KIND, identifier}, type{type}, body{body} {
    }

    ProcedureType *type;
    BlockStatement *body;
};

struct TypeDeclaration : public Declaration {
    static constexpr auto KIND = Kind::TYPE;

    constexpr TypeDeclaration(const Token &token, Identifier *identifier,
                              Type *type)
        : Declaration{KIND, identifier}, token{token}, type{type} {
    }

    Token token;
    Type *type;
};

struct Expression {
    enum class Kind : u8 {
        BAD,
        INTEGER_LITERAL,
        UNARY_OPERATOR,
        BINARY_OPERATOR,
        BOOL_LITERAL,
        IDENTIFIER,
        CALL_OPERATOR,
        STRING_LITERAL,
        FLOAT_LITERAL,
        SELECTOR,
        INDEX,
    };

    DEFINE_AST_NODE_DOWNCAST_FUNCTIONS_FOR(Expression);

    constexpr Expression(Kind kind) : kind{kind} {
    }
    Token start_token() const;
    Token end_token() const;

    Kind kind;
};

struct BadExpression : public Expression {
    static constexpr auto KIND = Kind::BAD;

    constexpr BadExpression(const Token &token)
        : Expression{KIND}, token{token} {
    }

    Token token;
};

struct IntegerLiteralExpression : public Expression {
    static constexpr auto KIND = Kind::INTEGER_LITERAL;

    constexpr IntegerLiteralExpression(const Token &token, s64 value)
        : Expression{KIND}, token{token}, value{value} {
    }

    Token token;
    s64 value;
};

struct IdentifierExpression : public Expression {
    static constexpr auto KIND = Kind::IDENTIFIER;

    constexpr IdentifierExpression(Identifier *identifier)
        : Expression{KIND}, identifier{identifier} {
    }

    Identifier *identifier;
};

struct UnaryOperatorExpression : public Expression {
    static constexpr auto KIND = Kind::UNARY_OPERATOR;

    constexpr UnaryOperatorExpression(const Token &op, Expression *right)
        : Expression{KIND}, op{op}, right{right} {
    }

    Token op;
    Expression *right;
};

struct BinaryOperatorExpression : public Expression {
    static constexpr auto KIND = Kind::BINARY_OPERATOR;

    constexpr BinaryOperatorExpression(Expression *left, const Token &op,
                                       Expression *right)
        : Expression{KIND}, op{op}, left{left}, right{right} {
    }

    Token op;
    Expression *left = nullptr;
    Expression *right = nullptr;
};

struct BoolLiteralExpression : public Expression {
    static constexpr auto KIND = Kind::BOOL_LITERAL;

    BoolLiteralExpression(const Token &token, bool value)
        : Expression{KIND}, token{token}, value{value} {
    }

    Token token;
    bool value;
};

struct CallOperatorExpression : public Expression {
    static constexpr auto KIND = Kind::CALL_OPERATOR;

    constexpr CallOperatorExpression(Expression *expression, const Token &open,
                                     std::span<Expression *> arguments,
                                     const Token &close)
        : Expression{KIND}, open{open}, close{close}, expression{expression},
          arguments{arguments} {
    }

    Token open;
    Token close;
    Expression *expression;
    std::span<Expression *> arguments;
};

struct SelectorExpression : public Expression {
    static constexpr auto KIND = Kind::SELECTOR;

    constexpr SelectorExpression(Expression *expression, const Token &dot,
                                 Identifier *identifier)
        : Expression{KIND}, expression{expression}, dot{dot},
          identifier{identifier} {
    }

    Expression *expression;
    Token dot;
    Identifier *identifier;
};

struct StringLiteralExpression : public Expression {
    static constexpr auto KIND = Kind::STRING_LITERAL;

    constexpr StringLiteralExpression(const Token &token)
        : Expression{KIND}, token{token} {
    }

    Token token;
};

struct FloatLiteralExpression : public Expression {
    static constexpr auto KIND = Kind::FLOAT_LITERAL;

    constexpr FloatLiteralExpression(const Token &token, f64 value)
        : Expression{KIND}, token{token}, value{value} {
    }

    Token token;
    f64 value;
};

struct IndexExpression : public Expression {
    static constexpr auto KIND = Kind::INDEX;

    constexpr IndexExpression(Expression *expression, const Token &open,
                              Expression *index, const Token &close)
        : Expression{KIND}, open{open}, close{close}, expression{expression}, index{index} {
    }

    Token open;
    Token close;
    Expression *expression;
    Expression *index;
};

struct Type {
    enum class Kind : u8 {
        BAD,
        IDENTIFIER,
        STRUCT,
        POINTER,
        FUNCTION,
        ARRAY,
    };

    DEFINE_AST_NODE_DOWNCAST_FUNCTIONS_FOR(Type);

    constexpr Type(Kind kind) : kind{kind} {
    }
    Token start_token() const;
    Token end_token() const;

    Kind kind;
};

using TypePath = std::span<Identifier *>;

inline std::string type_path_to_string(TypePath path) {
    auto result = std::string{};
    for (usize i = 0; i < path.size(); ++i) {
        result += path[i]->token.value;

        if (i != path.size() - 1) {
            result += '.';
        }
    }
    return result;
}

struct BadType : public Type {
    static constexpr auto KIND = Kind::BAD;

    constexpr BadType(const Token &token)
        : Type{KIND}, token{token} {
    }

    Token token;
};

struct IdentifierType : public Type {
    static constexpr auto KIND = Kind::IDENTIFIER;

    constexpr IdentifierType(TypePath path) : Type{KIND}, path{path} {
    }

    // temporary
    std::string get_full_type_name() const {
        return type_path_to_string(path);
    }

    // Path should always have at least one identifier
    TypePath path;
};

struct PointerType : public Type {
    static constexpr auto KIND = Kind::POINTER;

    constexpr PointerType(const Token &token, Type *type)
        : Type{KIND}, token{token}, type{type} {
    }

    Token token;
    Type *type;
};

struct ArrayType : public Type {
    static constexpr auto KIND = Kind::ARRAY;

    constexpr ArrayType(const Token &open, Expression *count,
                        const Token &close, Type *element_type)
        : Type{KIND}, open{open}, close{close}, element_type{element_type}, 
        count{count} {
    }

    Token open;
    Token close;

    Type *element_type;
    Expression *count;
};

struct ProcedureType : public Type {
    static constexpr auto KIND = Kind::FUNCTION;

    constexpr ProcedureType(const Token &token, const Token &open,
                            std::span<Field *> parameters, const Token &close,
                            std::optional<Type *> return_type)
        : Type{KIND}, token{token}, open{open}, close{close},
          parameters{parameters}, return_type{return_type} {
    }

    Token token;
    Token open;
    Token close;

    std::span<Field *> parameters;
    std::optional<Type *> return_type;
};

struct StructType : public Type {
    static constexpr auto KIND = Kind::STRUCT;

    constexpr StructType(const Token &token, const Token &open,
                         std::span<Field *> members,
                         std::span<DeclarationStatement *> declarations,
                         const Token &close)
        : Type{KIND}, token{token}, open{open}, close{close}, members{members},
          declarations{declarations} {
    }

    Token token;
    Token open;
    Token close;

    std::span<Field *> members;
    std::span<DeclarationStatement *> declarations;
};

template <typename T>
concept Node = std::derived_from<T, Statement> || std::derived_from<T, Declaration> ||
               std::derived_from<T, Type> || std::derived_from<T, Expression>;

std::string statement_to_string(const Statement *type, u64 tabs, bool block_indent = true);
std::string expression_to_string(const Expression *type, u64 tabs);
std::string type_to_string(const Type *type, u64 tabs = 0,
                           bool include_fn = true);
std::string declaration_to_string(const Declaration *decl, u64 tabs);

}; // namespace Ast
#endif // #ifndef AST_H