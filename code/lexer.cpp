#include <array>
#include <cassert>
#include <cctype>
#include <string_view>

#include "base/allocator.h"
#include "base/panic.h"
#include "base/tformat.h"
#include "base/utf8.h"
#include "base/util.h"
#include "lexer.h"

struct Keyword {
    TokenType token_type = TokenType::invalid;
    std::string_view str;
};

static TokenType check_identifier_for_keyword(std::string_view identifier) {    
    static constexpr std::array keywords = {
        Keyword{TokenType::keyword_if, "if"},           
        Keyword{TokenType::keyword_else, "else"},
        Keyword{TokenType::keyword_while, "while"},     
        Keyword{TokenType::keyword_for, "for"},
        Keyword{TokenType::keyword_return, "return"},   
        Keyword{TokenType::keyword_fn, "fn"},
        Keyword{TokenType::keyword_true, "true"},       
        Keyword{TokenType::keyword_false, "false"},
        Keyword{TokenType::keyword_cast, "cast"},       
        Keyword{TokenType::keyword_transmute, "transmute"},
        Keyword{TokenType::keyword_type, "type"},       
        Keyword{TokenType::keyword_const, "const"},
        Keyword{TokenType::keyword_struct, "struct"},   
        Keyword{TokenType::keyword_var, "var"},
        Keyword{TokenType::keyword_break, "break"},     
        Keyword{TokenType::keyword_continue, "continue"},
        Keyword{TokenType::keyword_size_of, "size_of"},
    };

    auto search = std::ranges::find(keywords, identifier, &Keyword::str);
    if (search != keywords.end()) {
        return search->token_type;
    }
    return TokenType::identifier;
}

AllocatorString Lexer::location_to_report_string(const FileLocation &location) const {
    return tformat("{}({}:{})", file_name_, location.line, location.column);
}

AllocatorString Lexer::token_to_location_string(const Token &token) const {
    auto location = byte_position_to_file_location(token.start);
    return location_to_report_string(location);
}

FileLocation Lexer::byte_position_to_file_location(u64 byte_position) const {
    auto search = std::ranges::lower_bound(new_lines, byte_position);
    auto line_number = static_cast<u64>(std::distance(new_lines.begin(), search)) + 1;
    auto new_line_position = [&]() -> u64 {
        if (search == new_lines.begin()) {
            return 0;
        }
        return *std::prev(search) + 1;
    }();
    return FileLocation{.line = line_number, .column = byte_position - new_line_position + 1, .byte = byte_position};
}

std::string_view Lexer::get_line(u64 byte) const {
    if (byte >= input_.size()) {
        return {};
    }

    if (byte == 0) {
        for (auto i : indices(input_.size())) {
            if (is_new_line(input_[i])) {
                return input_view().substr(0, i);
            }
        }
    }

    u64 start = 0;
    for (auto i : reverse_indices(byte)) {
        if (is_new_line(input_[i])) {
            start = i + 1;
            break;
        }
    }

    u64 end = input_.size();
    if (is_new_line(input_[byte])) {
        // dont include '\r' in resulting string
        if (input_[byte - 1] == '\r') {
            end = byte - 1;
        } else {
            end = byte;
        }
    } else {
        for (auto i : iota(byte + 1, input_.size())) {
            if (is_new_line(input_[i])) {
                end = i;
                break;
            }
        }
    }

    assert(end > start);
    return input_view().substr(start, end - start);
}

Token Lexer::next_token() {
    return peek_token(0);
}

// peek is how much to look ahead (or behind)
Token Lexer::peek_token(int peek) {
    if (static_cast<isize>(tokens_cursor_) + peek < 0) {
        panic("No previous token");
    }

    auto index = tokens_cursor_ + static_cast<usize>(peek);
    if (index < tokens_.size()) {
        return tokens_.at(index);
    }

    tokenize();
    return peek_token(peek);
}

Token Lexer::previous_token() const {
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
    token.start = input_cursor_;

    if (is_letter(ch) || ch == '_') {
        token.value = parse_identifier();
        token.type = check_identifier_for_keyword(token.value);
    } else {
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
    }

    assert(input_cursor_ != 0);
    token.end = input_cursor_ - 1;
    tokens_.push_back(token);
}

void Lexer::tokenize_until_eof() {
    tokenize();
    while (tokens_.back().type != TokenType::eof) {
        tokenize();
    }
}

DecodeRuneResult Lexer::peek_next_char_result() const {
    return peek_char_result(0);
}

DecodeRuneResult Lexer::peek_char_result(u64 peek) const {
    return rune_at_pos(input_view_left(), peek);
}

Rune Lexer::peek_next_char() const {
    return peek_char(0);
}

Rune Lexer::peek_char(u64 peek) const {
    auto [rune, size] = peek_char_result(peek);
    return rune;
}

void Lexer::eat_char() {
    auto [ch, size] = peek_next_char_result();
    if (is_new_line(ch)) {
        if (ch == '\r') {
            input_cursor_ += 1;
        }
        new_lines.push_back(input_cursor_);
    }
    input_cursor_ += size;
}

Maybe<Lexer::ParseNumberResult> Lexer::parse_number() {
    auto token_type = TokenType::integer;

    auto integer_start = input_cursor_;
    usize count = 0;
    while (true) {
        auto ch = peek_next_char();

        if (!(is_digit(ch) || ch == '.')) {
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

Maybe<std::string_view> Lexer::parse_string() {
    assert(peek_next_char() == '\"');
    eat_char();
    auto string_start = input_cursor_;
    usize count = 0;
    while (true) {
        auto [ch, size] = peek_next_char_result();

        if (ch == '\"' || ch == -1) {
            eat_char();
            if (ch == '\"') {
                return input_view().substr(string_start, count);
            }
            return {};
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

        count += size;
        eat_char();
    }
}

std::string_view Lexer::parse_identifier() {
    usize identifier_start = input_cursor_;
    usize count = 0;
    while (true) {
        auto [ch, size] = peek_next_char_result();
        if (!(is_letter(ch) || is_digit(ch) || ch == '_')) {
            break;
        }
        count += size;
        eat_char();
    }
    return input_view().substr(identifier_start, count);
}

// ch has to be the current rune
bool Lexer::is_new_line(Rune ch) const {
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

Token Lexer::get_token_before(u64 byte) const {
    auto token = std::ranges::lower_bound(tokens_, byte, {}, &Token::start);

    assert(token != tokens_.end());
    assert(token != tokens_.begin());

    return *std::prev(token);
}