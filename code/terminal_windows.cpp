#include "terminal.h"

#define WINBASEAPI __declspec(dllimport)
#define WINAPI __stdcall

using BOOL = int;
using UINT = unsigned int;

extern "C" {
WINBASEAPI BOOL WINAPI SetConsoleCP(UINT wCodePageID);
WINBASEAPI BOOL WINAPI SetConsoleOutputCP(UINT wCodePageID);
}

void set_utf8_terminal() {
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
}