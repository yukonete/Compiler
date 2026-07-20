#ifndef ERROR_H
#define ERROR_H

#include <format>
#include <vector>

#include "base/types.h"
#include "lexer.h"
#include "token.h"
#include "ast.h"

struct Diagnostic {
    enum class Kind {
        INVALID,
        ERROR,
        WARNING,
    };

    Kind kind = Kind::INVALID;
    u64 start = 0;
    u64 end = 0;
    std::string message;
};

struct Reporter {
    Lexer *lexer = nullptr;

    std::vector<Diagnostic> diagnostics;
    u64 count = 0;
    u64 warning_count = 0;
    bool report_on_add = false;

    FILE *log = stderr;

    void add(Diagnostic &&diagnostic);
    bool any_errors() const;
    void print_all_diagnostics();
};

void highlight_location_on_line(Reporter &reporter, std::string &out, const FileLocation &start,
                                const FileLocation &end);

template <typename... Args>
void error(Reporter &reporter, u64 start, u64 end, std::format_string<Args...> fmt, Args &&...args) {
    auto message = reporter.lexer->byte_position_to_location_string(start);
    message += " Error: ";
    std::format_to(std::back_inserter(message), fmt, std::forward<Args>(args)...);
    message += '\n';
    auto start_location = reporter.lexer->byte_position_to_file_location(start);
    auto end_location = reporter.lexer->byte_position_to_file_location(end);
    highlight_location_on_line(reporter, message, start_location, end_location); 
    reporter.add(Diagnostic{.kind = Diagnostic::Kind::ERROR, .start = start, .end = end, .message = message});
}

template <typename... Args>
void error(Reporter &reporter, const Token &token, std::format_string<Args...> fmt,
           Args &&...args) {
    error(reporter, token.start, token.end, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void warning(Reporter &reporter, u64 start, u64 end, std::format_string<Args...> fmt,
             Args &&...args) {
    auto message = reporter.lexer->byte_position_to_location_string(start);
    message += " Warning: ";
    std::format_to(std::back_inserter(message), fmt, std::forward<Args>(args)...);
    message += '\n';
    auto start_location = reporter.lexer->byte_position_to_file_location(start);
    auto end_location = reporter.lexer->byte_position_to_file_location(end);
    highlight_location_on_line(reporter, message, start_location, end_location);
    reporter.add(Diagnostic{.kind = Diagnostic::Kind::WARNING, .start = start, .end = end, .message = message});
}

template <typename... Args>
void warning(Reporter &reporter, const Token &token, std::format_string<Args...> fmt,
             Args &&...args) {
    warning(reporter, token.start, token.end, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void error(Reporter &reporter, Ast::Node auto const *node, std::format_string<Args...> fmt,
           Args &&...args) {
    auto start_pos = node->start_token().start;
    auto end_pos = node->end_token().end;
    error(reporter, start_pos, end_pos, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void warning(Reporter &reporter, Ast::Node auto const *node,
             std::format_string<Args...> fmt, Args &&...args) {
    auto start_pos = node->start_token().start;
    auto end_pos = node->end_token().end;
    warning(reporter, start_pos, end_pos, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void error(Reporter &reporter, Ast::TypePath path, std::format_string<Args...> fmt,
           Args &&...args) {
    assert(path.size() > 0);
    error(reporter, path.front()->token.start, path.back()->token.end, fmt,
          std::forward<Args>(args)...);
}

#endif