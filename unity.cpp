#include "code/base/arena.cpp"
#include "code/base/utf8.cpp"
#include "code/base/file.cpp"

#include "code/utf8proc/utf8proc.c"

#include "code/value.cpp"
#include "code/big_int.cpp"
#include "code/ast.cpp"
#include "code/error.cpp"
#include "code/lexer.cpp"
#include "code/parser.cpp"
#include "code/types.cpp"
#include "code/typer.cpp"
#include "code/entity.cpp"
#if defined(_WIN64)
    #include "terminal_windows.cpp"
#else
    #include "terminal_linux.cpp"
#endif
#include "code/main.cpp"