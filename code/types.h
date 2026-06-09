#ifndef TYPER_TYPES_H
#define TYPER_TYPES_H

struct Type {
    enum class Kind {
        BASIC,
        INT,
        BOOL,
        FLOAT,
        STRING,
        POINTER,
        ARRAY,
        STRUCT,
        PROCEDURE,
    };

    Kind kind;
};

#endif