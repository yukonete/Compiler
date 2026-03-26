#include "type_check.h"
#include "base.h"

namespace TypeCheck {
void do_type_check(Ast::Program *program) {
    for (auto decl : program->declarations) {
        switch (decl->type) {
            using enum Ast::NodeType;

            case declaration_procedure:
                // TODO: Typecheck procedure signature and add it to global
                // scope
                break;
            case declaration_type:
                // TODO: Typecheck the type and add it to global scope
                break;
            case declaration_const:
                // TODO: Typecheck the const variable and add it to global scope
                break;
            case declaration_variable:
                // TODO: Typecheck the variable and add it to global scope
                break;
            default: panic("Unknown declaration"); break;
        }
    }
}
} // namespace TypeCheck