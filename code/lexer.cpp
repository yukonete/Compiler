#include <array>
#include <cctype>
#include <string_view>
#include <cassert>

#include "base/panic.h"
#include "lexer.h"

const FileLocation FileLocation::no_location = FileLocation{
    static_cast<u64>(-1), 
    static_cast<u64>(-1), 
    static_cast<u64>(-1)
};

static TokenType check_identifier_for_keyword(std::string_view identifier) {
    if (Lexer::keywords.contains(identifier)) {
        return Lexer::keywords.at(identifier);
    }
    return TokenType::identifier;
}

std::string_view Lexer::get_line(u64 byte) const {
    if (byte >= input_.size()) {
        return {};
    }

    // new line is not a token and byte is byte position of the token
    // so it should never be new line
    assert(!is_new_line(input_[byte]));

    if (byte == 0) {
        for (usize i = 0; i < input_.size(); ++i) {
            if (is_new_line(input_[i])) {
                return input_view().substr(0, i);
            }
        } 
    }
    
    u64 start = 0;
    for (auto i = byte - 1; i < input_.size(); --i) {
        if (is_new_line(input_[i])) {
            start = i + 1;
            break;
        }
    }

    u64 end = input_.size();
    for (auto i = byte + 1; i < input_.size(); ++i) {
        if (is_new_line(input_[i])) {
            end = i;
            break;
        }
    }

    assert(end > start);
    return input_view().substr(start, end - start);
}

const Token &Lexer::next_token() {
    return peek_token(0);
}

// peek is how much to look ahead (or behind)
const Token &Lexer::peek_token(int peek) {
    auto index = static_cast<isize>(tokens_cursor_) + peek;
    if (index < 0) {
        panic("No previous token");
    }

    if (static_cast<usize>(index) < tokens_.size()) {
        return tokens_.at(tokens_cursor_ + peek);
    }

    tokenize();
    return peek_token(peek);
}

const Token &Lexer::previous_token() const {
    return tokens_.at(tokens_cursor_ - 1);
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
    skip_whitespaces_and_comments();
    auto ch = peek_next_char();

    Token token;
    token.start = current_location_;

    switch (ch) {
        case '=': {
            if (peek_char(1) == '=') {
                token.type = TokenType::equals;
                eat_char();
            } else {
                token.type = TokenType::assign;
            }
            eat_char();
            break;
        }
        case '!': {
            if (peek_char(1) == '=') {
                token.type = TokenType::not_equals;
                eat_char();
            } else {
                token.type = TokenType::bang;
            }
            eat_char();
            break;
        }

        case '.': {
            token.type = TokenType::dot;
            eat_char();
            break;
        }

        case '+': {
            if (peek_char(1) == '=') {
                token.type = TokenType::plus_assign;
                eat_char();
            } else {
                token.type = TokenType::plus;
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
                token.type = TokenType::minus;
            }
            eat_char();
            break;
        }
        case '*': {
            if (peek_char(1) == '=') {
                token.type = TokenType::multiply_assign;
                eat_char();
            } else {
                token.type = TokenType::star;
            }
            eat_char();
            break;
        }
        case '/': {
            if (peek_char(1) == '=') {
                token.type = TokenType::divide_assign;
                eat_char();
            } else {
                token.type = TokenType::divide;
            }
            eat_char();
            break;
        }
        case '%': {
            if (peek_char(1) == '=') {
                token.type = TokenType::modulo_assign;
                eat_char();
            } else {
                token.type = TokenType::modulo;
            }
            eat_char();
            break;
        }

        case '<': {
            if (peek_char(1) == '=') {
                token.type = TokenType::less_equals;
                eat_char();
            } else {
                token.type = TokenType::less;
            }
            eat_char();
            break;
        }
        case '>': {
            if (peek_char(1) == '=') {
                token.type = TokenType::greater_equals;
                eat_char();
            } else {
                token.type = TokenType::greater;
            }
            eat_char();
            break;
        }

        case '{': {
            token.type = TokenType::open_brace;
            eat_char();
            break;
        }
        case '}': {
            token.type = TokenType::close_brace;
            eat_char();
            break;
        }

        case '(': {
            token.type = TokenType::open_paren;
            eat_char();
            break;
        }
        case ')': {
            token.type = TokenType::close_paren;
            eat_char();
            break;
        }
        case '[': {
            token.type = TokenType::open_bracket;
            eat_char();
            break;
        }
        case ']': {
            token.type = TokenType::close_bracket;
            eat_char();
            break;
        }
        case ';': {
            token.type = TokenType::semicolon;
            eat_char();
            break;
        }
        case ':': {
            token.type = TokenType::colon;
            eat_char();
            break;
        }
        case ',': {
            token.type = TokenType::comma;
            eat_char();
            break;
        }
        case '&': {
            token.type = TokenType::ampersand;
            eat_char();
            break;
        }

        case '\"': {
            auto str = parse_string();
            if (str) {
                token.type = TokenType::string;
                token.value = str.value();
            }
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
            auto parse_result = parse_number();
            if (parse_result) {
                token.type = parse_result->type;
                token.value = parse_result->value;
            }
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
            token.value = parse_identifier();
            token.type = check_identifier_for_keyword(token.value);
            break;
        }

        case -1: {
            token.type = TokenType::eof;
            eat_char();
            break;
        }

        default: {
            token.type = TokenType::invalid;
            eat_char();
            break;
        }
    }

    token.end = current_location_;
    token.end.column -= 1;
    token.end.byte -= 1;
    assert(token.end.column != 0);
    tokens_.push_back(token);
}

