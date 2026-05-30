#ifndef AST_H
#define AST_H

#include <optional>
#include <span>
#include <variant>
#include <string_view>

#include "base.h"
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
struct TypeIdentifier;
struct TypePointer;
struct TypeStruct;
struct TypeProcedure;
struct TypeArray;

struct Expression;
struct IntegerLiteralExpression;
struct UnaryOperatorExpression;
struct BinaryOperatorExpression;
struct BoolLiteralExpression;
struct IdentifierExpression;
struct CallOperatorExpression;
struct ArraySubscriptExpression;
struct FloatLiteralExpression;
struct StringLiteralExpression;

template <typename Node, typename Variant>
struct AstVariantBase {
    AstVariantBase() = default;
    AstVariantBase(const AstVariantBase &) = delete;
    AstVariantBase(AstVariantBase &&) = delete;
    AstVariantBase &operator=(const AstVariantBase &) = delete;
    AstVariantBase &operator=(AstVariantBase &&) = delete;
    ~AstVariantBase() = default;

    Variant const &get_variant() {
        return static_cast<Node*>(this)->variant;
    }

    Variant const &get_variant() const {
        return static_cast<Node const*>(this)->variant;
    }

    template <typename T>
    bool is() const {
        return std::holds_alternative<T*>(get_variant());
    }

    template <typename T>
    std::optional<T *> get_if() {
        auto pointer = std::get_if<T*>(&get_variant());
        if (pointer != nullptr) {
            return *pointer;
        }
        return {};
    }

    template <typename T>
    std::optional<T const*> get_if() const {
        auto pointer = std::get_if<T*>(&get_variant());
        if (pointer != nullptr) {
            return *pointer;
        }
        return {};
    }

    template <typename T>
    T *as() {
        return std::get<T*>(get_variant());
    }

    template <typename T>
    T const *as() const {
        return std::get<T*>(get_variant());
    }
};

struct Identifier {
    Token token;

    std::string_view value() const {
        return token.value;
    }
};

#define DEFINE_AST_NODE_DOWNCAST_FUNCTIONS_FOR(type)                           \
    template <typename T>                                                      \
    constexpr bool is() const                                                  \
        requires std::derived_from<T, type>                                    \
    {                                                                          \
        return kind == T::KIND;                                                \
    }                                                                          \
    template <typename T>                                                      \
    const T *as() const                                                        \
        requires std::derived_from<T, type>                                    \
    {                                                                          \
        if (!is<T>()) {                                                        \
            panic("Wrong AST downcast");                                       \
        }                                                                      \
        return static_cast<const T *>(this);                                   \
    }                                                                          \
    template <typename T>                                                      \
    T *as()                                                                    \
        requires std::derived_from<T, type>                                    \
    {                                                                          \
        if (!is<T>()) {                                                        \
            panic("Wrong AST downcast");                                       \
        }                                                                      \
        return static_cast<T *>(this);                                   \
    }

struct Statement {
    enum class Kind : u8 {
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

    constexpr Statement(Kind kind) : kind{kind} {}

    DEFINE_AST_NODE_DOWNCAST_FUNCTIONS_FOR(Statement);

    Kind kind;
};

struct IfStatement : public Statement {
    static constexpr auto KIND = Kind::IF;

    constexpr IfStatement(const Token &if_token) 
        : Statement{KIND}, if_token{if_token} {}

    struct ElseBranch {
        Token else_token;
        std::span<Statement *> body;
    };

    Token if_token;
    Expression *condition = nullptr;
    std::span<Statement *> true_branch_body;
    std::optional<ElseBranch> else_branch;
};

struct WhileStatement : public Statement {
    static constexpr auto KIND = Kind::WHILE;

    constexpr WhileStatement(const Token &while_token) 
        : Statement{KIND}, while_token{while_token} {}

    Token while_token;
    Expression *condition = nullptr;
    std::span<Statement *> body;
};

struct BlockStatement : public Statement {
    static constexpr auto KIND = Kind::BLOCK;
    
    constexpr BlockStatement(const Token &open_brace) 
        : Statement{KIND}, open_brace{open_brace} {}

