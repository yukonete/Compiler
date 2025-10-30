#include <print>

#include "Base.h"
#include "Lexer.h"
#include "Parser.h"

int main() 
{
	Arena arena;

	auto input = "{ if (true) { a: int = 0; while (a <= 10) { a = 10 % a + 1 * 3 ; } } else if (false) { test; } }";
	auto lexer = Lexer{input};
	auto parser = Ast::Parser{&lexer, &arena};
	auto [program, ok] = parser.ParseProgram();
	for (auto statement : program.declarations)
	{
		auto node_string = Ast::NodeToString(statement);
		std::println("{}", node_string);
	}
	return 0;
}
