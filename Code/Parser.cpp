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
	return {program, !invalid};
}

Statement* Parser::ParseStatement() 
{
	auto parse_statement = [this]() -> Statement* 
	{ 
		const auto token_type = lexer_->PeekNextToken().type;
		switch (token_type) 
		{
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
	ExpectToken(Lex::TokenType{';'});
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
	lexer_->UneatToken();
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

	// TODO: How many statements can we expect on average? Reserve that amount.
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

// Returns precedence of a binary operator.
static Precedence TokenTypeToPrecedense(Lex::TokenType type)
{
	switch (type) 
	{
	using enum Lex::TokenType;
	case equals:
	case not_equals:
		return Precedence::equals;

	case Lex::TokenType{'<'}:
	case Lex::TokenType{'>'}:
	case less_equals:
	case greater_equals:
		return Precedence::comparison;

	case Lex::TokenType{'+'}:
	case Lex::TokenType{'-'}:
		return Precedence::plus;

	case Lex::TokenType{'*'}:
	case Lex::TokenType{'/'}:
	case Lex::TokenType{'%'}:
		return Precedence::multiply;

	default:
		return Precedence::lowest;
	}
}

Expression* Parser::ParseUnaryExpression()
{
	Expression *expression = nullptr;
	const auto& token = lexer_->PeekNextToken();
	lexer_->EatToken();
	switch (token.type)
	{
		using enum Lex::TokenType;

		case identifier:
		{
			auto ident = arena_->PushItem<Identifier>();
			ident->name = token.Identifier();
			expression = ident;
			break;
		}

		case integer:
		{
			auto integer_literal = arena_->PushItem<IntegerLiteral>();
			integer_literal->value = token.Integer();
			expression = integer_literal;
			break;
		}

		case keyword_true:
		case keyword_false:
		{
			auto bool_literal = arena_->PushItem<BoolLiteral>();
			bool_literal->value = (token.type == keyword_true);
			expression = bool_literal;
			break;
		}

		case Lex::TokenType{'-'}:
		case Lex::TokenType{'!'}:
		{
			auto unary_operator = arena_->PushItem<UnaryOperator>();
			unary_operator->op = token.type;
			unary_operator->right = ParseExpression(Precedence::prefix);
			expression = unary_operator;
			break;
		}

		default:
			Panic("Token \"{}\" can not be parse as a unary expression.", token.type);
	}
	return expression;
}

Expression* Parser::ParseBinaryExpression(Expression *left)
{
	const auto &token = lexer_->PeekNextToken();
	lexer_->EatToken();

	auto binary_operator = arena_->PushItem<BinaryOperator>();
	binary_operator->op = token.type;
	binary_operator->left = left;
	binary_operator->right = ParseExpression(TokenTypeToPrecedense(token.type));
	return binary_operator;
}

Expression* Parser::ParseExpression(Precedence precedence)
{
	Expression *left = ParseUnaryExpression();

	while (precedence < TokenTypeToPrecedense(lexer_->PeekNextToken().type)) 
	{
		left = ParseBinaryExpression(left);
	}

	return left;
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

std::string Ast::NodeToString(const Node *node, int tabs) 
{
	auto Indent = [](int tabs)
	{
		constexpr auto tab_width = 4;
		return std::string(tab_width * tabs, ' ');
	};

	std::string result;
	switch (node->type)
	{
		using enum Ast::NodeType;

		case statement_variable_declaration:
		{
			auto decl = reinterpret_cast<const VariableDeclarationStatement*>(node);
			result = std::format("{}: {}", decl->identifier, decl->type_identifier);
			if (decl->value)
			{
				result += std::format(" = {}", NodeToString(*(decl->value)));
			}
			result += ";";
			break;
		}
		case statement_if:
		{
			auto if_statement = reinterpret_cast<const IfStatement*>(node);
			result = std::format(
				"if ({}) {}", 
				NodeToString(if_statement->condition),
				NodeToString(if_statement->true_branch, tabs));
			if (if_statement->false_branch) 
			{
				result += std::format(
					"else {}", 
					NodeToString(*(if_statement->false_branch), tabs));
			}
			break;
		}
		case statement_while:
		{
			auto while_statement = reinterpret_cast<const WhileStatement*>(node);
			result = std::format(
				"while ({}) {}",
				NodeToString(while_statement->condition),
				NodeToString(while_statement->body, tabs));
			break;
		}
		case statement_assingment:
		{
			auto assingment = reinterpret_cast<const AssignmentStatement*>(node);
			result = std::format(
				"{} = {};", 
				assingment->identifier, 
				NodeToString(assingment->value));
			break;
		}
		case statement_block:
		{
			auto block = reinterpret_cast<const BlockStatement*>(node);
			result = "{\n";
			for (int i = 0; i < block->statements.size(); ++i) 
			{
				const auto statement = block->statements[i];
				result += std::format("{}{}\n", Indent(tabs + 1), NodeToString(statement, tabs + 1));
			}
			result += Indent(tabs);
			result += "} ";
			break;
		}
		case statement_expression:
		{
			auto statement = reinterpret_cast<const ExpressionStatement*>(node);
			result = std::format("{};", NodeToString(statement->expression));
			break;
		}

		case expression_integer_literal:
		{
			auto integer = reinterpret_cast<const IntegerLiteral*>(node);
			result = std::format("{}", integer->value);
			break;
		}
		case expression_bool_literal:
		{
			auto boolean = reinterpret_cast<const BoolLiteral *>(node);
			result = std::format("{}", boolean->value);
			break;
		};
		case expression_identifier:
		{
			auto ident = reinterpret_cast<const Identifier*>(node);
			result = ident->name;
			break;
		};
		case expression_unary_operator:
		{
			auto unary_operator = reinterpret_cast<const UnaryOperator*>(node);
			result = std::format("({}{})", unary_operator->op, NodeToString(unary_operator->right));
			break;
		};
		case expression_binary_operator:
		{
			auto binary_operator = reinterpret_cast<const BinaryOperator*>(node);
			result = std::format(
				"({} {} {})", 
				NodeToString(binary_operator->left),
				binary_operator->op,
				NodeToString(binary_operator->right));
			break;
		};

		case invalid:
		{
			result = "invalid;";
			break;
		}

		default:
		{
			result = "unknown;";
			break;
		}
	}

	return result;
}