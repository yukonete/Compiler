#include <print>

#include "Base.h"
#include "Parser.h"

int main()
{
	Arena arena;

	auto input = ReadEntireFile("test.txt");;
	auto parser = Ast::Parser{input, &arena};
	auto program = parser.ParseProgram();
	for (auto statement : program.declarations)
	{
		auto node_string = Ast::NodeToString(statement, 0);
		std::println("{}", node_string);
	}
	return 0;
}
