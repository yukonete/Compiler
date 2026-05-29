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

struct Identifier {
    Token token;

    std::string_view value() const {
        return token.value;
    }
};

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

using StatementVariant =
    std::variant<ExpressionStatement *, IfStatement *, WhileStatement *,
                 AssignmentStatement *, BlockStatement *, ReturnStatement *,
                 DeclarationStatement *, ContinueStatement *, BreakStatement *>;

struct Statement : AstVariantBase<Statement, StatementVariant> {
    StatementVariant variant;
};

using DeclarationVariant =
    std::variant<VariableDeclaration *, ProcedureDeclaration *,
                 ConstDeclaration *, TypeDeclaration *>;

struct Declaration : AstVariantBase<Declaration, DeclarationVariant> {             
    DeclarationVariant variant;
    Identifier *identifier = nullptr;
};

using ExpressionVariant =
    std::variant<IntegerLiteralExpression *, UnaryOperatorExpression *,
                 BinaryOperatorExpression *, BoolLiteralExpression *,
                 IdentifierExpression *, CallOperatorExpression *,
                 ArraySubscriptExpression *, StringLiteralExpression *,
                 FloatLiteralExpression *>;

struct Expression : AstVariantBase<Expression, ExpressionVariant> {
    ExpressionVariant variant;
};

using TypeVariant = std::variant<TypeIdentifier *, TypeStruct *, TypeArray *, TypePointer *,
                 TypeProcedure *>;

struct Type : AstVariantBase<Type, TypeVariant> {
    TypeVariant variant;
};

struct IfStatement : public Statement {
    IfStatement(const Token &if_token) : if_token{if_token} {
        variant = this;
    }

    struct ElseBranch {
        Token else_token;
        Statement *branch_body = nullptr;
    };

    Token if_token;
    Expression *condition = nullptr;
    Statement *true_branch_body = nullptr;

    std::optional<ElseBranch> false_branch;
};

struct WhileStatement : public Statement {
    WhileStatement(const Token &while_token) : while_token{while_token} {
        variant = this;
    }

    Token while_token;
    Expression *condition = nullptr;
    Statement *body = nullptr;
};

struct BlockStatement : public Statement {
    BlockStatement(const Token &open_brace) : open_brace{open_brace} {
        variant = this;
    }

    Token open_brace;
    Token close_brace;
    std::span<Statement *> body;
};

struct ReturnStatement : public Statement {
    ReturnStatement(const Token &return_token) : return_token{return_token} {
        variant = this;
    }

    Token return_token;
    Expression *value = nullptr;
};

struct AssignmentStatement : public Statement {
    AssignmentStatement(const Token &assign) : assign{assign} {
        variant = this;
    }

    Token assign;
    Expression *assignee = nullptr;
    Expression *value = nullptr;
};

struct ExpressionStatement : public Statement {
    ExpressionStatement() {
        variant = this;
    }

    Expression *expression = nullptr;
};

struct DeclarationStatement : public Statement {
    DeclarationStatement() {
        variant = this;
    }

    Declaration *declaration = nullptr;
};

struct BreakStatement : public Statement {
    BreakStatement(Token const &token) : token{token} {
        variant = this;
    }

    Token token;
};

struct ContinueStatement : public Statement {
    ContinueStatement(Token const &token) : token{token} {
        variant = this;
    }

    Token token;
};

struct VariableDeclaration : public Declaration {
    VariableDeclaration(const Token &var) : var{var} {
        variant = this;
    }

    Token var;
    std::optional<Type *> variable_type;
    std::optional<Expression *> value;
};

struct ConstDeclaration : public Declaration {
    ConstDeclaration(const Token &const_token) : const_token{const_token} {
        variant = this;
    }

    Token const_token;
    std::optional<Type *> variable_type;
    Expression *value = nullptr;
};

struct ProcedureDeclaration : public Declaration {
    ProcedureDeclaration() {
        variant = this;
    }

    TypeProcedure *type = nullptr;
    BlockStatement *body = nullptr;
};

struct TypeDeclaration : public Declaration {
    TypeDeclaration(const Token &type_token) : type_token{type_token} {
        variant = this;
    }

    Token type_token;
    Type *declared_type = nullptr;
};

struct TypeIdentifier : public Type {
    TypeIdentifier() {
        variant = this;
    }

    Identifier *identifier = nullptr;
};

struct TypePointer : public Type {
    TypePointer(const Token &pointer_token) : pointer_token{pointer_token} {
        variant = this;
    }

    Token pointer_token;
    Type *points_to = nullptr;
};

struct TypeArray : public Type {
    TypeArray(const Token &open_bracket) : open_bracket{open_bracket} {
        variant = this;
    }

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
    TypeProcedure(const Token &fn) : fn{fn} {
        variant = this;
    }

    Token fn;
    std::span<ProcedureParameter *> parameters;
    Type *return_type = nullptr;
};

struct StructMember {
    Identifier *identifier = nullptr;
    Type *type = nullptr;
};

struct TypeStruct : public Type {
    TypeStruct(const Token &struct_token) : struct_token{struct_token} {
        variant = this;
    }

    Token struct_token;
    Token open_brace;
    Token close_brace;
    std::span<StructMember *> members;
    std::span<Declaration *> declarations;
};

struct IntegerLiteralExpression : public Expression {
    IntegerLiteralExpression(const Token &literal) : literal{literal} {
        variant = this;
    }

    Token literal;
    s64 value = 0;
};

struct IdentifierExpression : public Expression {
    IdentifierExpression() {
        variant = this;
    }

    Identifier *identifier = nullptr;
};

struct UnaryOperatorExpression : public Expression {
    UnaryOperatorExpression(const Token &op) : op{op} {
        variant = this;
    }

    Token op;
    Expression *right = nullptr;
};

struct BinaryOperatorExpression : public Expression {
    BinaryOperatorExpression(const Token &op) : op{op} {
        variant = this;
    }
    
    Token op;
    Expression *left = nullptr;
    Expression *right = nullptr;
};

struct BoolLiteralExpression : public Expression {
    BoolLiteralExpression(const Token &literal) : literal{literal} {
        variant = this;
    }

    Token literal;
    bool value = false;
};

struct CallOperatorExpression : public Expression {
    CallOperatorExpression(const Token &open_paren) : open_paren{open_paren} {
        variant = this;
    }

    Token open_paren;
    Token close_paren;
    Expression *callable = nullptr;
    std::span<Expression *> arguments;
};

struct ArraySubscriptExpression : public Expression {
    ArraySubscriptExpression(const Token &open_bracket) : open_bracket{open_bracket} {
        variant = this;
    }

    Token open_bracket;
    Token close_bracket;
    Expression *array = nullptr;
    Expression *index = nullptr;
};

struct StringLiteralExpression : public Expression {
    StringLiteralExpression(Token const &string) : string{string} {
        variant = this;
    }

    Token string;
};

struct FloatLiteralExpression : public Expression {
    FloatLiteralExpression(Token const &literal) : literal{literal} {
        variant = this;
    }

    Token literal;
    f64 value = 0.0;
};

using Node = std::variant<Statement*, Type*, Expression*, Declaration*>;

struct Program {
    std::vector<Statement *> declarations;
    int error_count = 0;
};

std::string node_to_string(Node node, int tabs = 0);
std::string statement_to_string(Statement const *type, int tabs);
std::string expression_to_string(Expression const *type, int tabs);
std::string type_to_string(Type const *type, int tabs = 0);
std::string declaration_to_string(Declaration const *decl, int tabs);

}; // namespace Ast
#endif // #ifndef AST_H