#include "base/panic.h"
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
    panic("statement.kind is not Statement::Kind")
}

Token Statement::end_token() const {
    switch (kind) {
        using enum Statement::Kind;

        case BAD: return as<BadStatement>()->token;
        case EMPTY: return as<EmptyStatement>()->token;
        case IF: {
            auto if_statement = as<IfStatement>();
            if (if_statement->else_branch.has_value()) {
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
    panic("statement.kind is not Statement::Kind")
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
    }
    panic("expression.kind is not Expression::Kind")
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
    }
    panic("expression.kind is not Expression::Kind")
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
    panic("delcaration.kind is not Declaration::Kind")
}

Token Declaration::end_token() const {
    switch (kind) {
        using enum Declaration::Kind;

        case VARIABLE: {
            auto variable = as<VariableDeclaration>();
            if (variable->value.has_value()) {
                return variable->value.value()->end_token();
            }
            if (variable->type.has_value()) {
                return variable->type.value()->end_token();
            }
            return variable->identifier->token;
        }
        case PROCEDURE: return as<ProcedureDeclaration>()->type->close;
        case CONSTANT: return as<ConstDeclaration>()->value->end_token();
        case TYPE: return as<TypeDeclaration>()->type->end_token();
        case FIELD: return as<Field>()->type->end_token();
    }
    panic("delcaration.kind is not Declaration::Kind")
}

Token Type::start_token() const {
    switch (kind) {
        using enum Type::Kind;

        case BAD: return as<BadType>()->token;
        case IDENTIFIER: return as<IdentifierType>()->path[0]->token;
        case STRUCT: return as<StructType>()->token;
        case POINTER: return as<PointerType>()->token;
        case FUNCTION: return as<ProcedureType>()->token;
        case ARRAY: return as<ArrayType>()->open;
    }
    panic("type.kind is not Type::Kind")
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
        case FUNCTION: return as<ProcedureType>()->close;
        case ARRAY: return as<ArrayType>()->element_type->end_token();
    }
    panic("type.kind is not Type::Kind")
}

} // namespace Ast