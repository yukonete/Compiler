#include <print>
#include <string>
#include <cstdlib>
#include <string_view>

#include "ast.h"
#include "base/allocator.h"
#include "parser.h"
#include "typer.h"

int main(int argc, char **argv) {
    std::string_view input = "test.txt";
    if (argc > 1) {
        input = argv[1];
    }

    auto parser = Ast::Parser::open(std::string{input}, NEW_ALLOCATOR)
                      .value_or_else([input]() -> Ast::Parser {
                          std::println("Could not open/read file {}.", input);
                          std::exit(1);
                      });

    if (!parser.parse_program()) {
        return 1;
    }

    std::println("AST to code:");
    for (auto statement : parser.ast) {
        auto node_string = Ast::statement_to_string(statement, 0);
        std::println("{}", node_string);
    }

    auto typer = Typing::Typer{parser, NEW_ALLOCATOR};
	typer.do_typing();

    return 0;
}