#pragma once

#include "base.h"
#include "lexer.h"

enum class DiagnosticsLevel { Warning, Error };

inline std::string_view diagnostics_level_to_string(DiagnosticsLevel level) {
    switch (level) {
        using enum DiagnosticsLevel;
        case Warning: return "Warning";
        case Error: return "Error";
    }
    return "";
}

template <typename... Args>
void log_diagnostics(FILE *log, DiagnosticsLevel level, const Token &token,
                     std::format_string<Args...> fmt, Args &&...args) {
    std::print(log, "{} at ({}, {}): ", diagnostics_level_to_string(level), token.start.line, token.start.column);
    std::println(log, fmt, std::forward<Args>(args)...);
}