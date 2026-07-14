#include "base/panic.h"
#include "base/arena.h"
#include "ast.h"

namespace Ast {

Token Statement::start_token() const {
    switch (kind) {
        using enum Statement::Kind;

        case BAD: return as<BadStatement>()->token;
        case EMPTY: return as<EmptyStatement>()->token;
        case IF: return as<IfStatement>()->token;
        case WHILE:
            return as<WhileStatement>()->token;
        case ASSIGNMENT:
            return as<AssignmentStatement>()->expression->start_token();
        case BLOCK: return as<BlockStatement>()->open;
        case RETURN: return as<ReturnStatement>()->token;
        case DECLARATION:
            return as<DeclarationStatement>()->declaration->start_token();
        case CONTINUE: return as<ContinueStatement>()->token;
        case BREAK: return as<BreakStatement>()->token;
        case EXPRESSION:
            return as<ExpressionStatement>()->expression->start_token();
    }
    panic("statement.kind is not Statement::Kind");
}

Token Statement::end_token() const {
    switch (kind) {
        using enum Statement::Kind;

        case BAD: return as<BadStatement>()->token;
        case EMPTY: return as<EmptyStatement>()->token;
        case IF: {
            auto if_statement = as<IfStatement>();
            if (if_statement->else_branch) {
                return if_statement->else_branch->body->end_token();
            }
            return if_statement->body->end_token();
        }
        case WHILE: {
            return as<WhileStatement>()->body->end_token();
        }
        case ASSIGNMENT: return as<AssignmentStatement>()->value->end_token();
        case BLOCK: return as<BlockStatement>()->close;
        case RETURN: return as<ReturnStatement>()->token;
        case DECLARATION:
            return as<DeclarationStatement>()->declaration->end_token();
        case CONTINUE: return as<ContinueStatement>()->token;
        case BREAK: return as<BreakStatement>()->token;
        case EXPRESSION:
            return as<ExpressionStatement>()->expression->end_token();
    }
    panic("statement.kind is not Statement::Kind");
}

Token Expression::start_token() const {
    switch (kind) {
        using enum Expression::Kind;

        case BAD: return as<BadExpression>()->token;
        case INTEGER_LITERAL: return as<IntegerLiteralExpression>()->token;
        case UNARY_OPERATOR: return as<UnaryOperatorExpression>()->op;
        case BINARY_OPERATOR:
            return as<BinaryOperatorExpression>()->left->start_token();
        case BOOL_LITERAL: return as<BoolLiteralExpression>()->token;
        case IDENTIFIER: return as<IdentifierExpression>()->identifier->token;
        case CALL_OPERATOR:
            return as<CallOperatorExpression>()->expression->start_token();
        case STRING_LITERAL: return as<StringLiteralExpression>()->token;
        case FLOAT_LITERAL: return as<FloatLiteralExpression>()->token;
        case INDEX: return as<IndexExpression>()->expression->start_token();
        case SELECTOR: return as<SelectorExpression>()->expression->start_token();
        case CAST_OPERATOR: return as<CastOperatorExpression>()->cast;
        case COMPOUND: {
            auto compound =  as<CompoundExpression>();
            if (compound->type) {
                return compound->type->start_token();
            }
            return compound->open;
        }
        case TYPE: return as<TypeExpression>()->type->start_token();
        case DEREF: return as<DerefExpression>()->expression->start_token();
        case SLICE: return as<SliceExpression>()->expression->start_token();
    }
    panic("expression.kind is not Expression::Kind");
}

Token Expression::end_token() const {
    switch (kind) {
        using enum Expression::Kind;

        case BAD: return as<BadExpression>()->token;
        case INTEGER_LITERAL: return as<IntegerLiteralExpression>()->token;
        case UNARY_OPERATOR:
            return as<UnaryOperatorExpression>()->right->end_token();
        case BINARY_OPERATOR:
            return as<BinaryOperatorExpression>()->right->end_token();
        case BOOL_LITERAL: return as<BoolLiteralExpression>()->token;
        case IDENTIFIER: return as<IdentifierExpression>()->identifier->token;
        case CALL_OPERATOR: return as<CallOperatorExpression>()->close;
        case STRING_LITERAL: return as<StringLiteralExpression>()->token;
        case FLOAT_LITERAL: return as<FloatLiteralExpression>()->token;
        case INDEX: return as<IndexExpression>()->close;
        case SELECTOR: return as<SelectorExpression>()->identifier->token;
        case CAST_OPERATOR: return as<CastOperatorExpression>()->expression->end_token();
        case COMPOUND: return as<CompoundExpression>()->close;
        case TYPE: return as<TypeExpression>()->type->end_token();
        case DEREF: return as<DerefExpression>()->expression->end_token();
        case SLICE: return as<SliceExpression>()->close;
    }
    panic("expression.kind is not Expression::Kind");
}

Token Declaration::start_token() const {
    switch (kind) {
        using enum Declaration::Kind;

        case VARIABLE: return as<VariableDeclaration>()->token;
        case PROCEDURE: return as<ProcedureDeclaration>()->type->token;
        case CONSTANT: return as<ConstDeclaration>()->token;
        case TYPE: return as<TypeDeclaration>()->token;
        case FIELD: return as<Field>()->identifier->token;
    }
    panic("delcaration.kind is not Declaration::Kind");
}

Token Declaration::end_token() const {
    switch (kind) {
        using enum Declaration::Kind;

        case VARIABLE: {
            auto variable = as<VariableDeclaration>();
            if (variable->value) {
                return variable->value->end_token();
            }
            if (variable->type) {
                return variable->type->end_token();
            }
            return variable->identifier->token;
        }
        case PROCEDURE: return as<ProcedureDeclaration>()->type->close;
        case CONSTANT: return as<ConstDeclaration>()->value->end_token();
        case TYPE: return as<TypeDeclaration>()->type->end_token();
        case FIELD: return as<Field>()->type->end_token();
    }
    panic("delcaration.kind is not Declaration::Kind");
}

Token Type::start_token() const {
    switch (kind) {
        using enum Type::Kind;

        case BAD: return as<BadType>()->token;
        case IDENTIFIER: return as<IdentifierType>()->path[0]->token;
        case STRUCT: return as<StructType>()->token;
        case POINTER: return as<PointerType>()->token;
        case PROCEDURE: return as<ProcedureType>()->token;
        case ARRAY: return as<ArrayType>()->open;
        case SLICE: return as<SliceType>()->open;
    }
    panic("type.kind is not Type::Kind");
}

Token Type::end_token() const {
    switch (kind) {
        using enum Type::Kind;

        case BAD: return as<BadType>()->token;
        case IDENTIFIER: {
            auto identifier = as<IdentifierType>();
            return identifier->path[identifier->path.size() - 1]->token;
        }
        case STRUCT: return as<StructType>()->close;
        case POINTER: return as<PointerType>()->type->end_token();
        case PROCEDURE: return as<ProcedureType>()->close;
        case ARRAY: return as<ArrayType>()->element_type->end_token();
        case SLICE: return as<SliceType>()->element_type->end_token();
    }
    panic("type.kind is not Type::Kind");
}

Maybe<TypePath>
expression_to_type_path(const Expression *expression,
                        AllocatorVector<Identifier*> &out) {
    auto type_path_from_expression =
        [&out](this auto &&self,
                     const Ast::Expression *expression) -> bool {
        switch (expression->kind) {
            using enum Expression::Kind;

            case IDENTIFIER: {
                auto identifier =
                    expression->as<Ast::IdentifierExpression>()
                        ->identifier;
                out.push_back(identifier);
                return true;
            }

            case SELECTOR: {
                auto selector =
                    expression->as<Ast::SelectorExpression>();
                if (!self(selector->expression)) {
                    return false;
                }
                out.push_back(selector->identifier);
                return true;
            }

            default: return false;
        }
    };

    if (type_path_from_expression(expression)) {
        assert(!out.empty());
        return std::span{out};
    }
    return {};
}

static void append(std::string &out, std::string_view str) {
    out += str;
}

template <typename T>
void appendf(std::string &out, const T &value) {
    std::format_to(std::back_inserter(out), "{}", value);
} 

static void indent(std::string &out, u32 indent_level) {
    constexpr auto TAB_WIDTH = 4;
    for (u32 i = 0; i < indent_level * TAB_WIDTH; ++i) {
        out += " ";
    }
}

void Statement::dump(std::string &out, u32 indent_level, bool do_indent) const {
    if (do_indent) {
        indent(out, indent_level);
    }
    switch (kind) {
        using enum Statement::Kind;

        case BAD: {
            append(out, "BAD_STATEMENT;");
            return;
        }

        case EMPTY: {
            append(out, ";");
            return;
        }

        case RETURN: {
            auto return_statement = as<ReturnStatement>();
            append(out, "return");
            if (return_statement->value) {
                append(out, " ");
                return_statement->value->dump(out, indent_level);
            }
            append(out, ";");
            return;
        }

        case IF: {
            auto if_statement = as<IfStatement>();
            append(out, "if ");
            if_statement->condition->dump(out, indent_level);
            append(out, " ");
            if_statement->body->dump(out, indent_level, false);
            if (if_statement->else_branch) {
                append(out, " else ");
                if_statement->else_branch->body->dump(out, indent_level, false);
            }
            return;
        }

        case WHILE: {
            auto while_statement = as<WhileStatement>();
            append(out, "while ");
            while_statement->condition->dump(out, indent_level);
            append(out, " ");
            while_statement->body->dump(out, indent_level, false);
            return;
        }

        case ASSIGNMENT: {
            auto assignment_statement = as<AssignmentStatement>();
            assignment_statement->expression->dump(out, indent_level);
            append(out, " ");
            appendf(out, assignment_statement->assign.type);
            append(out, " ");
            assignment_statement->value->dump(out, indent_level);
            append(out, ";");
            return;
        }

        case BLOCK: {
            auto block_statement = as<BlockStatement>();
            append(out, "{\n");
            for (auto statement : block_statement->body) {
                statement->dump(out, indent_level + 1);
                append(out, "\n");
            }
            indent(out, indent_level);
            append(out, "}");
            return;
        }

        case EXPRESSION: {
            auto expression_statement = as<ExpressionStatement>();
            expression_statement->expression->dump(out, indent_level);
            append(out, ";");
            return;
        }

        case DECLARATION: {
            auto declaration_statement = as<DeclarationStatement>();
            declaration_statement->declaration->dump(out, indent_level);
            return;
        }

        case BREAK: {
            append(out, "break;");
            return;
        }

        case CONTINUE: {
            append(out, "continue;");
            return;
        }
    }
    panic("statement.kind is not Statement::Kind");
}

void Expression::dump(std::string& out, u32 indent_level) const {
    switch (kind) {
        using enum Expression::Kind;

        case BAD: {
            append(out, "BAD_EXPRESSION");
            return;
        }

        case INTEGER_LITERAL: {
            auto integer_literal = as<IntegerLiteralExpression>();
            appendf(out, integer_literal->value);
            return;
        }

        case BOOL_LITERAL: {
            auto bool_literal = as<BoolLiteralExpression>();
            appendf(out, bool_literal->value);
            return;
        }

        case IDENTIFIER: {
            auto identifier = as<IdentifierExpression>();
            append(out, identifier->identifier->token.value); 
            return;
        }

        case UNARY_OPERATOR: {
            auto unary_operator = as<UnaryOperatorExpression>();
            if (unary_operator->op.type == TokenType::keyword_size_of) {
                append(out, "size_of(");
                unary_operator->right->dump(out, indent_level);
                append(out, ")");
            } else {
                append(out, "(");
                appendf(out, unary_operator->op.type);
                unary_operator->right->dump(out, indent_level);
                append(out, ")");
            }
            return;
        }

        case COMPOUND: {
            auto compound = as<CompoundExpression>();
            if (compound->type) {
                compound->type->dump(out, indent_level);
            }
            append(out, "{");
            for (auto i : indices(compound->values.size())) {
                if (i != 0) {
                    append(out, ", ");
                }

                auto member = compound->values[i];
                if (member->identifier) {
                    append(out, member->identifier->token.value);
                    append(out, "=");   
                }
                member->value->dump(out, indent_level);
            }
            append(out, "}");
            return;
        }

        case BINARY_OPERATOR: {
            auto binary_operator = as<BinaryOperatorExpression>();
            append(out, "(");
            binary_operator->left->dump(out, indent_level);
            appendf(out, binary_operator->op.type);
            binary_operator->right->dump(out, indent_level);
            append(out, ")");
            return;
        }

        case SELECTOR: {
            auto selector = as<SelectorExpression>();
            selector->expression->dump(out, indent_level);
            append(out, ".");
            appendf(out, selector->identifier->token.value);
            return;
        }

        case INDEX: {
            auto index = as<IndexExpression>();
            index->expression->dump(out, indent_level);
            append(out, "[");
            index->index->dump(out, indent_level);
            append(out, "]");
            return;
        }

        case CALL_OPERATOR: {
            auto call_operator = as<CallOperatorExpression>();
            call_operator->expression->dump(out, indent_level);
            append(out, "(");
            for (usize i : indices(call_operator->arguments.size())) {
                if (i != 0) {
                    append(out, ", ");
                }

                auto arg = call_operator->arguments[i];
                arg->dump(out, indent_level);
            }
            append(out, ")");
            return;
        }

        case STRING_LITERAL: {
            auto string = as<StringLiteralExpression>();
            append(out, string->token.value);
            return;
        }

        case FLOAT_LITERAL: {
            auto float_literal = as<FloatLiteralExpression>();
            appendf(out, float_literal->value);
            return;
        }

        case CAST_OPERATOR: {
            auto cast = as<CastOperatorExpression>();
            append(out, "cast(");
            cast->type->dump(out, indent_level);
            append(out, ")");
            cast->expression->dump(out, indent_level);
            return;
        }

        case TYPE: {
            auto type = as<TypeExpression>();
            type->type->dump(out, indent_level);
            return;
        }

        case DEREF: {
            auto deref = as<DerefExpression>();
            deref->expression->dump(out, indent_level);
            append(out, ".*");
            return;
        }

        case SLICE: {
            auto slice = as<SliceExpression>();
            slice->expression->dump(out, indent_level);
            append(out, "[");
            if (slice->interval_open) {
                slice->interval_open->dump(out, indent_level);
            }
            append(out, ":");
            if (slice->interval_close) {
                slice->interval_close->dump(out, indent_level);
            }
            append(out, "]");
            return;
        }
    }
    panic("expression.kind is not Expression::Kind");
}

void Declaration::dump(std::string &out, u32 indent_level) const {
    switch (kind) {
        using enum Declaration::Kind;

        case FIELD: {
            auto field = as<Field>();
            if (field->identifier != nullptr) {
                append(out, field->identifier->token.value);
                append(out, ": ");
            }
            field->type->dump(out, indent_level);
            return;
        }

        case VARIABLE: {
            auto variable = as<VariableDeclaration>();
            append(out, "var ");
            append(out, variable->identifier->token.value);
            if (variable->type) {
                append(out, ": ");
                variable->type->dump(out, indent_level);
            }
            if (variable->value) {
                append(out, " = ");
                variable->value->dump(out, indent_level);
            }
            append(out, ";");
            return;
        }

        case CONSTANT: {
            auto contanst = as<ConstDeclaration>();
            append(out, "const ");
            append(out, contanst->identifier->token.value);
            if (contanst->type) {
                append(out, ": ");
                contanst->type->dump(out, indent_level);
            }
            append(out, " = ");
            contanst->value->dump(out, indent_level);
            append(out, ";");
            return;
        }

        case PROCEDURE: {
            auto function = as<ProcedureDeclaration>();
            append(out, "fn ");
            append(out, function->identifier->token.value);
            function->type->dump(out, indent_level, false);
            append(out, " ");
            function->body->dump(out, indent_level, false);
            return;
        }

        case TYPE: {
            auto type = as<TypeDeclaration>();
            append(out, "type ");
            append(out, type->identifier->token.value);
            append(out, " = ");
            type->type->dump(out, indent_level);
            append(out, ";");
            return;
        }
    }
    panic("declaration.kind is not Declaration::Kind");
}

void Type::dump(std::string &out, u32 indent_level, bool include_fn) const {
    switch (kind) {
         using enum Type::Kind;

        case BAD: {
            append(out, "BAD_TYPE");
            return;
        }

        case IDENTIFIER: {
            auto type_ident = as<IdentifierType>();
            for (auto i : indices(type_ident->path.size())) {
                if (i != 0) {
                    append(out, ".");
                }

                auto ident = type_ident->path[i];
                append(out, ident->token.value);
            }
            return;
        }

        case STRUCT: {
            auto type_struct = as<StructType>();
            append(out, "struct {");
            for (auto member : type_struct->members) {
                append(out, "\n");
                indent(out, indent_level + 1);
                member->dump(out, indent_level);
                append(out, ";");
            }
            for (auto declaration : type_struct->declarations) {
                append(out, "\n");
                declaration->dump(out, indent_level + 1);
            }
            if (type_struct->members.size() > 0 || type_struct->declarations.size() > 0) {
                append(out, "\n");
                indent(out, indent_level);
            }
            append(out, "}");
            return;
        }

        case POINTER: {
            auto type_pointer = as<PointerType>();
            append(out, "*");
            type_pointer->type->dump(out, indent_level);
            return;
        }

        case ARRAY: {
            auto type_array = as<ArrayType>();
            append(out, "[");
            type_array->count->dump(out, indent_level);
            append(out, "]");
            type_array->element_type->dump(out, indent_level);
            return;
        }

        case SLICE: {
            auto slice = as<SliceType>();
            append(out, "[]");
            slice->element_type->dump(out, indent_level);
            return;
        }

        case PROCEDURE: {
            auto type_function = as<ProcedureType>();
            if (include_fn) {
                append(out, "fn");
            }
            append(out, "(");
            for (auto i : indices(type_function->parameters.size())) {
                if (i != 0) {
                    append(out, ", ");
                }

                auto parameter = type_function->parameters[i];
                parameter->dump(out, indent_level);
            }
            append(out, ")");
            if (type_function->return_type) {
                append(out, " -> ");
                type_function->return_type->dump(out, indent_level);
            }
            return;
        }
    }
    panic("type.kind is not Type::Kind");
}

} // namespace Ast