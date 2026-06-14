#ifndef ERROR_H
#define ERROR_H

#include <print>
#include <format>

#include "base/types.h"
#include "ast.h"
#include "lexer.h"

void highlight_token_on_line(Lexer &lexer, const FileLocation &start,
                             const FileLocation &end);

void highlight_token_on_line(Lexer &lexer, const Token &token);

template <typename... Args>
void error_no_line(Lexer &lexer, const FileLocation &location,
                   std::format_string<Args...> fmt, Args &&...args) {
    lexer.error_count += 1;
    if (lexer.log == nullptr) {
        return;
    }
    std::print(lexer.log, "{}({}:{}) Error: ", lexer.file_name(), location.line,
               location.column);
    std::println(lexer.log, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void error(Lexer &lexer, const FileLocation &start, const FileLocation &end,
           std::format_string<Args...> fmt, Args &&...args) {
    error_no_line(lexer, start, fmt, std::forward<Args>(args)...);
    highlight_token_on_line(lexer, start, end);
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
void warning(Lexer &lexer, const FileLocation &start, const FileLocation &end,
             std::format_string<Args...> fmt, Args &&...args) {
    warning_no_line(lexer, start, fmt, std::forward<Args>(args)...);
    highlight_token_on_line(lexer, start, end);
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
    if (lexer.log == nullptr) {
        return;
    }
    std::print(lexer.log, "{}({}:{}) Syntax error: ", lexer.file_name(),
               location.line, location.column);
    std::println(lexer.log, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void syntax_error(Lexer &lexer, const FileLocation &start,
                  const FileLocation &end, std::format_string<Args...> fmt,
                  Args &&...args) {
    syntax_error_no_line(lexer, start, fmt, std::forward<Args>(args)...);
    highlight_token_on_line(lexer, start, end);
}

template <typename... Args>
void syntax_error(Lexer &lexer, const Token &token,
                  std::format_string<Args...> fmt, Args &&...args) {
    syntax_error(lexer, token.start, token.end, fmt,
                 std::forward<Args>(args)...);
}

template <typename... Args>
void error(Lexer &lexer, Ast::Node auto *node, std::format_string<Args...> fmt,
           Args &&...args) {
    auto start_pos = node->start_token().start;
    auto end_pos = node->end_token().end;
    error(lexer, start_pos, end_pos, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void warning(Lexer &lexer, Ast::Node auto *node,
             std::format_string<Args...> fmt, Args &&...args) {
    auto start_pos = node->start_token().start;
    auto end_pos = node->end_token().end;
    warning(lexer, start_pos, end_pos, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void syntax_error(Lexer &lexer, Ast::Node auto *node,
                  std::format_string<Args...> fmt, Args &&...args) {
    auto start_pos = node->start_token().start;
    auto end_pos = node->end_token().end;
    syntax_error(lexer, start_pos, end_pos, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void error(Lexer &lexer, Ast::TypePath path, std::format_string<Args...> fmt,
           Args &&...args) {
    assert(path.size() > 0);
    error(lexer, path.front()->token.start, path.back()->token.end, fmt,
          std::forward<Args>(args)...);
}

#endif