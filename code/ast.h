#ifndef AST_H
#define AST_H

#include <optional>
#include <span>
#include <variant>
#include <string_view>
#include <cassert>

#include "base/types.h"

#include "lexer.h" // Potentially remove lexer.h form here

namespace Ast {

struct Program;

struct Node;

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
        assert(is<T>());                                                       \
        return static_cast<const T *>(this);                                   \
    }                                                                          \
    template <typename T>                                                      \
    T *as()                                                                    \
        requires std::derived_from<T, type>                                    \
    {                                                                          \
        assert(is<T>());                                                       \
                                                                               \
        return static_cast<T *>(this);                                         \
    }

// AST conventions:
// 1) All nodes derive from Node directly or indirectly.
//    Which means that all nodes when created need to specify location,
//    for nodes that do not have location (example, nodes that are 
//    inserted into ast at later stages of compiler) 
//    file location should be set to FileLocation::no_location.
// 2) Pointers in AST are not allowed to be nullptr.  
//    Only exceptions are pointers to Expression and Type which can be nullptr, 
//    but only if there were errors in source code that is being parsed.
// 3) If field of a node is optional, it is represented as std::optional<Node*> field.
// 4) Nodes can not have non-trival destructors.

// TODOs:
// 1) For each node, require all field values in the constructor.
// 2) Since i probably will want to modify AST at later stages of compilation
//    move logic of storing nodes from Parser to some kind of AstStorage.
//    Or just pass Parse to TypeChecker and later stages as well
// 2) Search parser.cpp for more.

struct Node {
    constexpr Node(const FileLocation &location)
        : location{location} {}

    FileLocation location;
};

struct Identifier : Node {
    constexpr Identifier(const FileLocation &location) : Node{location} {}

    std::string_view value;
};

struct Field : public Node {
    constexpr Field(const FileLocation &location) : Node{location} {}

    Identifier *identifier = nullptr;
    Type *type = nullptr;
};

struct Statement : public Node {
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

    constexpr Statement(Kind kind, const FileLocation &location) 
        : Node{location}, kind{kind} {}

    DEFINE_AST_NODE_DOWNCAST_FUNCTIONS_FOR(Statement);

    Kind kind;
};

struct IfStatement : public Statement {
    static constexpr auto KIND = Kind::IF;

    constexpr IfStatement(const FileLocation &if_location) 
        : Statement{KIND, if_location} {}

    struct ElseBranch {
        FileLocation else_location;
        std::span<Statement *> body;
    };

    Expression *condition = nullptr;
    std::span<Statement *> true_branch_body;
    std::optional<ElseBranch> else_branch;
};

struct WhileStatement : public Statement {
    static constexpr auto KIND = Kind::WHILE;

    constexpr WhileStatement(const FileLocation &while_location) 
        : Statement{KIND, while_location} {}

    Expression *condition = nullptr;
    std::span<Statement *> body;
};

struct BlockStatement : public Statement {
    static constexpr auto KIND = Kind::BLOCK;
    
    constexpr BlockStatement(const FileLocation &open_brace_location) 
        : Statement{KIND, open_brace_location} {}

    std::span<Statement *> body;
};

struct ReturnStatement : public Statement {
    static constexpr auto KIND = Kind::RETURN;

    constexpr ReturnStatement(const FileLocation &return_location) 
        : Statement{KIND, return_location} {}

    Expression *value = nullptr;
};

struct AssignmentStatement : public Statement {
    static constexpr auto KIND = Kind::ASSIGNMENT;

    constexpr AssignmentStatement(const FileLocation &assign_location)
        : Statement{KIND, assign_location} {}

    TokenType assign_kind = TokenType::invalid;
    Expression *assignee = nullptr;
    Expression *value = nullptr;
};

struct ExpressionStatement : public Statement {
    static constexpr auto KIND = Kind::EXPRESSION;
    
    constexpr ExpressionStatement(const FileLocation &location) 
        : Statement{KIND, location} {}

    Expression *expression = nullptr;
};

struct DeclarationStatement : public Statement {
    static constexpr auto KIND = Kind::DECLARATION;
    
    constexpr DeclarationStatement(const FileLocation &location) 
        : Statement{KIND, location} {}

    Declaration *declaration = nullptr;
};

struct BreakStatement : public Statement {
    static constexpr auto KIND = Kind::BREAK;
    
    constexpr BreakStatement(const FileLocation &break_location) 
        : Statement{KIND, break_location} {}
};

struct ContinueStatement : public Statement {
    static constexpr auto KIND = Kind::CONTINUE;
    
    constexpr ContinueStatement(const FileLocation &continue_location) 
        : Statement{KIND, continue_location} {}
};

struct Declaration : public Node {
    enum class Kind : u8 {
        VARIABLE,
        FUNCTION,
        CONSTANT,
        TYPE,
    };
    
    constexpr Declaration(Kind kind, const FileLocation &location) 
        :  Node{location}, kind{kind} {}

    DEFINE_AST_NODE_DOWNCAST_FUNCTIONS_FOR(Declaration);

    Kind kind;
    Identifier *identifier = nullptr;
};

struct VariableDeclaration : public Declaration {
    static constexpr auto KIND = Kind::VARIABLE;

    constexpr VariableDeclaration(const FileLocation &var_location) 
        : Declaration{KIND, var_location} {}

    std::optional<Type*> variable_type;
    std::optional<Expression*> value;
};

struct ConstDeclaration : public Declaration {
    static constexpr auto KIND = Kind::CONSTANT;

    constexpr ConstDeclaration(const FileLocation &const_location) 
        : Declaration{KIND, const_location} {}

    std::optional<Type *> variable_type;
    Expression *value = nullptr;
};

