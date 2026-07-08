#include "code/base/arena.cpp"
#include "code/ast.cpp"
#include "code/error.cpp"
#include "code/lexer.cpp"
#include "code/parser.cpp"
#include "code/typer.cpp"
#include "code/base/file.cpp"
#include "code/utf8proc/utf8proc.c"
#include "base/utf8.cpp"
#if defined(_WIN32) || defined(_WIN64)
    #include "terminal_windows.cpp"
#else
    #include "termnial_linux.cpp"
#endif
#include "code/main.cpp"