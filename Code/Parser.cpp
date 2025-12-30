#include <algorithm>
#include <cstdio>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include "Base.h"
#include "Lexer.h"
#include "Parser.h"

using namespace Ast;

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
	while (true) 
	{
		const auto token = lexer_.PeekNextToken();
		if (lexer_.PeekNextToken().type == TokenType::invalid)
		{
			break;
		}

		auto statement = ParseStatement();
		if (!IsDeclaration(statement))
		{
			LogError(token, "Expected declaration.");
			statement->type = NodeType::invalid;
		}
		result.declarations.push_back(reinterpret_cast<Declaration*>(statement));
	}
	result.error_count = error_count_;
	return result;
}

Statement* Parser::ParseStatement() 
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
		case keyword_return: return ParseReturnStatement();
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
	auto proc = New<ProcedureDeclaration>();
	ExpectToken(TokenType::keyword_proc);
	proc->identifier = ExpectToken(TokenType::identifier);
	ExpectToken(TokenType{'('});
	std::vector<ProcedureParameter*> temp;
	bool first_parameter = true;
	while (!(NextTokenIs(TokenType{')'}) || NextTokenIs(TokenType::invalid)))
	{
		auto parameter = New<ProcedureParameter>();
		if (!first_parameter)
		{
			ExpectToken(TokenType{','});
		}
		else
		{
			first_parameter = false;
		}
		parameter->identifier = ExpectToken(TokenType::identifier);
		ExpectToken(TokenType{':'});
		parameter->type = ParseType();
		temp.push_back(parameter);
	}
	ExpectToken(TokenType{')'});
	proc->parameters = arena_->PushArray<ProcedureParameter*>(temp.size());
	std::ranges::copy(temp, proc->parameters.begin());
	ExpectToken(TokenType::return_arrow);
	proc->return_type = ParseType();
	proc->body = ParseBlockStatement();
	return proc;
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
	Type *type = nullptr;
	const auto &token = lexer_.PeekNextToken();
	lexer_.EatToken();
	switch (token.type)
	{
		using enum TokenType;

		case identifier:
		{
			auto ident = New<TypeIdentifier>();
			ident->identifier = token;
			type = ident;
			break;
		}
        case TokenType{'*'}:
		{
			auto pointer = New<TypePointer>();
			pointer->points_to = ParseType();
			type = pointer;
			break;
		}
        case keyword_struct:
		{
			auto st = New<TypeStruct>();
            ExpectToken(TokenType{'{'});
			std::vector<StructMember*> temp;
			temp.reserve(16);
			while (!(NextTokenIs(TokenType{'}'}) || NextTokenIs(TokenType::invalid)))
			{
				auto member = New<StructMember>();
				member->identifier = ExpectToken(TokenType::identifier);
				ExpectToken(TokenType{':'});
				member->type = ParseType();
				ExpectToken(TokenType{';'});
				temp.push_back(member);
			}
            ExpectToken(TokenType{'}'});
			st->members = arena_->PushArray<StructMember*>(temp.size());
			std::ranges::copy(temp, st->members.begin());
			type = st;
			break;
		}
        default:
		{
            type = New<Type>(NodeType::invalid);
            LogError(token, "Token \"{}\" can not be parsed as a type.", token.type);
            break;
        }
	}
	return type;
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

ReturnStatement* Parser::ParseReturnStatement()
{
	auto ret = New<ReturnStatement>();
	ExpectToken(TokenType::keyword_return);
	if (!NextTokenIs(TokenType{';'}))
	{
		ret->value = ParseExpression();
	}
	ExpectToken(TokenType{';'});
	return ret;
}

