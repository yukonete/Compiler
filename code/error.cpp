#include "error.h"

void highlight_token_on_line(Lexer &lexer, const FileLocation &start,
                            const FileLocation &end) {
    if (lexer.log == nullptr) {
        return;
    }

    auto line = lexer.get_line(start.byte);
    u64 spaces = 0;
    for (auto ch : line) {
        if (ch != ' ' && ch != '\t') {
            break;
        }
        spaces += 1;
    }
    line.remove_prefix(spaces);

    std::println(lexer.log, "    {}", line);
    assert(start.column > spaces);
    std::print(lexer.log, "    {:>{}}", '^', start.column - spaces);
    if (start.line == end.line && start.column < end.column) {
        std::print(lexer.log, "{:~>{}}", '^', end.column - start.column);
    }
    std::println(lexer.log, "");
}

// void highlight_token_on_line(Lexer &lexer, const Token &token) {
//     highlight_token_on_line(lexer, token.start, token.end);
// }