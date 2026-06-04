#include <print>

#include "base/allocator.h"
#include "base/arena.h"
#include "base/file.h"
#include "parser.h"
#include "type_check.h"
#include "ast.h"

int main(int argc, char **argv) {
	std::string_view input = "test.txt";
	if (argc > 1) {
		input = argv[1];
	}

	auto parser = Ast::Parser::open(input, NEW_ALLOCATOR);
	if (!parser) {
		std::println("Could not open/read file {}.", input);
		return 1;
	}

	if (!parser->parse_program()) {
		return 1;
	}

	std::println("AST to code:");
	for (auto statement : parser->ast) {
		auto node_string = Ast::statement_to_string(statement, 0);
		std::println("{}", node_string);
	}

	// auto type_checker = TypeCheck::TypeChecker(&arena);
	// if (!type_checker.do_type_check(&program)) {
	// 	std::println("Type checking error");
	// 	return 1;
	// }
	
	// std::println("Defined types:");
	// for (const auto &[name, type] : type_checker.global_scope.declarations) {
	// 	std::println("{}\n", TypeCheck::type_to_string(type, true));
	// }

	return 0;
}