BlockStatement* Parser::ParseBlockStatement()
{
	auto block = New<BlockStatement>();
	ExpectToken(TokenType{'{'});
	std::vector<Statement*> temp;
	temp.reserve(16);
	while (!(NextTokenIs(TokenType{'}'}) || NextTokenIs(TokenType::invalid))) 
	{
		const auto statement = ParseStatement();
		temp.push_back(statement);
	}
    ExpectToken(TokenType{'}'});
	block->body = arena_->PushArray<Statement*>(temp.size());
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

		case declaration_procedure:
		{
			auto proc = reinterpret_cast<const ProcedureDeclaration*>(node);
			result = std::format("proc {}(", proc->identifier.identifier);
			for (int i = 0; i < proc->parameters.size(); ++i) {
				auto parameter = proc->parameters[i];
				result += std::format(
					"{}: {}",
					parameter->identifier.identifier,
					NodeToString(parameter->type, tabs));
				if (i != proc->parameters.size() - 1)
				{
					result += ", ";
				}
			}
			result += std::format(
				") -> {} {}",
				NodeToString(proc->return_type, tabs),
				NodeToString(proc->body, tabs));
			break;
		}

		case declaration_type:
		{
			auto decl = reinterpret_cast<const TypeDeclaration*>(node);
            result = std::format(
				"type {} = {};",
				decl->identifier.identifier,
				NodeToString(decl->declared_type, tabs));
			break;
		}

		case type_identifier:
		{
			auto type_ident = reinterpret_cast<const TypeIdentifier *>(node);
			result = type_ident->identifier.identifier;
			break;
		}

		case type_struct:
		{
			auto st = reinterpret_cast<const TypeStruct*>(node);
			result = "struct {\n";
			for (auto member : st->members)
			{
				result += std::format("{}{}: {};\n", Indent(tabs + 1), member->identifier.identifier, NodeToString(member->type, tabs + 1));
			}
			result += Indent(tabs);
			result += "}";
			break;
		}

		case type_pointer:
		{
			auto type_pointer = reinterpret_cast<const TypePointer *>(node);
			result = std::format("*{}", NodeToString(type_pointer->points_to, tabs));
			break;
		}

		case declaration_const:
		{
			auto decl = reinterpret_cast<const ConstDeclaration*>(node);
			result = std::format(
				"const {}: {} = {};", 
				decl->identifier.identifier, 
				NodeToString(decl->variable_type, tabs),
				NodeToString(decl->value, tabs));
			break;
		}

		case declaration_variable:
		{
			auto decl = reinterpret_cast<const VariableDeclaration*>(node);
			result = std::format("{}: {}", decl->identifier.identifier, NodeToString(decl->variable_type, tabs));
			if (decl->value)
			{
				result += std::format(" = {}", NodeToString(decl->value.value(), tabs));
			}
			result += ";";
			break;
		}
		case statement_return:
		{
			auto ret = reinterpret_cast<const ReturnStatement*>(node);
			result = std::format("return {};", NodeToString(ret->value, tabs));
			break;
		}
		case statement_if:
		{
			auto if_statement = reinterpret_cast<const IfStatement*>(node);
			result = std::format(
				"if {} {}", 
				NodeToString(if_statement->condition, tabs),
				NodeToString(if_statement->true_branch, tabs));
			if (if_statement->false_branch) 
			{
				result += std::format(
					" else {}", 
					NodeToString(*(if_statement->false_branch), tabs));
			}
			break;
		}
		case statement_while:
		{
			auto while_statement = reinterpret_cast<const WhileStatement*>(node);
			result = std::format(
				"while {} {}",
				NodeToString(while_statement->condition, tabs),
				NodeToString(while_statement->body, tabs));
			break;
		}
		case statement_assingment:
		{
			auto assingment = reinterpret_cast<const AssignmentStatement*>(node);
			result = std::format(
				"{} = {};", 
				assingment->identifier.identifier, 
				NodeToString(assingment->value, tabs));
			break;
		}
		case statement_block:
		{
			auto block = reinterpret_cast<const BlockStatement*>(node);
			result = "{\n";
			for (auto statement : block->body)
			{
				result += std::format("{}{}\n", Indent(tabs + 1), NodeToString(statement, tabs + 1));
			}
			result += Indent(tabs);
			result += "}";
			break;
		}
		case statement_expression:
		{
			auto statement = reinterpret_cast<const ExpressionStatement*>(node);
			result = std::format("{};", NodeToString(statement->expression, tabs));
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
			result = std::format("({}{})", unary_operator->op, NodeToString(unary_operator->right, tabs));
			break;
		};
		case expression_binary_operator:
		{
			auto binary_operator = reinterpret_cast<const BinaryOperator*>(node);
			result = std::format(
				"({} {} {})", 
				NodeToString(binary_operator->left, tabs),
				binary_operator->op,
				NodeToString(binary_operator->right, tabs));
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