#pragma once

#include <string_view>
#include <format>
#include <print>

#include "lexer.h"

enum class DiagnosticsLevel { Warning, Error };

constexpr std::string_view diagnostics_level_to_string(DiagnosticsLevel level) {
    switch (level) {
        using enum DiagnosticsLevel;
        case Warning: return "Warning";
        case Error: return "Error";
    }
    return "Invalid DiagnosticsLevel";
}

template <typename... Args>
void log_diagnostics(FILE *log, DiagnosticsLevel level, const FileLocation &location,
                     std::format_string<Args...> fmt, Args &&...args) {
    if (log == nullptr) {
        return;
    }
    std::print(log, "{} at ({}, {}): ", diagnostics_level_to_string(level), location.line, location.column);
    std::println(log, fmt, std::forward<Args>(args)...);
}