struct ProcedureDeclaration : public Declaration {
    static constexpr auto KIND = Kind::FUNCTION;
    
    constexpr ProcedureDeclaration(const FileLocation &location) 
        : Declaration{KIND, location} {}

    TypeProcedure *type = nullptr;
    std::span<Statement*> body;
};

struct TypeDeclaration : public Declaration {
    static constexpr auto KIND = Kind::TYPE;

    constexpr TypeDeclaration(const FileLocation &type_location) 
        : Declaration{KIND, type_location} {}

    Type *declared_type = nullptr;
};

struct Expression : public Node {
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

    constexpr Expression(Kind kind, const FileLocation &location) 
        : Node{location}, kind{kind} {}

    DEFINE_AST_NODE_DOWNCAST_FUNCTIONS_FOR(Expression);

    Kind kind;
};

struct IntegerLiteralExpression : public Expression {
    static constexpr auto KIND = Kind::INTEGER_LITERAL;

    constexpr IntegerLiteralExpression(const FileLocation &literal_location) 
        : Expression{KIND, literal_location} {}

    s64 value = 0;
};

struct IdentifierExpression : public Expression {
    static constexpr auto KIND = Kind::IDENTIFIER;

    constexpr IdentifierExpression(const FileLocation &location) 
        : Expression{KIND, location} {}

    Identifier *identifier = nullptr;
};

struct UnaryOperatorExpression : public Expression {
    static constexpr auto KIND = Kind::UNARY_OPERATOR;
    
    constexpr UnaryOperatorExpression(const FileLocation &op_location) 
        : Expression{KIND, op_location} {}

    TokenType op = TokenType::invalid;
    Expression *right = nullptr;
};

struct BinaryOperatorExpression : public Expression {
    static constexpr auto KIND = Kind::BINARY_OPERATOR;

    constexpr BinaryOperatorExpression(const FileLocation &op_location) 
        : Expression{KIND, op_location} {}
    
    TokenType op = TokenType::invalid;
    Expression *left = nullptr;
    Expression *right = nullptr;
};

struct BoolLiteralExpression : public Expression {
    static constexpr auto KIND = Kind::BOOL_LITERAL;

    BoolLiteralExpression(const FileLocation &literal_location) 
        : Expression{KIND, literal_location} {}

    bool value = false;
};

struct CallOperatorExpression : public Expression {
    static constexpr auto KIND = Kind::CALL_OPERATOR;

    constexpr CallOperatorExpression(const FileLocation &open_paren_location) 
        : Expression{KIND, open_paren_location} {}

    Expression *callable = nullptr;
    std::span<Expression *> arguments;
};

struct StringLiteralExpression : public Expression {
    static constexpr auto KIND = Kind::STRING_LITERAL;

    constexpr StringLiteralExpression(const FileLocation &string_location) 
        : Expression{KIND, string_location} {}

    std::string_view string;
};

struct FloatLiteralExpression : public Expression {
    static constexpr auto KIND = Kind::FLOAT_LITERAL;

    constexpr FloatLiteralExpression(const FileLocation &literal_location) 
        : Expression{KIND, literal_location} {}

    f64 value = 0.0;
};

struct Type : public Node {
    enum class Kind : u8 {
        IDENTIFIER,
        STRUCT,
        POINTER,
        FUNCTION,
        ARRAY,
    };

    constexpr Type(Kind kind, const FileLocation &location) 
        : Node{location}, kind{kind} {}

    DEFINE_AST_NODE_DOWNCAST_FUNCTIONS_FOR(Type);

    Kind kind;
};

struct TypeIdentifier : public Type {
    static constexpr auto KIND = Kind::IDENTIFIER;

    constexpr TypeIdentifier(const FileLocation &location) 
        : Type{KIND, location} {}

    // temporary
    std::string get_full_type_name() const {
        auto result = std::string{};
        for (usize i = 0; i < identifier.size(); ++i) {
            result += identifier[i]->value;

            if (i != identifier.size() - 1) {
                result += '.';
            }
        }
        return result;
    }

    // Parts of path to identifier. For example, for type A.B span will contain
    // two identifiers: A, B.
    std::span<Identifier *> identifier;
};

struct TypePointer : public Type {
    static constexpr auto KIND = Kind::POINTER;

    constexpr TypePointer(const FileLocation &pointer_location) 
        : Type{KIND, pointer_location} {
    }

    Type *points_to = nullptr;
};

struct TypeArray : public Type {
    static constexpr auto KIND = Kind::ARRAY;

    constexpr TypeArray(const FileLocation &open_bracket_location) 
        : Type{KIND, open_bracket_location} {}

    Type *element_type = nullptr;
    Expression *element_count = nullptr;
};

struct TypeProcedure : public Type {
    static constexpr auto KIND = Kind::FUNCTION;

    constexpr TypeProcedure(const FileLocation &fn_location) 
        : Type{KIND, fn_location} {}

    std::span<Field*> parameters;
    Type *return_type = nullptr;
};

struct TypeStruct : public Type {
    static constexpr auto KIND = Kind::STRUCT;
    
    constexpr TypeStruct(const FileLocation &struct_location) 
        : Type{KIND, struct_location} {}

    std::span<Field*> members;
    std::span<DeclarationStatement *> declarations;
};

struct Program {
    std::span<Statement *> declarations;
    u64 error_count = 0;
};

std::string statement_to_string(Statement const *type, u64 tabs);
std::string expression_to_string(Expression const *type, u64 tabs);
std::string type_to_string(Type const *type, u64 tabs = 0, bool include_fn = true);
std::string declaration_to_string(Declaration const *decl, u64 tabs);

}; // namespace Ast
#endif // #ifndef AST_H