int Lexer::peek_next_char() const {
    return peek_char(0);
}

int Lexer::peek_char(usize peek) const {
    auto index = input_cursor_ + peek;
    if (index >= input_.size()) {
        return -1;
    }

    return input_.at(index);
}

void Lexer::eat_char() {
    const auto ch = peek_next_char();
    if (is_new_line(ch)) {
        current_location_.line += 1;
        current_location_.column = 1;
        if (ch == '\r') {
            current_location_.byte += 1;
            input_cursor_ += 1;
        }
    } else {
        current_location_.column += 1;
    }
    current_location_.byte += 1;
    input_cursor_ += 1;
}

std::optional<Lexer::ParseNumberResult> Lexer::parse_number() {
    auto token_type = TokenType::integer;
    
    auto integer_start = input_cursor_;
    auto count = 0;
    while (true) {
        auto ch = peek_next_char();
        
        if (!(std::isdigit(ch) || ch == '.')) {
            break;
        }

        if (ch == '.') {
            if (token_type == TokenType::float_literal) {
                return {};
            }
            token_type = TokenType::float_literal;
        }

        count += 1;
        eat_char();
    }

    return ParseNumberResult{
        .value = input_view().substr(integer_start, count),
        .type = token_type,
    };
}

std::optional<std::string_view> Lexer::parse_string() {    
    auto ch = peek_next_char();
    assert(ch == '\"');
    eat_char();
    auto string_start = input_cursor_;
    auto count = 0;
    while (true) {
        ch = peek_next_char();
        
        if (ch == '\"' || ch == -1) {
            eat_char();
            break;
        }

        if (ch == '\\') {
            if (peek_char(1) == '\"') {
                count += 1;
                eat_char();
            }
        }

        if (is_new_line(ch)) {
            return {};
        }
        
        count += 1;
        eat_char();
    }

    if (ch == '\"') {
        return input_view().substr(string_start, count);
    }

    return {};
}

std::string_view Lexer::parse_identifier() {
    usize identifier_start = input_cursor_;
    usize count = 0;
    auto ch = peek_next_char();
    while (std::isalnum(ch) || ch == '_') {
        count += 1;
        eat_char();
        ch = peek_next_char();
    }

    return input_view().substr(identifier_start, count);
}

bool Lexer::is_new_line(int ch) const {
    return ch == '\n' || (ch == '\r' && peek_char(1) == '\n');
}

void Lexer::skip_whitespaces_and_comments() {
    while (true) {
        auto ch = peek_next_char();

        if (ch == '/' && peek_char(1) == '/') {
            eat_char();
            eat_char();
            while (true) {
                ch = peek_next_char();
                if (is_new_line(ch) || ch == -1) {
                    break;
                }
                eat_char();
            }

            eat_char();
            continue;
        } 

        if (ch == ' ' || ch == '\t' || is_new_line(ch)) {
            eat_char();
            continue;
        }

        break;
    }
}

const Token &Lexer::get_token_before(u64 byte) const {
    auto token =
        std::ranges::lower_bound(tokens_, byte, {}, [](const Token &token) {
            return token.start.byte;
        });

    assert(token != tokens_.end());
    assert(token != tokens_.begin());

    return *std::prev(token);
}