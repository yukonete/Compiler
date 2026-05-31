#include <print>

#include "base.h"
#include "parser.h"
#include "type_check.h"
#include "ast.h"

int main(int argc, char **argv) {
	char const *input_file_name = "test.txt";
	if (argc > 1) {
		input_file_name = argv[1];
	}

	DynamicArena arena;
	auto arena_allocator = DynamicArenaStorage{arena};

	auto [input, ok] = read_file_to_string(input_file_name);
	if (!ok) {
		std::println("Coud not open/read the file.");
		return 1;
	}
	input += '\n';

	auto parser = Ast::Parser(input, arena_allocator);
	auto program = parser.parse_program();
	if (program.error_count != 0) {
		std::println("Parsing error");
		return 1;
	}

	std::println("AST to code:");
	for (auto statement : program.declarations) {
		auto node_string = Ast::statement_to_string(statement, 0);
		std::println("{}", node_string);
	}

	auto type_checker = TypeCheck::TypeChecker(&arena_allocator);
	if (!type_checker.do_type_check(&program)) {
		std::println("Type checking error");
		return 1;
	}
	
	std::println("Defined types:");
	for (const auto &[name, type] : type_checker.global_scope.declarations) {
		std::println("{}\n", TypeCheck::type_to_string(type, true));
	}

	return 0;
}