#include <print>

#include "base.h"
#include "parser.h"

int main() {
	Arena arena;

	auto [input, ok] = read_entire_file("test.txt");;
	if (!ok) {
		std::println("Coud not open the file.");
		return 1;
	}
	
	auto parser = Ast::Parser(input, &arena);
	auto program = parser.parse_program();
	for (auto statement : program.declarations) {
		auto node_string = Ast::node_to_string(statement, 0);
		std::println("{}", node_string);
	}
	return 0;
}
