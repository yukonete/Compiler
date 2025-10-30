#pragma once

#include <vector>
#include <string_view>
#include <optional>
#include <print>

#include "Base.h"
#include "Lexer.h"

namespace Ast 
{

enum class NodeType 
{
	invalid,

	statement_variable_declaration,
	statement_procedure_declaration,
	statement_if,
	statement_while,
	statement_assingment,
	statement_block,
	statement_expression,

	expression_integer_literal,
	expression_bool_literal,
	expression_identifier,
	expression_unary_operator,
	expression_binary_operator,
	expression_cast,
};

struct Program;

struct Node;

using Identifier = Token;

struct Statement;
struct VariableDeclarationStatement;
struct ProcedureDeclarationStatement;
struct IfStatement;
struct WhileStatement;
struct AssignmentStatement;
struct BlockStatement;
struct ExpressionStatement;

struct Expression;
struct IntegerLiteral;
struct UnaryOperator;
struct BinaryOperator;
struct BoolLiteral;
struct IdentifierExpression;
struct Declaration;
struct StructDeclaration;

struct Type;

// Program is an array of declarations because only declarations are allowed at top level scope.
struct Program
{
	std::vector<Statement*> declarations;
};

struct Node 
{
	NodeType type;
	Node(NodeType type_) : type{type_} {} 
};

struct Statement : public Node 
{
	Statement(NodeType type_) : Node(type_) {}
};

struct Expression : public Node
{
	Expression(NodeType type_) : Node(type_) {}
};

enum class TypeKind 
{
	identifier,
};

struct Type
{
	Type(TypeKind kind_) : kind{kind_}{}
	TypeKind kind;
};

struct TypeIdentifier : public Type
{
	TypeIdentifier() : Type(TypeKind::identifier) {}
	Identifier identifier;
};

struct VariableDeclarationStatement : public Statement 
{
	VariableDeclarationStatement() : Statement(NodeType::statement_variable_declaration) {}
	Identifier identifier;
	Type *variable_type = nullptr;
	std::optional<Expression*> value;
};

struct IfStatement : public Statement 
{
	IfStatement() : Statement(NodeType::statement_if) {}
	Expression *condition = nullptr;
	Statement *true_branch = nullptr;

	std::optional<Statement*> false_branch = nullptr;
};

struct WhileStatement : public Statement 
{
	WhileStatement() : Statement(NodeType::statement_while) {}
	Expression *condition = nullptr;
	Statement *body = nullptr;
};

struct BlockStatement : public Statement
{
	BlockStatement() : Statement(NodeType::statement_block) {}
	std::span<Statement*> statements;
};

struct AssignmentStatement : public Statement 
{
	AssignmentStatement() : Statement(NodeType::statement_assingment) {}
	Identifier identifier;
	Expression *value = nullptr;
};

struct ExpressionStatement : public Statement 
{
	ExpressionStatement() : Statement(NodeType::statement_expression) {}
	Expression *expression = nullptr;
};

struct InvalidExpression : public Expression
{
	InvalidExpression() : Expression(NodeType::invalid) {}
};

struct IntegerLiteral : public Expression 
{
	IntegerLiteral() : Expression(NodeType::expression_integer_literal) {}
	Token value;
};

struct IdentifierExpression : public Expression 
{
	IdentifierExpression() : Expression(NodeType::expression_identifier) {}
	Identifier identifier;
};

struct UnaryOperator : public Expression 
{
	UnaryOperator() : Expression(NodeType::expression_unary_operator) {}
	TokenType op = TokenType::invalid;
	Expression *right = nullptr;
};

struct BinaryOperator : public Expression 
{
	BinaryOperator() : Expression(NodeType::expression_binary_operator) {}
	TokenType op = TokenType::invalid;
	Expression *left = nullptr;
	Expression *right = nullptr;
};

struct BoolLiteral : public Expression 
{
	BoolLiteral() : Expression(NodeType::expression_bool_literal) {}
	bool value = false;
};

struct ParseProgramResult 
{
	Program program;
	int error_count = 0;
};

enum class Precedence
{
	lowest,
	equals, // ==, !=
	comparison, // <, <=, >, >=
	plus, // +, -
	multiply, // *, /, %
	prefix // -, !
};

std::string NodeToString(const Node *node, int tabs = 0);

class Parser 
{
public:
	Parser(Lexer *lexer, Arena *arena, FILE *log = stderr);

	// If error_count is not 0 then is it not safe to use AST because 
	// tokens in indentifiers might have incorrect value in union 
	// (well there will just be garabage instead of string)
	ParseProgramResult ParseProgram();
private:
	Statement* ParseStatement();
	
	ExpressionStatement* ParseExpressionStatement();
	IfStatement* ParseIfStatement();
	WhileStatement* ParseWhileStatement();
	VariableDeclarationStatement* ParseVariableDeclarationStatement();
	AssignmentStatement* ParseAssignmentStatement();
	BlockStatement* ParseBlockStatement();
	
	Type* ParseType();

	Expression* ParseExpression(Precedence precedence = Precedence::lowest);
	Expression* ParseUnaryExpression();
	Expression* ParseBinaryExpression(Expression *left);
	
	template <typename NodeType>
	NodeType* New() 
	{
		return arena_->PushItem<NodeType>();
	};

	// If next token is not expected token this function will eat tokens until ";", log the error and
	// set current_statement_invalid_ to true.
	// Otherwise expected token will be stored in expected_token_ (and token will be eaten).
	const Token& ExpectToken(TokenType type);

	bool NextTokenIs(TokenType type);

	template <typename ...Args>
	void LogError(const Token &token, std::format_string<Args...> fmt, Args&&... args) 
	{
		std::print(log_, "({}, {}): ", token.start.line, token.start.column);
		std::println(log_, fmt, std::forward<Args>(args)...);
	}

	Lexer *lexer_ = nullptr;
	Arena *arena_ = nullptr;
	FILE *log_ = nullptr;
	
	Token expected_token_;
	int error_count_ = 0;
};

};