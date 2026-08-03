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
    usize error_count = 0;
    usize warning_count = 0;

    std::vector<Diagnostic> silent_errors;

    bool report_on_add = false;
    bool always_report_silent_errors = false;
    FILE *log = stderr;

    void add(Diagnostic &&diagnostic);
    void add_silent(Diagnostic &&diagnostic);
    bool any_errors() const;
    void print_all_diagnostics();
};

void highlight_location_on_line(const Reporter &reporter, std::string &out, const FileLocation &start,
                                const FileLocation &end);

template <typename... Args>
std::string format_diagnostic_message(const Reporter &reporter, u64 start, u64 end, std::string_view prefix,
                                      std::format_string<Args...> fmt, Args &&...args) {
    auto message = std::string{};
    if (start != 0) { 
        message = reporter.lexer->byte_position_to_location_string(start);
    }
    message += ' ';
    message += prefix;
    message += ": ";
    std::format_to(std::back_inserter(message), fmt, std::forward<Args>(args)...);
    message += '\n';
    if (start != 0) {
        assert(end != 0);
        auto start_location = reporter.lexer->byte_position_to_file_location(start);
        auto end_location = reporter.lexer->byte_position_to_file_location(end);
        highlight_location_on_line(reporter, message, start_location, end_location); 
    }
    return message;
}

template <typename... Args>
void error(Reporter &reporter, u64 start, u64 end, std::format_string<Args...> fmt, Args &&...args) {
    auto message = format_diagnostic_message(reporter, start, end, "Error", fmt, std::forward<Args>(args)...);
    reporter.add(Diagnostic{.kind = Diagnostic::Kind::ERROR, .start = start, .end = end, .message = message});
}

template <typename... Args>
void error(Reporter &reporter, const Token &token, std::format_string<Args...> fmt,
           Args &&...args) {
    error(reporter, token.start, token.end, fmt, std::forward<Args>(args)...);
}

// Silent erros should be errors that were caused by other errors

template <typename... Args>
void silent_error(Reporter &reporter, u64 start, u64 end, std::format_string<Args...> fmt, Args &&...args) {
    auto message = format_diagnostic_message(reporter, start, end, "Error", fmt, std::forward<Args>(args)...);
    reporter.add_silent(Diagnostic{.kind = Diagnostic::Kind::ERROR, .start = start, .end = end, .message = message});
}

template <typename... Args>
void silent_error(Reporter &reporter, const Token &token, std::format_string<Args...> fmt,
           Args &&...args) {
    silent_error(reporter, token.start, token.end, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void warning(Reporter &reporter, u64 start, u64 end, std::format_string<Args...> fmt,
             Args &&...args) {
    auto message = format_diagnostic_message(reporter, start, end, "Warning", fmt, std::forward<Args>(args)...);
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
void silent_error(Reporter &reporter, Ast::Node auto const *node, std::format_string<Args...> fmt,
           Args &&...args) {
    auto start_pos = node->start_token().start;
    auto end_pos = node->end_token().end;
    silent_error(reporter, start_pos, end_pos, fmt, std::forward<Args>(args)...);
}


template <typename... Args>
void warning(Reporter &reporter, Ast::Node auto const *node,
             std::format_string<Args...> fmt, Args &&...args) {
    auto start_pos = node->start_token().start;
    auto end_pos = node->end_token().end;
    warning(reporter, start_pos, end_pos, fmt, std::forward<Args>(args)...);
}

#endif