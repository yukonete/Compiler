#include <print>
#include <vector>

#include "Base.h"

#include "Parser.h"
#include "Lexer.h"

using namespace Ast;

Parser::Parser(Lex::Lexer *lexer, Arena *arena, FILE *log) : lexer_{lexer}, arena_{arena}, log_{log} {}

ParseProgramResult Parser::ParseProgram() 
{
	Program program;
	bool invalid = false;
	while (lexer_->PeekNextToken().type != Lex::TokenType::invalid) 
	{
		auto statement = ParseStatement();
		program.statements.push_back(statement);

		if (current_statement_invalid_)
		{
			current_statement_invalid_ = false;
			statement->type = NodeType::invalid;
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
#pragma warning( push )
#pragma warning( disable : 4063 )
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
		case Lex::TokenType{'{'}: return ParseBlockStatement();
#pragma warning( pop )
		}
		return ParseExpressionStatement();
	};

	auto statement = parse_statement();
	Assert(statement);
	return statement;
}

void Parser::EatTokensUntilSemicolon() {
	while (!(NextTokenIs(Lex::TokenType{';'}) || NextTokenIs(Lex::TokenType::invalid))) {
		lexer_->EatToken();
	}
	lexer_->EatToken();
}

ExpressionStatement* Parser::ParseExpressionStatement()
{
	auto statement = arena_->PushItem<ExpressionStatement>();
	statement->expression = ParseExpression();
	return statement;
}

IfStatement* Parser::ParseIfStatement()
{
	auto statement = arena_->PushItem<IfStatement>();
	AssumeToken(Lex::TokenType::keyword_if);
	if (!ExpectToken(Lex::TokenType{'('}))
	{
		return statement;
	}
	statement->condition = ParseExpression();
	if (!ExpectToken(Lex::TokenType{ ')' }))
	{
		return statement;
	}
	if (!ExpectToken(Lex::TokenType{'{'})) 
	{
		return statement;
	}
	lexer_->UneatToken();
	statement->true_branch = ParseBlockStatement();
	if (!NextTokenIs(Lex::TokenType::keyword_else)) 
	{
		return statement;
	}

	lexer_->EatToken();
	if (NextTokenIs(Lex::TokenType::keyword_if)) 
	{
		statement->false_branch = ParseIfStatement();
		return statement;
	}

	// TODO: Change error message to be something like: "Only block and if statements are allowed here"
	if (!ExpectToken(Lex::TokenType{'{'}))
	{
		return statement;
	}
	statement->false_branch = ParseBlockStatement();

	return statement;
}

WhileStatement* Parser::ParseWhileStatement()
{
	auto statement = arena_->PushItem<WhileStatement>();
	AssumeToken(Lex::TokenType::keyword_while);
	if (!ExpectToken(Lex::TokenType{'('}))
	{
		return statement;
	}
	statement->condition = ParseExpression();
	if (!ExpectToken(Lex::TokenType{')'}))
	{
		return statement;
	}
	if (!ExpectToken(Lex::TokenType{ '{' }))
	{
		return statement;
	}
	lexer_->UneatToken();
	statement->body = ParseBlockStatement();
	return statement;
}

VariableDeclarationStatement* Parser::ParseVariableDeclarationStatement() 
{
	auto statement = arena_->PushItem<VariableDeclarationStatement>();
	statement->identifier = AssumeToken(Lex::TokenType::identifier).Identifier();
	AssumeToken(Lex::TokenType{':'});
	
	if (!ExpectToken(Lex::TokenType::identifier)) 
	{
		return statement;
	}

	statement->type_identifier = expected_token_.Identifier();
	
	if (NextTokenIs(Lex::TokenType{'='}))
	{
		lexer_->EatToken();
		statement->value = ParseExpression();
	}
	
	ExpectToken(Lex::TokenType{';'});
	return statement;
}

AssignmentStatement* Parser::ParseAssignmentStatement()
{
	auto statement = arena_->PushItem<AssignmentStatement>();
	statement->identifier = AssumeToken(Lex::TokenType::identifier).Identifier();
	AssumeToken(Lex::TokenType{'='});
	statement->value = ParseExpression();

	ExpectToken(Lex::TokenType{';'});
	return statement;
}

BlockStatement* Parser::ParseBlockStatement()
{
	auto block = arena_->PushItem<BlockStatement>();
	AssumeToken(Lex::TokenType{'{'});

	// TODO: How many statements we can expect on average? Reserve that amount.
	std::vector<Statement*> temp;
	while (!(NextTokenIs(Lex::TokenType{'}'}) || NextTokenIs(Lex::TokenType::invalid))) 
	{
		const auto statement = ParseStatement();
		temp.push_back(statement);
	}
	lexer_->EatToken();

	block->statements = arena_->PushArray<Statement*>(temp.size());
	std::ranges::copy(temp, block->statements.begin());

	return block;
}

Expression* Parser::ParseExpression()
{
	auto expression = arena_->PushItem<Expression>(Ast::NodeType::invalid);
	// Temporary
	ExpectToken(Lex::TokenType{';'});
	current_statement_invalid_ = true;
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
		LogError(token, "Expected {}. Got {}.", type, token.type);
		EatTokensUntilSemicolon();
		current_statement_invalid_ = true;
		return false;
	}

	lexer_->EatToken();
	expected_token_ = token;
	return true;
}

bool Parser::NextTokenIs(Lex::TokenType type) 
{
	return lexer_->PeekNextToken().type == type;
}