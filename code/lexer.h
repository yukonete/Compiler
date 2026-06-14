#pragma once

#include <algorithm>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <string>
#include <optional>
#include <utility>
#include <cassert>
#include <cstdio>
#include <format>
#include <iterator>
#include <print>

#include "base/file.h"
#include "base/types.h"

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

constexpr std::string_view token_type_to_string(TokenType type) {
    switch (type) {
        using enum TokenType;
        case invalid: return "invalid";

        case dot: return ".";
        case plus: return "+";
        case minus: return "-";
        case star: return "*";
        case divide: return "/";
        case modulo: return "%";
        case assign: return "=";
        case bang: return "!";
        case equals: return "==";
        case not_equals: return "!=";
        case less: return "<";
        case greater: return ">";
        case plus_assign: return "+=";
        case minus_assign: return "-=";
        case multiply_assign: return "*=";
        case divide_assign: return "/=";
        case modulo_assign: return "%=";
        case return_arrow: return "->";
        case less_equals: return "<=";
        case greater_equals: return ">=";

        case open_brace: return "{";
        case close_brace: return "}";
        case open_paren: return "(";
        case close_paren: return ")";
        case open_bracket: return "[";
        case close_bracket: return "]";

        case semicolon: return ";";
        case colon: return ":";
        case comma: return ",";
        case ampersand: return "&";

        case float_literal: return "float";
        case string: return "string";
        case identifier: return "identifier";
        case integer: return "integer";
        case keyword_if: return "if";
        case keyword_else: return "else";
        case keyword_while: return "while";
        case keyword_for: return "for";
        case keyword_return: return "return";
        case keyword_fn: return "fn";
        case keyword_const: return "const";
        case keyword_struct: return "struct";
        case keyword_true: return "true";
        case keyword_false: return "false";
        case keyword_cast: return "cast";
        case keyword_transmute: return "transmute";
        case keyword_type: return "type";
        case keyword_var: return "var";
        case keyword_break: return "break";
        case keyword_continue: return "continue";
        case eof: return "eof";
    }

    return "unknown";
}

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

    constexpr Lexer(std::string &&input, std::string &&file_name,
                             FILE *log)
        : file_name_{std::move(file_name)}, input_{std::move(input)}, log{log} {
    }
    constexpr Lexer(const Lexer &) = delete;
    constexpr Lexer& operator=(const Lexer &) = delete;
    constexpr Lexer(Lexer&&) = default;
    constexpr Lexer& operator=(Lexer &&) = default;

    static std::optional<Lexer> open(std::string &&path, FILE *log = stderr) {
        return read_file_to_string(path.c_str()).transform(
            [&](std::string &&file_content) {
                return Lexer{std::move(file_content), std::move(path), log};
            });
    }

    // Might invalidate references to tokens
    const Token &next_token();
    // Might invalidate references to tokens
    const Token &peek_token(int peek);

    const Token &previous_token() const;

    // Finds first token that starts on byte or after
    // and returs token that comes before it
    // There has to be token before, otherwise crash
    const Token &get_token_before(u64 byte) const;

    void eat_token();
    void uneat_token();

    std::string_view get_line(u64 byte) const;

    std::string_view input_view() const {
        return input_;
    }

    bool any_errors() const {
        return error_count != 0;
    }

    std::string_view file_name() const {
        return file_name_;
    }

private:
    struct ParseNumberResult {
        std::string_view value;
        TokenType type; // integer or float
    };

    void tokenize();
    int peek_next_char() const;
    int peek_char(usize peek) const;
    void eat_char();
    std::optional<ParseNumberResult> parse_number();
    std::string_view parse_identifier();
    std::optional<std::string_view> parse_string();
    void skip_whitespaces_and_comments();
    bool is_new_line(int ch) const;
    
    std::string file_name_;
    
    std::string input_;
    usize input_cursor_ = 0;

    std::vector<Token> tokens_;
    usize tokens_cursor_ = 0;
    
    FileLocation current_location_ = {.line = 1, .column = 1, .byte = 0};
public:
    FILE *log = nullptr;
    u64 error_count = 0;
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