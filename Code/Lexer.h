#pragma once

#include <unordered_map>
#include <string>
#include <ctype.h>
#include <map>
#include <vector>
#include <variant>
#include <stdint.h>

#include "Base.h"

namespace Lex 
{

struct Identifier 
{
	std::string_view name;
};

struct Invalid {};

struct Integer 
{
	s64 value = 0;
};

enum class TokenType 
{
	// ASCII characters here

	identifier = 256,
	integer,

	plus_assign,
	minus_assign,
	multiply_assign,
	divide_assign,
	modulo_assign,

	equals,
	not_equals,

	less_equals,
	greater_equals,

	return_arrow,

	keyword_if,
	keyword_else,
	keyword_while,
	keyword_return,
	keyword_proc,
	keyword_true,
	keyword_false,
	keyword_cast,

	invalid,
};

using TokenData = std::variant<Invalid, Identifier, Integer>;

struct FileLocation 
{
	u64 line = 0;
	u64 column = 0;
};

struct Token 
{
	TokenType type = TokenType::invalid;
	TokenData data = Invalid{};
	FileLocation start;
	FileLocation end;

	std::string_view Identifier() const;
	s64 Integer() const;
};

std::string_view TokenTypeToString(TokenType type);

class Lexer 
{
public:
	static const inline std::unordered_map<std::string_view, TokenType> keywords = 
	{
		{ "if",     TokenType::keyword_if },
		{ "else",   TokenType::keyword_else },
		{ "while",  TokenType::keyword_while },
		{ "return", TokenType::keyword_return },
		{ "proc",   TokenType::keyword_proc },
		{ "true",   TokenType::keyword_true },
		{ "false",  TokenType::keyword_false },
		{ "cast",   TokenType::keyword_cast },
	};

	Lexer(std::string_view input);
	Lexer(const Lexer&) = delete;
	Lexer(const Lexer &&) = delete;

	// Call to any of this 2 functions might invalidate references to tokens
	const Token& PeekNextToken();
	const Token& PeekToken(int peek);

	const Token& PreviousToken() const;
	void EatToken();

private:
	void Tokenize();
	int PeekNextChar() const;
	int PeekChar(int peek) const;
	void EatChar();
	Integer ParseInteger();
	Identifier ParseIdentifier();
	void SkipWhitespaces();

	std::string_view input_;
	u64 input_cursor_ = 0;

	std::vector<Token> tokens_;
	u64 tokens_cursor_ = 0;

	FileLocation current_location_ = {1, 1};
};

}