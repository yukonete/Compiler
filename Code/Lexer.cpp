#include <cctype>
#include <string_view>

#include "Base.h"
#include "Lexer.h"

static TokenType CheckIdentifierForKeyword(std::string_view identifier) 
{
	if (Lexer::keywords.contains(identifier)) 
	{
		return Lexer::keywords.at(identifier);
	}
	return TokenType::identifier;
}

Lexer::Lexer(std::string_view input) : input_{ input } {}

const Token& Lexer::PeekNextToken() 
{
    if (tokens_cursor_ < tokens_.size()) 
	{
        return tokens_.at(tokens_cursor_);
    }

    Tokenize();
    return PeekNextToken();
}

const Token& Lexer::PeekToken(int peek) 
{
    if (tokens_cursor_ + peek < tokens_.size()) 
	{
        return tokens_.at(tokens_cursor_ + peek);
    }

    Tokenize();
    return PeekToken(peek);
}

const Token& Lexer::PreviousToken() const 
{
    if (tokens_cursor_ - 1 >= 0) 
	{
        return tokens_.at(tokens_cursor_ - 1);
    }

    Panic("No previous token");
}

void Lexer::EatToken() 
{
    tokens_cursor_ += 1;
}

void Lexer::UneatToken() 
{
	if (tokens_cursor_ > 0) 
	{
		tokens_cursor_ -= 1;
	} 
	else 
	{
		Panic("No previous token");
	}
}

void Lexer::Tokenize() 
{
	SkipWhitespaces();
	auto ch = PeekNextChar();

	Token token;
	token.start = current_location_;

	switch (ch) 
	{
		case '=': 
		{
			if (PeekChar(1) == '=') 
			{
				token.type = TokenType::equals;
				EatChar();
			}
			else 
			{
				token.type = static_cast<TokenType>(ch);
			}
			EatChar();
			break;
		}
		case '!': 
		{
			if (PeekChar(1) == '=') 
			{
				token.type = TokenType::not_equals;
				EatChar();
			}
			else 
			{
				token.type = static_cast<TokenType>(ch);
			}
			EatChar();
			break;
		}

		case '+': 
		{
			if (PeekChar(1) == '=') 
			{
				token.type = TokenType::plus_assign;
				EatChar();
			}
			else 
			{
				token.type = static_cast<TokenType>(ch);
			}
			EatChar();
			break;
		}
		case '-': 
		{
			if (PeekChar(1) == '=') 
			{
				token.type = TokenType::minus_assign;
				EatChar();
			}
			else if (PeekChar(1) == '>') 
			{
				token.type = TokenType::return_arrow;
				EatChar();
			}
			else 
			{
				token.type = static_cast<TokenType>(ch);
			}
			EatChar();
			break;
		}
		case '*': 
		{
			if (PeekChar(1) == '=') 
			{
				token.type = TokenType::multiply_assign;
				EatChar();
			}
			else 
			{
				token.type = static_cast<TokenType>(ch);
			}
			EatChar();
			break;
		}
		case '/': 
		{
			if (PeekChar(1) == '=') 
			{
				token.type = TokenType::divide_assign;
				EatChar();
			}
			else 
			{
				token.type = static_cast<TokenType>(ch);
			}
			EatChar();
			break;
		}
		case '%': 
		{
			if (PeekChar(1) == '=') 
			{
				token.type = TokenType::modulo_assign;
				EatChar();
			}
			else 
			{
				token.type = static_cast<TokenType>(ch);
			}
			EatChar();
			break;
		}

		case '<': 
		{
			if (PeekChar(1) == '=') 
			{
				token.type = TokenType::less_equals;
				EatChar();
			}
			else 
			{
				token.type = static_cast<TokenType>(ch);
			}
			EatChar();
			break;
		}
		case '>': 
		{
			if (PeekChar(1) == '=') 
			{
				token.type = TokenType::greater_equals;
				EatChar();
			}
			else 
			{
				token.type = static_cast<TokenType>(ch);
			}
			EatChar();
			break;
		}

		case '{':
		case '}':
		case '(':
		case ')':
		case '[':
		case ']':
		case ';':
		case ':':
		case ',': 
		{
			token.type = static_cast<TokenType>(ch);
			EatChar();
			break;
		}

		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9': 
		{
			token.integer_value = ParseInteger();
			token.type = TokenType::integer;
			break;
		}

		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
		case 'g':
		case 'h':
		case 'i':
		case 'j':
		case 'k':
		case 'l':
		case 'm':
		case 'n':
		case 'o':
		case 'p':
		case 'q':
		case 'r':
		case 's':
		case 't':
		case 'u':
		case 'v':
		case 'w':
		case 'x':
		case 'y':
		case 'z':
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':
		case 'G':
		case 'H':
		case 'I':
		case 'J':
		case 'K':
		case 'L':
		case 'M':
		case 'N':
		case 'O':
		case 'P':
		case 'Q':
		case 'R':
		case 'S':
		case 'T':
		case 'U':
		case 'V':
		case 'W':
		case 'X':
		case 'Y':
		case 'Z':
		case '_': 
		{
			const auto identifier = ParseIdentifier();
			token.identifier = identifier;
			token.type = CheckIdentifierForKeyword(identifier);
			break;
		}

		default: 
		{
			EatChar();
			break;
		}
	}
	
	token.end = current_location_;
	token.end.column -= 1;
	Assert(token.end.column != 0);
	tokens_.push_back(token);
}

