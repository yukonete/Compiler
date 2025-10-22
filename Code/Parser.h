#pragma once

#include <vector>
#include <string_view>
#include <optional>

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
	expression_true_literal,
	expression_false_literal,
	expression_identifier,
	expression_unary_operator,
	expression_binary_operator,
	expression_cast,
};

struct Program;

struct Node;

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
struct Identifier;
struct UnaryOperator;
struct BinaryOperator;
struct BoolLiteral;

struct Program 
{
	std::vector<Statement*> statements;
};

struct Node 
{
	NodeType type;
	Node(NodeType type_) : type{ type_ } {} 
};

struct Statement : public Node 
{
	Statement(NodeType type_) : Node(type_) {}
};

struct Expression : public Node
{
	Expression(NodeType type_) : Node(type_) {}
};

struct VariableDeclarationStatement : public Statement 
{
	VariableDeclarationStatement() : Statement(NodeType::statement_variable_declaration) {}
	std::string_view identifier;
	std::string_view type_identifier;
	std::optional<Expression*> value;
};

struct IfStatement : public Statement 
{
	IfStatement() : Statement(NodeType::statement_if) {}
	Expression *condition = nullptr;
	BlockStatement *true_branch = nullptr;
	std::optional<BlockStatement*> false_branch;
};

struct WhileStatement : public Statement 
{
	WhileStatement() : Statement(NodeType::statement_if) {}
	Expression *condition = nullptr;
	BlockStatement *body = nullptr;
};

struct AssignmentStatement : public Statement 
{
	AssignmentStatement() : Statement(NodeType::statement_assingment) {}
	std::string_view identifier;
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
	s64 value = 0;
};

struct Identifier : public Expression 
{
	Identifier() : Expression(NodeType::expression_identifier) {}
	std::string_view name;
};

struct UnaryOperator : public Expression 
{
	UnaryOperator() : Expression(NodeType::expression_unary_operator) {}
	Lex::TokenType op = Lex::TokenType::invalid;
	Expression *right = nullptr;
};

struct BinaryOperator : public Expression 
{
	BinaryOperator() : Expression(NodeType::expression_binary_operator) {}
	Lex::TokenType op = Lex::TokenType::invalid;
	Expression *left = nullptr;
	Expression *right = nullptr;
};

struct TrueLiteral : public Expression 
{
	TrueLiteral() : Expression(NodeType::expression_true_literal) {}
};

struct FalseLiteral : public Expression 
{
	FalseLiteral() : Expression(NodeType::expression_false_literal) {}
};

struct ParseProgramResult 
{
	Program program;
	bool valid = false;
};

class Parser 
{
public:
	Parser(Lex::Lexer *lexer, Arena *arena);
	ParseProgramResult ParseProgram();
private:
	Statement* ParseStatement();

	// Eats tokens untill ; so that next statement can be correctly parsed
	Statement* ParseInvalidStatement();

	ExpressionStatement* ParseExpressionStatement();
	IfStatement* ParseIfStatement();
	WhileStatement* ParseWhileStatement();
	VariableDeclarationStatement* ParseVariableDeclarationStatement();
	AssignmentStatement* ParseAssignmentStatement();

	Expression* ParseExpression();
	TrueLiteral* ParseTrueLiteral();
	FalseLiteral* ParseFalseLiteral();

	const Lex::Token& AssumeToken(Lex::TokenType type);
	bool ExpectToken(Lex::TokenType type);
	bool ExpectToken(char type);

	bool NextTokenIs(Lex::TokenType type);
	bool NextTokenIs(char type);

	Lex::Lexer *lexer_;
	Arena *arena_;
	Lex::Token expected_token_;
	bool current_statement_invalid_ = false;
};

};