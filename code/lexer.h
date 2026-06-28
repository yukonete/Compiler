#ifndef LEXER_H
#define LEXER_H

#include <algorithm>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <string>
#include <utility>
#include <cassert>
#include <cstdio>
#include <format>
#include <iterator>
#include <print>

#include "base/file.h"
#include "base/types.h"
#include "token.h"

class Lexer {
public:
    constexpr Lexer(std::string &&input, std::string &&file_name,
                             FILE *log)
        : file_name_{std::move(file_name)}, input_{std::move(input)}, log{log} {
    }
    constexpr Lexer(const Lexer &) = delete;
    constexpr Lexer& operator=(const Lexer &) = delete;
    constexpr Lexer(Lexer&&) = default;
    constexpr Lexer& operator=(Lexer &&) = default;

    static Maybe<Lexer> open(std::string &&path, FILE *log = stderr) {
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

    void tokenize_until_eof();

private:
    struct ParseNumberResult {
        std::string_view value;
        TokenType type; // integer or float
    };

    void tokenize();
    int peek_next_char() const;
    int peek_char(usize peek) const;
    void eat_char();
    Maybe<ParseNumberResult> parse_number();
    std::string_view parse_identifier();
    Maybe<std::string_view> parse_string();
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
    bool report_only_first_error = false;
    bool report_only_first_syntax_error = true;
};

template <>
struct std::formatter<TokenType> : std::formatter<std::string_view> {
    template <class FmtContext>
    FmtContext::iterator format(TokenType type, FmtContext &ctx) const {
        return std::formatter<std::string_view>::format(
            token_type_to_string(type), ctx);
    }
};

#endif