#pragma once

#include <algorithm>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "base.h"

enum class TokenType {
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
	keyword_transmute,
	keyword_type,
	keyword_const,
	keyword_struct,

	invalid,
};

struct FileLocation {
	u64 line = 0;
	u64 column = 0;
};

struct Token {
	TokenType type = TokenType::invalid;
	FileLocation start;
	FileLocation end;

	union {
		std::string_view identifier = {};
		s64 integer_value;
	};
};

std::string_view token_type_to_string(TokenType type);

class Lexer {
public:
	static const inline std::unordered_map<std::string_view, TokenType> keywords = {
		{ "if",        TokenType::keyword_if        },
		{ "else",      TokenType::keyword_else      },
		{ "while",     TokenType::keyword_while     },
		{ "return",    TokenType::keyword_return    },
		{ "proc",      TokenType::keyword_proc      },
		{ "true",      TokenType::keyword_true      },
		{ "false",     TokenType::keyword_false     },
		{ "cast",      TokenType::keyword_cast      },
		{ "transmute", TokenType::keyword_transmute },
		{ "type",      TokenType::keyword_type      },
		{ "const",     TokenType::keyword_const     },
		{ "struct",    TokenType::keyword_struct    },
	};

	Lexer(std::string_view input);
	Lexer(const Lexer&) = delete;
	Lexer(const Lexer &&) = delete;

	// Might invalidate references to tokens
	const Token& next_token();
	// Might invalidate references to tokens
	const Token& peek_token(int peek);

	const Token& previous_token() const;
	void eat_token();
	void uneat_token();

private:
	void tokenize();
	int peek_next_char() const;
	int peek_char(int peek) const;
	void eat_char();
	s64 parse_integer();
	std::string_view parse_identifier();
	void skip_whitespaces();

	std::string_view input_;
	u64 input_cursor_ = 0;

	std::vector<Token> tokens_;
	u64 tokens_cursor_ = 0;

	FileLocation current_location_ = {1, 1};
};

template <>
struct std::formatter<TokenType> {
	template<class ParseContext>
	constexpr ParseContext::iterator parse(ParseContext &ctx) {
		return ctx.begin();
	}

	template<class FmtContext>
	FmtContext::iterator format(TokenType type, FmtContext &ctx) const {
		auto out = ctx.out();
		auto type_string = token_type_to_string(type);
		if (type_string.empty()) 
		{
			*out = static_cast<char>(type);
			out++;
			return out;
		}

		return std::ranges::copy(type_string, out).out;
	}
};