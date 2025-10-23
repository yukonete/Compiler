#include <print>

#include "Base.h"
#include "Lexer.h"
#include "Parser.h"

int main() 
{
	Arena arena;

	auto input = "while(true) { a: int; }";
	auto lexer = Lex::Lexer{input};
	auto parser = Ast::Parser{&lexer, &arena};
	auto [program, ok] = parser.ParseProgram();
	return 0;
}