    Token open_brace;
    Token close_brace;
    std::span<Statement *> body;
};

struct ReturnStatement : public Statement {
    static constexpr auto KIND = Kind::RETURN;

    constexpr ReturnStatement(const Token &return_token) 
        : Statement{KIND}, return_token{return_token} {}

    Token return_token;
    Expression *value = nullptr;
};

struct AssignmentStatement : public Statement {
    static constexpr auto KIND = Kind::ASSIGNMENT;
    
    constexpr AssignmentStatement(Token const &assign) 
        : Statement{KIND}, assign{assign} {}

    Token assign;
    Expression *assignee = nullptr;
    Expression *value = nullptr;
};

struct ExpressionStatement : public Statement {
    static constexpr auto KIND = Kind::EXPRESSION;
    
    constexpr ExpressionStatement() : Statement{KIND} {}

    Expression *expression = nullptr;
};

struct DeclarationStatement : public Statement {
    static constexpr auto KIND = Kind::DECLARATION;
    
    constexpr DeclarationStatement() : Statement{KIND} {}

    Declaration *declaration = nullptr;
};

struct BreakStatement : public Statement {
    static constexpr auto KIND = Kind::BREAK;
    
    constexpr BreakStatement(const Token &token) 
        : Statement{KIND}, token{token} {}

    Token token;
};

struct ContinueStatement : public Statement {
    static constexpr auto KIND = Kind::CONTINUE;
    
    constexpr ContinueStatement(const Token &token) 
        : Statement{KIND}, token{token} {}

    Token token;
};

struct Declaration {
    enum class Kind : u8 {
        VARIABLE,
        FUNCTION,
        CONSTANT,
        TYPE,
    };
    
    constexpr Declaration(Kind kind) : kind{kind} {}

    DEFINE_AST_NODE_DOWNCAST_FUNCTIONS_FOR(Declaration);

    Kind kind;
    Identifier *identifier = nullptr;
};

struct VariableDeclaration : public Declaration {
    static constexpr auto KIND = Kind::VARIABLE;

    constexpr VariableDeclaration(const Token &var) 
        : Declaration{KIND}, var{var} {}

    Token var;
    std::optional<Type *> variable_type;
    std::optional<Expression *> value;
};

struct ConstDeclaration : public Declaration {
    static constexpr auto KIND = Kind::CONSTANT;

    constexpr ConstDeclaration(const Token &const_token) 
        : Declaration{KIND}, const_token{const_token} {}

    Token const_token;
    std::optional<Type *> variable_type;
    Expression *value = nullptr;
};

struct ProcedureDeclaration : public Declaration {
    static constexpr auto KIND = Kind::FUNCTION;
    
    constexpr ProcedureDeclaration() : Declaration{KIND} {}

    TypeProcedure *type = nullptr;
    std::span<Statement*> body;
};

struct TypeDeclaration : public Declaration {
    static constexpr auto KIND = Kind::TYPE;

    constexpr TypeDeclaration(const Token &type_token) 
        : Declaration{KIND}, type_token{type_token} {}

    Token type_token;
    Type *declared_type = nullptr;
};

struct Expression {
    enum class Kind : u8 {
        INTEGER_LITERAL,
        UNARY_OPERATOR,
        BINARY_OPERATOR,
        BOOL_LITERAL,
        IDENTIFIER,
        CALL_OPERATOR,
        STRING_LITERAL,
        FLOAT_LITERAL,
    };

    constexpr Expression(Kind kind) : kind{kind} {}

    DEFINE_AST_NODE_DOWNCAST_FUNCTIONS_FOR(Expression);

    Kind kind;
};

struct IntegerLiteralExpression : public Expression {
    static constexpr auto KIND = Kind::INTEGER_LITERAL;

    constexpr IntegerLiteralExpression(const Token &literal) 
        : Expression{KIND}, literal{literal} {}

    Token literal;
    s64 value = 0;
};

struct IdentifierExpression : public Expression {
    static constexpr auto KIND = Kind::IDENTIFIER;

    constexpr IdentifierExpression() : Expression{KIND} {}

    Identifier *identifier = nullptr;
};

struct UnaryOperatorExpression : public Expression {
    static constexpr auto KIND = Kind::UNARY_OPERATOR;
    
