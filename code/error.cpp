#include <print>

#include "error.h"

void Reporter::add(Diagnostic &&diagnostic) {
    count += 1;
    if (diagnostic.kind == Diagnostic::Kind::WARNING) {
        warning_count += 1;
    }
    if (report_on_add) {
        std::println(log, "{}", diagnostic.message);
    }
    diagnostics.push_back(std::move(diagnostic));
}

bool Reporter::any_errors() const {
    return count != 0;
}

void Reporter::print_all_diagnostics() {
    for (const auto &diagnostic : diagnostics) {
        std::println(log, "{}", diagnostic.message);
    }
}

void highlight_location_on_line(Reporter &reporter, std::string &out, const FileLocation &start, const FileLocation &end) {
    auto line = reporter.lexer->get_line(start.byte);
    u64 spaces = 0;
    for (auto ch : line) {
        if (ch != ' ' && ch != '\t') {
            break;
        }
        spaces += 1;
    }
    line.remove_prefix(spaces);

    auto out_inserter = std::back_inserter(out);
    out_inserter = std::format_to(out_inserter, "    {}\n", line);
    assert(start.column > spaces);
    out_inserter = std::format_to(out_inserter, "    {:>{}}", '^', start.column - spaces);
    if (start.line == end.line && start.column < end.column) {
        out_inserter = std::format_to(out_inserter, "{:~>{}}", '^', end.column - start.column);
    }
    std::format_to(out_inserter, "");
}