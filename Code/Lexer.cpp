#include <array>
#include <cctype>
#include <string_view>

#include "base.h"
#include "lexer.h"

static TokenType check_identifier_for_keyword(std::string_view identifier) {
    if (Lexer::keywords.contains(identifier)) {
        return Lexer::keywords.at(identifier);
    }
    return TokenType::identifier;
}

Lexer::Lexer(std::string_view input) : input_{input} {
}

const Token &Lexer::next_token() {
    return peek_token(0);
}

// peek is how much to look ahead
const Token &Lexer::peek_token(int peek) {
    if (tokens_cursor_ + peek < tokens_.size()) {
        return tokens_.at(tokens_cursor_ + peek);
    }

    tokenize();
    return peek_token(peek);
}

const Token &Lexer::previous_token() const {
    if (tokens_cursor_ - 1 >= 0) {
        return tokens_.at(tokens_cursor_ - 1);
    }

    panic("No previous token");
}

void Lexer::eat_token() {
    tokens_cursor_ += 1;
}

void Lexer::uneat_token() {
    if (tokens_cursor_ > 0) {
        tokens_cursor_ -= 1;
    } else {
        panic("No previous token");
    }
}

void Lexer::tokenize() {
    skip_whitespaces();
    auto ch = peek_next_char();

    Token token;
    token.start = current_location_;

    switch (ch) {
        case '=': {
            if (peek_char(1) == '=') {
                token.type = TokenType::equals;
                eat_char();
            } else {
                token.type = static_cast<TokenType>(ch);
            }
            eat_char();
            break;
        }
        case '!': {
            if (peek_char(1) == '=') {
                token.type = TokenType::not_equals;
                eat_char();
            } else {
                token.type = static_cast<TokenType>(ch);
            }
            eat_char();
            break;
        }

        case '+': {
            if (peek_char(1) == '=') {
                token.type = TokenType::plus_assign;
                eat_char();
            } else {
                token.type = static_cast<TokenType>(ch);
            }
            eat_char();
            break;
        }
        case '-': {
            if (peek_char(1) == '=') {
                token.type = TokenType::minus_assign;
                eat_char();
            } else if (peek_char(1) == '>') {
                token.type = TokenType::return_arrow;
                eat_char();
            } else {
                token.type = static_cast<TokenType>(ch);
            }
            eat_char();
            break;
        }
        case '*': {
            if (peek_char(1) == '=') {
                token.type = TokenType::multiply_assign;
                eat_char();
            } else {
                token.type = static_cast<TokenType>(ch);
            }
            eat_char();
            break;
        }
        case '/': {
            if (peek_char(1) == '=') {
                token.type = TokenType::divide_assign;
                eat_char();
            } else {
                token.type = static_cast<TokenType>(ch);
            }
            eat_char();
            break;
        }
        case '%': {
            if (peek_char(1) == '=') {
                token.type = TokenType::modulo_assign;
                eat_char();
            } else {
                token.type = static_cast<TokenType>(ch);
            }
            eat_char();
            break;
        }

        case '<': {
            if (peek_char(1) == '=') {
                token.type = TokenType::less_equals;
                eat_char();
            } else {
                token.type = static_cast<TokenType>(ch);
            }
            eat_char();
            break;
        }
        case '>': {
            if (peek_char(1) == '=') {
                token.type = TokenType::greater_equals;
                eat_char();
            } else {
                token.type = static_cast<TokenType>(ch);
            }
            eat_char();
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
        case '&': {
            token.type = static_cast<TokenType>(ch);
            eat_char();
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
        case '9': {
            token.integer_value = parse_integer();
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
        case '_': {
            const auto identifier = parse_identifier();
            token.identifier = identifier;
            token.type = check_identifier_for_keyword(identifier);
            break;
        }

        default: {
            eat_char();
            break;
        }
    }

    token.end = current_location_;
    token.end.column -= 1;
    Assert(token.end.column != 0);
    tokens_.push_back(token);
}

int Lexer::peek_next_char() const {
    if (input_cursor_ >= input_.length()) {
        return -1;
    }
    return input_.at(input_cursor_);
}

int Lexer::peek_char(int peek) const {
    if (peek < 0) {
        return -1;
    }
    if (peek == 0) {
        return peek_next_char();
    }

    auto cursor = input_cursor_ + peek;
    if (cursor >= input_.length()) {
        return -1;
    }

    return input_.at(cursor);
}

void Lexer::eat_char() {
    const auto ch = peek_next_char();
    if (ch == '\n' || (ch == '\r' && peek_char(1) == '\n')) {
        current_location_.line += 1;
        current_location_.column = 1;
        if (ch == '\r') {
            input_cursor_ += 1;
        }
    } else {
        current_location_.column += 1;
    }
    input_cursor_ += 1;
}

s64 Lexer::parse_integer() {
    s64 result = 0;
    auto ch = peek_next_char();
    while (std::isdigit(ch)) {
        result *= 10;
        result += ch - '0';
        eat_char();
        ch = peek_next_char();
    }
    return result;
}

std::string_view Lexer::parse_identifier() {
    u64 identifier_start = input_cursor_;
    u64 count = 0;
    auto ch = peek_next_char();
    while (std::isalnum(ch) || ch == '_') {
        count += 1;
        eat_char();
        ch = peek_next_char();
    }

    return input_.substr(identifier_start, count);
}

void Lexer::skip_whitespaces() {
    while (true) {
        const auto ch = peek_next_char();

        if (ch == ' ' || ch == '\t' || ch == '\n' ||
            (ch == '\r' && peek_char(1) == '\n')) {
            eat_char();
            continue;
        }

        break;
    }
}

std::string_view token_type_to_string(TokenType type) {
    auto type_int = static_cast<int>(type);
    if (type_int > 0 && type_int < 256) {
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
