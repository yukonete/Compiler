#include "Base.h"

#include "Parser.h"
#include "Lexer.h"

using namespace Ast;

Parser::Parser(Lex::Lexer *lexer, Arena *arena) : lexer_{lexer}, arena_{arena} {}

ParseProgramResult Parser::ParseProgram() 
{
	Program program;
	bool invalid = false;
	while (lexer_->PeekNextToken().type != Lex::TokenType::invalid) 
	{
		auto statement = ParseStatement();
		program.statements.push_back(statement);

		if (statement->type == NodeType::invalid) 
		{
			invalid = true;
		}
	}
	return {program, invalid};
}

Statement* Parser::ParseStatement() 
{
	auto parse_statement = [this]() -> Statement* 
	{ 
		const auto token_type = lexer_->PeekNextToken().type;
		switch (token_type) {
			using enum Lex::TokenType;

		case identifier: {
			const auto next_token_type = lexer_->PeekToken(1).type;
			if (next_token_type == static_cast<Lex::TokenType>(':')) {
				return ParseVariableDeclarationStatement();
			}
			if (next_token_type == static_cast<Lex::TokenType>('=')) {
				return ParseAssignmentStatement();
			}
			break;
		};

		case keyword_if: return ParseIfStatement();
		case keyword_while: return ParseWhileStatement();
		}
		return ParseExpressionStatement();
	};

	auto statement = parse_statement();
	Assert(statement);
	if (current_statement_invalid_) 
	{
		current_statement_invalid_ = false;
		statement->type = NodeType::invalid;
	}
	return statement;
}

Statement* Parser::ParseInvalidStatement()
{
	auto statement = arena_->PushItem<Statement>(NodeType::invalid);
	return statement;
}

ExpressionStatement* Parser::ParseExpressionStatement()
{
	auto statement = arena_->PushItem<ExpressionStatement>();
	current_statement_invalid_ = true;
	return statement;
}

IfStatement* Parser::ParseIfStatement()
{
	auto statement = arena_->PushItem<IfStatement>();
	current_statement_invalid_ = true;
	return statement;
}

WhileStatement* Parser::ParseWhileStatement()
{
	auto statement = arena_->PushItem<WhileStatement>();
	current_statement_invalid_ = true;
	return statement;
}

VariableDeclarationStatement* Parser::ParseVariableDeclarationStatement() 
{
	auto statement = arena_->PushItem<VariableDeclarationStatement>();
	statement->identifier = AssumeToken(Lex::TokenType::identifier).Identifier();
	
	if (!ExpectToken(Lex::TokenType::identifier)) 
	{
		return statement;
	}

	statement->type_identifier = expected_token_.Identifier();
	
	if (NextTokenIs('=')) 
	{
		lexer_->EatToken();
		statement->value = ParseExpression();
	}
	
	ExpectToken(';');
	return statement;
}

AssignmentStatement* Parser::ParseAssignmentStatement()
{
	auto statement = arena_->PushItem<AssignmentStatement>();
	current_statement_invalid_ = true;
	return statement;
}

Expression* Parser::ParseExpression()
{
	auto expression = arena_->PushItem<Expression>(Ast::NodeType::invalid);
	return expression;
}

const Lex::Token& Parser::AssumeToken(Lex::TokenType type) 
{
	const auto &token = lexer_->PeekNextToken();
	Assert(token.type == type);
	lexer_->EatToken();
	return token;
}

bool Parser::ExpectToken(Lex::TokenType type) 
{
	const auto& token = lexer_->PeekNextToken();
	if (token.type != type) 
	{
		// TODO: Compiler error and skip tokens until next ;
		current_statement_invalid_ = true;
		return false;
	}

	lexer_->EatToken();
	expected_token_ = token;
	return true;
}

bool Parser::ExpectToken(char type) 
{
	return ExpectToken(static_cast<Lex::TokenType>(type));
}

bool Parser::NextTokenIs(Lex::TokenType type) 
{
	return lexer_->PeekNextToken().type == type;
}

bool Parser::NextTokenIs(char type) 
{
	return lexer_->PeekNextToken().type == static_cast<Lex::TokenType>(type);
}