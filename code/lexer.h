#pragma once

#include <algorithm>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <string>
#include <deque>

#include "base.h"

enum class TokenType {
    invalid = 0,

    identifier,
    integer,
    string,
    float_literal,

    dot,

    plus,
    minus,
    star,
    divide,
    modulo,
    
    plus_assign,
    minus_assign,
    multiply_assign,
    divide_assign,
    modulo_assign,

    assign,
    bang,

    equals,
    not_equals,

    less,
    greater,
    less_equals,
    greater_equals,

    return_arrow,

    keyword_var,
    keyword_if,
    keyword_else,
    keyword_for,
    keyword_while,
    keyword_return,
    keyword_fn,
    keyword_true,
    keyword_false,
    keyword_cast,
    keyword_transmute,
    keyword_type,
    keyword_const,
    keyword_struct,
    keyword_break,
    keyword_continue,

    open_brace,
    close_brace,

    open_paren,
    close_paren,

    open_bracket,
    close_bracket,

    semicolon,
    colon,
    comma,
    ampersand,

    eof,
};

struct FileLocation {
    u64 line = 0;
    u64 column = 0;
    u64 byte = 0;

    static const FileLocation no_location;
};

struct Token {
    TokenType type = TokenType::invalid;
    FileLocation start;
    FileLocation end;
    std::string_view value;
};

std::string_view token_type_to_string(TokenType type);

class Lexer {
public:
    static const inline std::unordered_map<std::string_view, TokenType>
        keywords = {
            {"if", TokenType::keyword_if},
            {"else", TokenType::keyword_else},
            {"while", TokenType::keyword_while},
            {"for", TokenType::keyword_for},
            {"return", TokenType::keyword_return},
            {"fn", TokenType::keyword_fn},
            {"true", TokenType::keyword_true},
            {"false", TokenType::keyword_false},
            {"cast", TokenType::keyword_cast},
            {"transmute", TokenType::keyword_transmute},
            {"type", TokenType::keyword_type},
            {"const", TokenType::keyword_const},
            {"struct", TokenType::keyword_struct},
            {"var", TokenType::keyword_var},
            {"break", TokenType::keyword_break},
            {"continue", TokenType::keyword_continue},
    };

    constexpr Lexer(std::string_view input) : input_{input} {}
    constexpr Lexer(const Lexer &) = delete;
    constexpr Lexer& operator=(const Lexer &) = delete;
    constexpr Lexer(Lexer&&) = default;
    constexpr Lexer& operator=(Lexer &&) = default;

    // Might invalidate references to tokens
    const Token &next_token();
    // Might invalidate references to tokens
    const Token &peek_token(int peek);

    const Token &previous_token() const;
    void eat_token();
    void uneat_token();

private:
    struct ParseStringResult {
        std::string_view str;
        bool ok = false;
    };

    struct ParseNumberResult {
        std::string_view value;
        TokenType type; // integer or float
        bool ok = false;
    };

    void tokenize();
    int peek_next_char() const;
    int peek_char(int peek) const;
    void eat_char();
    ParseNumberResult parse_number();
    std::string_view parse_identifier();
    ParseStringResult parse_string();
    void skip_whitespaces_and_comments();
    bool is_new_line(int ch);

    std::string_view input_;
    s64 input_cursor_ = 0;

    std::vector<Token> tokens_;
    s64 tokens_cursor_ = 0;
    
    FileLocation current_location_ = {.line = 1, .column = 1, .byte = 0};
};

template <> struct std::formatter<TokenType> {
    template <class ParseContext>
    constexpr ParseContext::iterator parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template <class FmtContext>
    FmtContext::iterator format(TokenType type, FmtContext &ctx) const {
        return std::ranges::copy(token_type_to_string(type), ctx.out()).out;
    }
};