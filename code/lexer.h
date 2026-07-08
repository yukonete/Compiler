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

#include "base/allocator.h"
#include "base/file.h"
#include "base/types.h"
#include "base/utf8.h"
#include "token.h"

// TODO: Handle UTF8 input properly
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
        auto input = read_file_to_string(path.c_str());
        if (!input) {
            return {};
        }
        return Lexer{std::move(*input), std::move(path), log};
    }

    Token next_token();
    Token peek_token(int peek);
    Token previous_token() const;

    // Finds first token that starts on byte or after
    // and returs token that comes before it
    // There has to be token before, otherwise crash
    Token get_token_before(u64 byte) const;

    void eat_token();
    void uneat_token();

    std::string_view get_line(u64 byte) const;

    std::string_view input_view() const {
        return input_;
    }

    std::string_view input_view_left() const {
        if (input_cursor_ > input_.size()) {
            return {};
        }
        return input_view().substr(input_cursor_);
    }

    bool any_errors() const {
        return error_count != 0;
    }

    std::string_view file_name() const {
        return file_name_;
    }

    void tokenize_until_eof();

    FileLocation byte_position_to_file_location(u64 byte_position) const;
    AllocatorString location_to_report_string(const FileLocation &location) const;
    AllocatorString token_to_location_string(const Token &token) const;

private:
    struct ParseNumberResult {
        std::string_view value;
        TokenType type = TokenType::invalid; // integer or float
    };

    void tokenize();
    
    DecodeRuneResult peek_next_char_result() const;
    DecodeRuneResult peek_char_result(u64 peek) const;
    Rune peek_next_char() const;
    Rune peek_char(u64 peek) const;
    
    void eat_char();
    Maybe<ParseNumberResult> parse_number();
    std::string_view parse_identifier();
    Maybe<std::string_view> parse_string();
    void skip_whitespaces_and_comments();
    bool is_new_line(Rune ch) const;
    
    std::string file_name_;
    
    std::string input_;
    u64 input_cursor_ = 0;

    std::vector<Token> tokens_;
    usize tokens_cursor_ = 0;
    
    std::vector<u64> new_lines; 
public:
    FILE *log = nullptr;
    u64 error_count = 0;
    bool report_only_first_error = false;
    bool report_only_first_syntax_error = true;
};

#endif