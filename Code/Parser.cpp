#include <print>
#include <vector>

#include "Base.h"

#include "Parser.h"
#include "Lexer.h"

using namespace Ast;

static bool IsStatement(Node* node)
{
	if (node->type >= NodeType::statement_if && node->type <= NodeType::statement_expression)
	{
		return true;
	}
	return false;
};

static bool IsDeclaration(Node* node)
{
	if (node->type >= NodeType::declaration_variable && node->type <= NodeType::declaration_type)
	{
		return true;
	}
	return false;
};

Parser::Parser(std::string_view input, Arena *arena, FILE *log) : lexer_{input}, arena_{arena}, log_{log} {}

Program Parser::ParseProgram()
{
	Program result;
	while (lexer_.PeekNextToken().type != TokenType::invalid) 
	{
		auto statement = ParseDeclaration();
		result.declarations.push_back(statement);
	}
	result.error_count = error_count_;
	return result;
}

Statement* Parser::ParseStatement()
{
	const auto token = lexer_.PeekNextToken();
	auto node = ParseStatementOrDeclaration();
	if (!IsStatement(node))
	{
		LogError(token, "Expected statement.");
		node->type = NodeType::invalid;
	}
	return reinterpret_cast<Statement*>(node);
};

Declaration* Parser::ParseDeclaration()
{
	const auto token = lexer_.PeekNextToken();
	auto node = ParseStatementOrDeclaration();
	if (!IsDeclaration(node))
	{
		LogError(token, "Expected declaration.");
		node->type = NodeType::invalid;
	}
	return reinterpret_cast<Declaration*>(node);
};

Node* Parser::ParseStatementOrDeclaration() 
{
	const auto token_type = lexer_.PeekNextToken().type;
	switch (token_type)
	{
		using enum TokenType;

		case identifier: {
			const auto next_token_type = lexer_.PeekToken(1).type;
			if (next_token_type == static_cast<TokenType>(':')) 
			{
				return ParseVariableDeclaration();
			}
			if (next_token_type == static_cast<TokenType>('=')) 
			{
				return ParseAssignmentStatement();
			}
			break;
		};

		case keyword_proc:   return ParseProcedureDeclaration();
		case keyword_const:  return ParseConstantDeclaration();
		case keyword_type:   return ParseTypeDeclaration();
		case keyword_if:     return ParseIfStatement();
		case keyword_while:  return ParseWhileStatement();
		case TokenType{'{'}: return ParseBlockStatement();     
	}
	return ParseExpressionStatement();
}

ConstDeclaration* Parser::ParseConstantDeclaration()
{
	auto declaration = New<ConstDeclaration>();
	ExpectToken(TokenType::keyword_const);
	declaration->identifier = ExpectToken(TokenType::identifier);
	ExpectToken(TokenType{':'});
	declaration->variable_type = ParseType();
	ExpectToken(TokenType{'='});
	declaration->value = ParseExpression();
	ExpectToken(TokenType{';'});
	return declaration;
}

VariableDeclaration* Parser::ParseVariableDeclaration() 
{
	auto statement = New<VariableDeclaration>();
	statement->identifier = ExpectToken(TokenType::identifier);
	ExpectToken(TokenType{':'});
	statement->variable_type = ParseType();
	
	if (NextTokenIs(TokenType{'='}))
	{
		lexer_.EatToken();
		statement->value = ParseExpression();
	}
	
	ExpectToken(TokenType{';'});
	return statement;
}

TypeDeclaration* Parser::ParseTypeDeclaration()
{
	auto declaration = New<TypeDeclaration>();
	ExpectToken(TokenType::keyword_type);
	declaration->identifier = ExpectToken(TokenType::identifier);
	ExpectToken(TokenType{'='});
	declaration->declared_type = ParseType();
	ExpectToken(TokenType{';'});
	return declaration;
}

ProcedureDeclaration* Parser::ParseProcedureDeclaration()
{
	Panic("TODO");
}

ExpressionStatement* Parser::ParseExpressionStatement()
{
	auto statement = New<ExpressionStatement>();
	statement->expression = ParseExpression();
	ExpectToken(TokenType{';'});
	return statement;
}

IfStatement* Parser::ParseIfStatement()
{
	auto statement = New<IfStatement>();
	ExpectToken(TokenType::keyword_if);
	statement->condition = ParseExpression();
	statement->true_branch = ParseStatement();
	if (!NextTokenIs(TokenType::keyword_else))
	{
		return statement;
	}

	lexer_.EatToken();
	statement->false_branch = ParseStatement();
	return statement;
}

WhileStatement* Parser::ParseWhileStatement()
{
	auto statement = New<WhileStatement>();
	ExpectToken(TokenType::keyword_while);
	statement->condition = ParseExpression();
	statement->body = ParseStatement();
	return statement;
}

Type* Parser::ParseType()
{
	const auto token_type = lexer_.PeekNextToken().type;
	switch (token_type)
	{
		using enum TokenType;

		case identifier:
		{
			auto ident = New<TypeIdentifier>();
			ident->identifier = ExpectToken(TokenType::identifier);
			return ident;
		}
	}

	Panic("TODO");
}

