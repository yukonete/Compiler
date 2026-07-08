#ifndef TOKEN_H
#define TOKEN_H

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
    keyword_size_of,

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
};

struct Token {
    TokenType type = TokenType::invalid;
    u64 start = 0;
    u64 end = 0;
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
        case keyword_size_of: return "size_of";
        case eof: return "eof";
    }

    return "unknown";
}

template <>
struct std::formatter<TokenType> : public std::formatter<std::string_view> {
    template <class FmtContext>
    FmtContext::iterator format(TokenType type, FmtContext &ctx) const {
        return std::formatter<std::string_view>::format(
            token_type_to_string(type), ctx);
    }
};

#endif