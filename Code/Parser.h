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

	
	// Always leave this as first declaration kind because it used in comparison to determine if node is a declaration
	declaration_variable,
	declaration_const,
	declaration_procedure,
	// Always leave this as last declaration kind because it used in comparison to determine if node is a declaration
	declaration_type,


	// Always leave this as first statement kind because it used in comparison to determine if node is a statement 
	statement_if,
	statement_while,
	statement_assingment,
	statement_block,
	// Always leave this as last statement kind because it used in comparison to determine if node is a statement 
	statement_expression,


	expression_integer_literal,
	expression_bool_literal,
	expression_identifier,
	expression_unary_operator,
	expression_binary_operator,

	expression_cast,


	type_identifier,
	type_pointer,
};

struct Program;

struct Node;

using Identifier = Token;

struct Declaration;
struct VariableDeclaration;
struct ProcedureDeclaration;
struct ConstDeclaration;
struct TypeDeclaration;
struct StructDeclaration;

struct Statement;
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

struct Type;

struct Node 
{
	NodeType type;
	Node(NodeType type_) : type{type_} {}
};

struct Declaration : public Node
{
	Declaration(NodeType type_) : Node(type_) {}
};

struct Statement : public Node
{
	Statement(NodeType type_) : Node(type_) {}
};

struct Expression : public Node
{
	Expression(NodeType type_) : Node(type_) {}
};

struct Type : public Node
{
	Type(NodeType type_) : Node(type_) {}
};

struct TypeIdentifier : public Type
{
	TypeIdentifier() : Type(NodeType::type_identifier) {}
	Identifier identifier;
};

struct TypePointer : public Type
{
	TypePointer() : Type(NodeType::type_pointer) {}
	Type *points_to = nullptr;
};

struct VariableDeclaration : public Declaration 
{
	VariableDeclaration() : Declaration(NodeType::declaration_variable) {}
	Identifier identifier;
	Type *variable_type = nullptr;
	std::optional<Expression*> value;
};

struct ConstDeclaration : public Declaration
{
	ConstDeclaration() : Declaration(NodeType::declaration_const) {}
	Identifier identifier;
	Type *variable_type = nullptr;
	Expression *value = nullptr;
};

struct ProcedureDeclaration : public Declaration
{
	ProcedureDeclaration() : Declaration(NodeType::declaration_procedure) {}
};

struct TypeDeclaration : public Declaration
{
	TypeDeclaration() : Declaration(NodeType::declaration_type) {}
	Identifier identifier;
	Type *declared_type = nullptr;
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
	std::span<Node*> body;
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

struct Program
{
	std::vector<Declaration*> declarations;
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
	Parser(std::string_view input, Arena *arena, FILE *log = stderr);

	// If error_count is not 0 then is it not safe to use AST because 
	// tokens in indentifiers might have incorrect value in union 
	// (well there will just be garabage instead of string)
	Program ParseProgram();
private:
	Declaration* ParseDeclaration();
	ConstDeclaration* ParseConstantDeclaration();
	VariableDeclaration* ParseVariableDeclaration();
	TypeDeclaration* ParseTypeDeclaration();
	ProcedureDeclaration* ParseProcedureDeclaration();

	Statement* ParseStatement();
	ExpressionStatement* ParseExpressionStatement();
	IfStatement* ParseIfStatement();
	WhileStatement* ParseWhileStatement();
	AssignmentStatement* ParseAssignmentStatement();
	BlockStatement* ParseBlockStatement();
	
	Node* ParseStatementOrDeclaration();

	Expression* ParseExpression(Precedence precedence = Precedence::lowest);
	Expression* ParseUnaryExpression();
	Expression* ParseBinaryExpression(Expression *left);
	
	Type* ParseType();

	template <typename NodeType>
	NodeType* New() 
	{
		return arena_->PushItem<NodeType>();
	};

	template <typename NodeType>
	NodeType* New(NodeType type) 
	{
		return arena_->PushItem<NodeType>(type);
	};

	const Token& ExpectToken(TokenType type);

	bool NextTokenIs(TokenType type);

	template <typename ...Args>
	void LogError(const Token &token, std::format_string<Args...> fmt, Args&&... args) 
	{
		std::print(log_, "({}, {}): ", token.start.line, token.start.column);
		std::println(log_, fmt, std::forward<Args>(args)...);
	}

	Lexer lexer_;
	Arena *arena_ = nullptr;
	FILE *log_ = nullptr;
	
	Token expected_token_;
	int error_count_ = 0;
};

};