AssignmentStatement* Parser::ParseAssignmentStatement()
{
	auto statement = New<AssignmentStatement>();
	statement->identifier = ExpectToken(TokenType::identifier);
	ExpectToken(TokenType{'='});
	statement->value = ParseExpression();
	ExpectToken(TokenType{';'});
	return statement;
}

BlockStatement* Parser::ParseBlockStatement()
{
	auto block = New<BlockStatement>();
	ExpectToken(TokenType{'{'});

	// TODO: How many statements can we expect on average? Reserve that amount.
	std::vector<Node*> temp;
	while (!(NextTokenIs(TokenType{'}'}) || NextTokenIs(TokenType::invalid))) 
	{
		const auto node = ParseStatementOrDeclaration();
		temp.push_back(node);
	}
	lexer_.EatToken();

	block->body = arena_->PushArray<Node*>(temp.size());
	std::ranges::copy(temp, block->body.begin());
	return block;
}

// Returns precedence of a binary operator.
static Precedence TokenTypeToPrecedense(TokenType type)
{
	switch (type) 
	{
	using enum TokenType;
	case equals:
	case not_equals:
		return Precedence::equals;

	case TokenType{'<'}:
	case TokenType{'>'}:
	case less_equals:
	case greater_equals:
		return Precedence::comparison;

	case TokenType{'+'}:
	case TokenType{'-'}:
		return Precedence::plus;

	case TokenType{'*'}:
	case TokenType{'/'}:
	case TokenType{'%'}:
		return Precedence::multiply;

	default:
		return Precedence::lowest;
	}
}

Expression* Parser::ParseUnaryExpression()
{
	Expression *expression = nullptr;
	const auto& token = lexer_.PeekNextToken();
	lexer_.EatToken();
	switch (token.type)
	{
		using enum TokenType;

		case identifier:
		{
			auto ident = New<IdentifierExpression>();
			ident->identifier = token;
			expression = ident;
			break;
		}

		case integer:
		{
			auto integer_literal = New<IntegerLiteral>();
			integer_literal->value = token;
			expression = integer_literal;
			break;
		}

		case keyword_true:
		case keyword_false:
		{
			auto bool_literal = New<BoolLiteral>();
			bool_literal->value = (token.type == keyword_true);
			expression = bool_literal;
			break;
		}

		case TokenType{'-'}:
		case TokenType{'!'}:
		{
			auto unary_operator = New<UnaryOperator>();
			unary_operator->op = token.type;
			unary_operator->right = ParseExpression(Precedence::prefix);
			expression = unary_operator;
			break;
		}

		default:
		{
			expression = New<Expression>(NodeType::invalid);
			LogError(token, "Token \"{}\" can not be parsed as a unary expression.", token.type);
			break;
		}
	}
	return expression;
}

Expression* Parser::ParseBinaryExpression(Expression *left)
{
	const auto &token = lexer_.PeekNextToken();
	lexer_.EatToken();
	auto binary_operator = New<BinaryOperator>();
	binary_operator->op = token.type;
	binary_operator->left = left;
	binary_operator->right = ParseExpression(TokenTypeToPrecedense(token.type));
	return binary_operator;
}

Expression* Parser::ParseExpression(Precedence precedence)
{
	Expression *left = ParseUnaryExpression();

	while (precedence < TokenTypeToPrecedense(lexer_.PeekNextToken().type)) 
	{
		left = ParseBinaryExpression(left);
	}

	return left;
}

const Token& Parser::ExpectToken(TokenType type) 
{
	const auto& token = lexer_.PeekNextToken();
	lexer_.EatToken();
	if (token.type != type) 
	{
		LogError(token, "Expected {}, got {}.", type, token.type);
		error_count_ += 1;
	}
	return token;
}

bool Parser::NextTokenIs(TokenType type) 
{
	return lexer_.PeekNextToken().type == type;
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

		case declaration_type:
		{
			auto decl = reinterpret_cast<const TypeDeclaration*>(node);
			Assert(decl->declared_type->type == type_identifier);
			auto type_ident = reinterpret_cast<const TypeIdentifier *>(decl->declared_type);
			result = std::format("type {} = {};", decl->identifier.identifier, type_ident->identifier.identifier);
			break;
		}

		case declaration_const:
		{
			auto decl = reinterpret_cast<const ConstDeclaration*>(node);
			Assert(decl->variable_type->type == type_identifier);
			auto type_ident = reinterpret_cast<const TypeIdentifier*>(decl->variable_type);
			result = std::format(
				"const {}: {} = {};", 
				decl->identifier.identifier, 
				type_ident->identifier.identifier,
				NodeToString(decl->value));
			break;
		}

		case declaration_variable:
		{
			auto decl = reinterpret_cast<const VariableDeclaration*>(node);
			auto type_identifier = reinterpret_cast<const TypeIdentifier*>(decl->variable_type);
			result = std::format("{}: {}", decl->identifier.identifier, type_identifier->identifier.identifier);
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
				assingment->identifier.identifier, 
				NodeToString(assingment->value));
			break;
		}
		case statement_block:
		{
			auto block = reinterpret_cast<const BlockStatement*>(node);
			result = "{\n";
			for (int i = 0; i < block->body.size(); ++i)
			{
				const auto statement = block->body[i];
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
			result = std::format("{}", integer->value.integer_value);
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
			auto ident = reinterpret_cast<const IdentifierExpression*>(node);
			result = ident->identifier.identifier;
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