int Lexer::PeekNextChar() const 
{
	if (input_cursor_ >= input_.length()) 
	{
		return -1;
	}
	return input_.at(input_cursor_);
}

int Lexer::PeekChar(int peek) const 
{
	if (peek < 0) 
	{
		return -1;
	}
	if (peek == 0) 
	{
		return PeekNextChar();
	}

	auto cursor = input_cursor_ + peek;
	if (cursor >= input_.length()) 
	{
		return -1;
	}

	return input_.at(cursor);
}

void Lexer::EatChar() {
	const auto ch = PeekNextChar();
	if (ch == '\n' || (ch == '\r' && PeekChar(1) == '\n'))
	{
		current_location_.line += 1;
		current_location_.column = 1;
		if (ch == '\r') 
		{
			input_cursor_ += 1;
		}
	}
	else 
	{
		current_location_.column += 1;
	}
	input_cursor_ += 1;
}

s64 Lexer::ParseInteger() 
{
	s64 result = 0;
	auto ch = PeekNextChar();
	while (std::isdigit(ch)) 
	{
		result *= 10;
		result += ch - '0';
		EatChar();
		ch = PeekNextChar();
	}
	return result;
}

std::string_view Lexer::ParseIdentifier() 
{
	u64 identifier_start = input_cursor_;
	u64 count = 0;
	auto ch = PeekNextChar();
	while (std::isalnum(ch) || ch == '_') 
	{
		count += 1;
		EatChar();
		ch = PeekNextChar();
	}

	return input_.substr(identifier_start, count);
}

void Lexer::SkipWhitespaces() 
{
	while (true) 
	{
		const auto ch = PeekNextChar();
		
		if (ch == ' ' || ch == '\t' || ch == '\n' || (ch == '\r' && PeekChar(1) == '\n') ) 
		{
			EatChar();
			continue;
		}

		break;
	}
}

std::string_view TokenTypeToString(TokenType type) 
{
	auto type_int = static_cast<int>(type);
	if (type_int > 0 && type_int < 256) 
	{
		return {};
	}

    switch (type) {
		using enum TokenType;
        case plus_assign: return "+=";
        case minus_assign: return "-=";
        case multiply_assign: return "*="; 
        case divide_assign: return "/=";
        case modulo_assign: return "%=";
        case return_arrow: return "->";
		case less_equals: return "<=";
        case identifier: return "identifier";
        case integer: return "integer";
        case invalid: return "invalid";
        case keyword_if: return "if";
        case keyword_else: return "else";
        case keyword_while: return "while";
        case keyword_return: return "return";
        case keyword_proc: return "proc";
		case keyword_const: return "const";
		case keyword_struct: return "struct";
        default: return "unknown";
    }
}