    constexpr UnaryOperatorExpression(const Token &op) 
        : Expression{KIND}, op{op} {}

    Token op;
    Expression *right = nullptr;
};

struct BinaryOperatorExpression : public Expression {
    static constexpr auto KIND = Kind::BINARY_OPERATOR;

    constexpr BinaryOperatorExpression(const Token &op) 
        : Expression{KIND}, op{op} {}
    
    Token op;
    Expression *left = nullptr;
    Expression *right = nullptr;
};

struct BoolLiteralExpression : public Expression {
    static constexpr auto KIND = Kind::BOOL_LITERAL;

    BoolLiteralExpression(const Token &literal) 
        : Expression{KIND}, literal{literal} {}

    Token literal;
    bool value = false;
};

struct CallOperatorExpression : public Expression {
    static constexpr auto KIND = Kind::CALL_OPERATOR;

    constexpr CallOperatorExpression(const Token &open_paren) 
        : Expression{KIND}, open_paren{open_paren} {}

    Token open_paren;
    Token close_paren;
    Expression *callable = nullptr;
    std::span<Expression *> arguments;
};

struct StringLiteralExpression : public Expression {
    static constexpr auto KIND = Kind::STRING_LITERAL;

    constexpr StringLiteralExpression(Token const &string) 
        : Expression{KIND}, string{string} {}

    Token string;
};

struct FloatLiteralExpression : public Expression {
    static constexpr auto KIND = Kind::FLOAT_LITERAL;

    constexpr FloatLiteralExpression(Token const &literal) 
        : Expression{KIND}, literal{literal} {}

    Token literal;
    f64 value = 0.0;
};

struct Type {
    enum class Kind : u8 {
        IDENTIFIER,
        STRUCT,
        POINTER,
        FUNCTION,
        ARRAY,
    };

    constexpr Type(Kind kind) : kind{kind} {}

    DEFINE_AST_NODE_DOWNCAST_FUNCTIONS_FOR(Type);

    Kind kind;
};

struct TypeIdentifier : public Type {
    static constexpr auto KIND = Kind::IDENTIFIER;

    constexpr TypeIdentifier() : Type{KIND} {}

    Identifier *identifier = nullptr;
};

struct TypePointer : public Type {
    static constexpr auto KIND = Kind::POINTER;

    constexpr TypePointer(const Token &pointer_token) 
        : Type{KIND}, pointer_token{pointer_token} {
    }

    Token pointer_token;
    Type *points_to = nullptr;
};

struct TypeArray : public Type {
    static constexpr auto KIND = Kind::ARRAY;

    constexpr TypeArray(const Token &open_bracket) 
        : Type{KIND}, open_bracket{open_bracket} {}

    Token open_bracket;
    Token close_bracket;
    Type *element_type = nullptr;
    Expression *element_count = nullptr;
};

struct ProcedureParameter {
    Identifier *identifier = nullptr;
    Type *type = nullptr;
};

struct TypeProcedure : public Type {
    static constexpr auto KIND = Kind::FUNCTION;

    constexpr TypeProcedure(const Token &fn) : Type{KIND}, fn{fn} {}

    Token fn;
    std::span<ProcedureParameter *> parameters;
    Type *return_type = nullptr;
};

struct StructMember {
    Identifier *identifier = nullptr;
    Type *type = nullptr;
};

struct TypeStruct : public Type {
    static constexpr auto KIND = Kind::STRUCT;
    
    constexpr TypeStruct(const Token &struct_token) 
        : Type{KIND}, struct_token{struct_token} {}

    Token struct_token;
    Token open_brace;
    Token close_brace;
    std::span<StructMember *> members;
    std::span<DeclarationStatement *> declarations;
};

using Node = std::variant<Statement*, Type*, Expression*, Declaration*>;

struct Program {
    std::vector<Statement *> declarations;
    int error_count = 0;
};

std::string node_to_string(Node node, int tabs = 0);
std::string statement_to_string(Statement const *type, int tabs);
std::string expression_to_string(Expression const *type, int tabs);
std::string type_to_string(Type const *type, int tabs = 0, bool include_fn = true);
std::string declaration_to_string(Declaration const *decl, int tabs);

}; // namespace Ast
#endif // #ifndef AST_H