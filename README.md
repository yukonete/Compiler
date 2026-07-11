# Compiler
Compiler for my programming language that has no name yet

For syntax see [syntax.md](syntax.md)

## Goals
For now: Windows only single file single-threaded compiler to native executable, with support for calling C

Eventually:
- Multiple files
- Multithreaded
- Calling language from C (producing static and dynamic libraries)
- Linux

## What is mostly done
- Parsing
- Typing:
    - Entity collection
    - Entity resolution
    - Recursive declaration detection
    - Sizing