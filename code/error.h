#ifndef ERROR_H
#define ERROR_H

#include <print>
#include <format>

#include "base/types.h"
#include "lexer.h"

void highlight_token_on_line(Lexer &lexer, const FileLocation &start,
                             const FileLocation &end);

template <typename... Args>
void error_no_line(Lexer &lexer, const FileLocation &location,
                   std::format_string<Args...> fmt, Args &&...args) {
    lexer.error_count += 1;
    if (lexer.report_only_first_error && lexer.error_count > 1) {
        return;
    }

    if (lexer.log == nullptr) {
        return;
    }

    std::print(lexer.log, "{}({}:{}) Error: ", lexer.file_name(), location.line,
               location.column);
    std::println(lexer.log, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void error(Lexer &lexer, u64 start, u64 end, std::format_string<Args...> fmt,
           Args &&...args) {
    auto start_location = lexer.byte_position_to_file_location(start);
    auto end_location = lexer.byte_position_to_file_location(end);
    error_no_line(lexer, start_location, fmt, std::forward<Args>(args)...);
    if (!lexer.report_only_first_error || lexer.error_count == 1) {
        highlight_token_on_line(lexer, start_location, end_location);
    }
}

template <typename... Args>
void error(Lexer &lexer, const Token &token, std::format_string<Args...> fmt,
           Args &&...args) {
    error(lexer, token.start, token.end, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void warning_no_line(Lexer &lexer, const FileLocation &location,
                     std::format_string<Args...> fmt, Args &&...args) {
    if (lexer.log == nullptr) {
        return;
    }

    std::print(lexer.log, "{}({}:{}) Warning: ", lexer.file_name(),
               location.line, location.column);
    std::println(lexer.log, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void warning(Lexer &lexer, u64 start, u64 end, std::format_string<Args...> fmt,
             Args &&...args) {
    auto start_location = lexer.byte_position_to_file_location(start);
    auto end_location = lexer.byte_position_to_file_location(end);
    warning_no_line(lexer, start_location, fmt, std::forward<Args>(args)...);
    highlight_token_on_line(lexer, start_location, end_location);
}

template <typename... Args>
void warning(Lexer &lexer, const Token &token, std::format_string<Args...> fmt,
             Args &&...args) {
    warning(token.start, token.end, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void syntax_error_no_line(Lexer &lexer, const FileLocation &location,
                          std::format_string<Args...> fmt, Args &&...args) {
    lexer.error_count += 1;
    if (lexer.report_only_first_syntax_error && lexer.error_count > 1) {
        return;
    }

    if (lexer.log == nullptr) {
        return;
    }
    std::print(lexer.log, "{}({}:{}) Syntax error: ", lexer.file_name(),
               location.line, location.column);
    std::println(lexer.log, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void syntax_error(Lexer &lexer, u64 start, u64 end, std::format_string<Args...> fmt,
                  Args &&...args) {
    auto start_location = lexer.byte_position_to_file_location(start);
    auto end_location = lexer.byte_position_to_file_location(end);
    syntax_error_no_line(lexer, start_location, fmt, std::forward<Args>(args)...);
    if (!lexer.report_only_first_syntax_error || lexer.error_count == 1) {
        highlight_token_on_line(lexer, start_location, end_location);
    }
}

template <typename... Args>
void syntax_error(Lexer &lexer, const Token &token,
                  std::format_string<Args...> fmt, Args &&...args) {
    syntax_error(lexer, token.start, token.end, fmt,
                 std::forward<Args>(args)...);
}


#endif