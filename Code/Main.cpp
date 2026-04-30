#include <print>

#include "base.h"
#include "parser.h"
#include "type_check.h"

int main() {
	Arena arena;

	auto [input, ok] = read_file_to_string("test.txt");;
	if (!ok) {
		std::println("Coud not open/read the file.");
		return 1;
	}
	
	auto parser = Ast::Parser(input, &arena);
	auto program = parser.parse_program();
	// for (auto statement : program.declarations) {
	// 	auto node_string = Ast::node_to_string(statement, 0);
	// 	std::println("{}", node_string);
	// }

	if (program.error_count != 0) {
		std::println("Parsing error");
		return 1;
	}
	
	auto type_checker = TypeCheck::TypeChecker(&arena);
	if (!type_checker.do_type_check(&program)) {
		std::println("Type checking error");
		return 1;
	}

	for (const auto &[name, type] : type_checker.global_scope.declarations) {
		std::println("{}\n", TypeCheck::type_to_string(type, true));
	}

	return 0;
}