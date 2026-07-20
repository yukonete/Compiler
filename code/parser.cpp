#include <algorithm>
#include <cstdio>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ast.h"
#include "base/panic.h"
#include "error.h"
#include "lexer.h"
#include "parser.h"

namespace Ast {

bool Parser::parse_program() {
    while (true) {
        if (next_token_is(TokenType::eof)) {
            break;
        }

        auto statement = parse_statement();
        ast.push_back(statement);
    }

    return !reporter.any_errors();
}

Identifier *Parser::parse_identifier() {
    return New<Identifier>(expect_token(TokenType::identifier));
}

Statement *Parser::parse_statement() {
    invalid_statement_ = false;
    auto token = lexer.next_token();
    switch (token.type) {
        using enum TokenType;

        case keyword_var:
        case keyword_fn:
        case keyword_const:
        case keyword_type: {
            return New<DeclarationStatement>(parse_declaration());
        }

        case keyword_break: {
            auto break_statement = New<BreakStatement>(expect_token(TokenType::keyword_break));
            expect_token(TokenType::semicolon);
            return break_statement;
        }

        case keyword_continue: {
            auto continue_statement = New<ContinueStatement>(expect_token(TokenType::keyword_continue));
            expect_token(TokenType::semicolon);
            return continue_statement;
        }

        case keyword_if: return parse_if_statement();
        case keyword_while: return parse_while_statement();
        case keyword_return: return parse_return_statement();
        case open_brace: return parse_block_statement();

        // Operands
        case identifier:
        case keyword_true:
        case keyword_false:
        case integer:
        case string:
        case float_literal:
        case open_paren:
        // Unary operators
        case keyword_cast:
        case keyword_size_of:
        case ampersand:
        case star:
        case plus:
        case minus:
        case bang: {
            // Those are essentially "simple" statements
            // Might move that to separate function
            auto expression = parse_expression(Precedence::lowest, true);
            if (next_token_is(TokenType::assign) || next_token_is(TokenType::plus_assign) ||
                next_token_is(TokenType::minus_assign) || next_token_is(TokenType::divide_assign) ||
                next_token_is(TokenType::multiply_assign) || next_token_is(TokenType::modulo_assign)) {
                auto assign_token = lexer.next_token();
                lexer.eat_token();
                auto value = parse_expression();
                expect_token(TokenType::semicolon);
                return New<AssignmentStatement>(expression, assign_token, value);
            }

            expect_token(TokenType::semicolon);
            return New<ExpressionStatement>(expression);
        }

        case invalid:
        case divide:
        case modulo:
        case plus_assign:
        case minus_assign:
        case multiply_assign:
        case divide_assign:
        case modulo_assign:
        case dot:
        case assign:
        case equals:
        case not_equals:
        case less:
        case greater:
        case less_equals:
        case greater_equals:
        case return_arrow:
        case keyword_struct:
        case close_brace:
        case close_paren:
        case open_bracket:
        case close_bracket:
        case colon:
        case comma:
        case keyword_else:
        case keyword_for:
        case keyword_transmute: {
            auto token_string = token_type_to_string(token.type);
            error(reporter, token, "Unexpected token {}({})", token_string, token.value);
            lexer.eat_token();
            break;
        }
        
        case semicolon: {
            lexer.eat_token();
            return New<EmptyStatement>(token);
        }
        
        case eof: panic("parse_statement called with eof");
    }
    
    // Skip tokens untill there is a token that can be parsed or eof
    while (true) {
        auto token = lexer.next_token();
        switch (token.type) {
            using enum TokenType;
            case invalid:
            case divide:
            case modulo:
            case plus_assign:
            case minus_assign:
            case multiply_assign:
            case divide_assign:
            case modulo_assign:
            case dot:
            case assign:
            case equals:
            case not_equals:
            case less:
            case greater:
            case less_equals:
            case greater_equals:
            case return_arrow:
            case keyword_struct:
            case close_brace:
            case close_paren:
            case open_bracket:
            case close_bracket:
            case colon:
            case comma:
            case keyword_else:
            case keyword_for:
            case keyword_transmute: {
                lexer.eat_token();
                continue;
            }
            default:
                break;
        }
        break;
    }

    return New<BadStatement>(token);
}

Declaration *Parser::parse_declaration() {
    const auto token_type = lexer.next_token().type;
    switch (token_type) {
        using enum TokenType;

        case keyword_var: return parse_variable_declaration();
        case keyword_fn: return parse_procedure_declaration();
        case keyword_const: return parse_constant_declaration();
        case keyword_type: return parse_type_declaration();

        default: panic("parse_declaratin should be called with delcaration");
    }
}

ConstDeclaration *Parser::parse_constant_declaration() {
    auto const_token = expect_token(TokenType::keyword_const);
    auto identifier = parse_identifier();
    auto type = Maybe<Type *>{};
    if (next_token_is(TokenType::colon)) {
        lexer.eat_token();
        type = parse_type();
    }
    expect_token(TokenType::assign);
    auto value = parse_expression();
    expect_token(TokenType::semicolon);
    return New<ConstDeclaration>(const_token, identifier, type, value);
}

VariableDeclaration *Parser::parse_variable_declaration() {
    auto var_token = expect_token(TokenType::keyword_var);
    auto identifier = parse_identifier();

    auto type = Maybe<Type *>{};
    if (next_token_is(TokenType::colon)) {
        lexer.eat_token();
        type = parse_type();
    }

    auto value = Maybe<Expression *>{};
    if (next_token_is(TokenType::assign)) {
        lexer.eat_token();
        value = parse_expression();
    }

    expect_token(TokenType::semicolon);

    if (!type && !value) {
        error(reporter, var_token,
                     "Variable declaration has no type and value."
                     " At least one should be specified");
    }

    return New<VariableDeclaration>(var_token, identifier, type, value);
}

TypeDeclaration *Parser::parse_type_declaration() {
    auto type_token = expect_token(TokenType::keyword_type);
    auto identifier = parse_identifier();
    expect_token(TokenType::assign);
    auto type = parse_type(true);
    expect_token(TokenType::semicolon);
    return New<TypeDeclaration>(type_token, identifier, type);
}

ProcedureType *Parser::parse_procedure_type(bool skip_identifier) {
    auto fn_token = expect_token(TokenType::keyword_fn);
    if (skip_identifier) {
        if (!next_token_is(TokenType::identifier) && !reporter.any_errors()) {
            panic("parse_procedure_type was called with skip_identifier = true but "
                  "there was no identifier.");
        }
        expect_token(TokenType::identifier);
    }

    auto open = expect_token(TokenType::open_paren);
    auto parse_fields = [this]() -> std::span<Field *> {
        bool first_parameter = true;
        auto parameters_temp = create_temp_vector<Field *>(16);
        while (!(next_token_is(TokenType::close_paren) || next_token_is(TokenType::eof))) {
            if (!first_parameter) {
                expect_token(TokenType::comma);
            } else {
                first_parameter = false;
            }

            if (next_token_is(TokenType::identifier) && peek_token_is(TokenType::colon, 1)) {
                parameters_temp.push_back(parse_field());
            } else {
                auto type = parse_type();
                auto field = New<Field>(nullptr, type);
                parameters_temp.push_back(field);
            }
        }
        return NewArray(std::span{parameters_temp});
    };
    auto parameters = parse_fields();
    auto close = expect_token(TokenType::close_paren);

    auto return_type = Maybe<Type *>{};
    if (next_token_is(TokenType::return_arrow)) {
        lexer.eat_token();
        if (next_token_is(TokenType::open_paren)) {
            auto open_return = expect_token(TokenType::open_paren);
            auto return_values = parse_fields();
            auto close_return = expect_token(TokenType::close_paren);
            if (return_values.size() > 0) {
                return_type = return_values[0]->type;
            } else {
                error(reporter, open_return.start, close_return.end, "Expected return type");
            }
            if (return_values.size() > 1) {
                error(reporter, open_return.start, close_return.end, "Multiple return type are not supported");
            }
        } else {
            return_type = parse_type();
        }
    }

    return New<ProcedureType>(fn_token, open, parameters, close, return_type);
}

ProcedureDeclaration *Parser::parse_procedure_declaration() {
    expect_token(TokenType::keyword_fn);
    auto identifier = parse_identifier();
    lexer.uneat_token(); // Put identifier back (it is going to be skipped in
                         // parse_procedure_type)
    lexer.uneat_token(); // Put fn back
    auto type = parse_procedure_type(true);
    auto body = parse_block_statement();
    return New<ProcedureDeclaration>(identifier, type, body);
}

IfStatement *Parser::parse_if_statement() {
    auto if_token = expect_token(TokenType::keyword_if);
    auto condition = parse_expression(Precedence::lowest, true);
    auto body = parse_block_statement();
    auto else_branch = Maybe<IfStatement::ElseBranch>{};
    if (next_token_is(TokenType::keyword_else)) {
        auto else_token = expect_token(TokenType::keyword_else);
        auto statement = parse_statement();
        if (!statement->is<BlockStatement>() && !statement->is<IfStatement>()) {
            error(reporter, statement, "Only block or if statements are allowed after else");
        }
        else_branch = IfStatement::ElseBranch{else_token, statement};
    }
    return New<IfStatement>(if_token, condition, body, else_branch);
}

WhileStatement *Parser::parse_while_statement() {
    auto while_token = expect_token(TokenType::keyword_while);
    auto condition = parse_expression(Precedence::lowest, true);
    auto body = parse_block_statement();
    return New<WhileStatement>(while_token, condition, body);
}

Field *Parser::parse_field() {
    auto identifier = parse_identifier();
    expect_token(TokenType::colon);
    auto type = parse_type();
    return New<Field>(identifier, type);
}

Type *Parser::parse_type(bool named) {
    auto token = lexer.next_token();
    lexer.eat_token();

    switch (token.type) {
        using enum TokenType;

        case identifier: {
            lexer.uneat_token();
            auto path_temp = create_temp_vector<Identifier *>(16);
            while (true) {
                path_temp.push_back(parse_identifier());
                if (next_token_is(TokenType::dot)) {
                    lexer.eat_token();
                    continue;
                }

                break;
            }
            auto path = NewArray(std::span{path_temp});
            return New<IdentifierType>(path);
        }

        case star: {
            auto pointer_token = token;
            auto type = parse_type();
            return New<PointerType>(pointer_token, type);
        }

        case keyword_struct: {
            auto struct_token = token;

            auto open = expect_token(TokenType::open_brace);
            auto members_temp = create_temp_vector<Field *>(16);
            auto declarations_temp = create_temp_vector<DeclarationStatement *>(16);
            while (!(next_token_is(TokenType::close_brace) || next_token_is(TokenType::eof))) {
                if (next_token_is(TokenType::identifier)) {
                    auto field = parse_field();
                    expect_token(TokenType::semicolon);
                    members_temp.push_back(field);
                    continue;
                }

                // Next token is not an identifier, which means that it is not a
                // member Which means it has to be a declaration, but not a
                // variable declaration
                auto statement = parse_statement();
                if (!statement->is<DeclarationStatement>()) {
                    error(reporter, statement, "Expected declaration or struct member");
                } else {
                    if (!named) {
                        error(reporter, statement, "Declarations are not allowed inside unnamed struct");
                    }
                    declarations_temp.push_back(statement->as<DeclarationStatement>());
                }
            }
            auto members = NewArray(std::span{members_temp});
            auto declarations = NewArray(std::span{declarations_temp});
            auto close = expect_token(TokenType::close_brace);

            return New<StructType>(struct_token, open, members, declarations, close);
        }

        case keyword_fn: {
            lexer.uneat_token();
            return parse_procedure_type();
        }

        case open_bracket: {
            auto open = token;
            if (next_token_is(TokenType::close_bracket)) {
                auto close = expect_token(TokenType::close_bracket);
                auto element_type = parse_type();
                return New<SliceType>(open, close, element_type);
            }
            auto count = parse_expression();
            auto close = expect_token(TokenType::close_bracket);
            auto element_type = parse_type();
            return New<ArrayType>(open, count, close, element_type);
        }

        default: {
            error(reporter, token, "Token \"{}\" can not be parsed as a type", token.type);
            return New<BadType>(token);
        }
    }
}

ReturnStatement *Parser::parse_return_statement() {
    auto return_token = expect_token(TokenType::keyword_return);
    auto value = Maybe<Expression *>{};
    if (!next_token_is(TokenType::semicolon)) {
        value = parse_expression();
    }
    expect_token(TokenType::semicolon);
    return New<ReturnStatement>(return_token, value);
}

BlockStatement *Parser::parse_block_statement() {
    auto open = expect_token(TokenType::open_brace);
    auto statements_temp = create_temp_vector<Statement *>(256);
    while (!(next_token_is(TokenType::close_brace) || next_token_is(TokenType::eof))) {
        statements_temp.push_back(parse_statement());
    }
    auto statements = NewArray(std::span{statements_temp});
    auto close = expect_token(TokenType::close_brace);
    return New<BlockStatement>(open, statements, close);
}

// Returns precedence of a binary operator.
static Precedence token_type_to_precedense(TokenType type) {
    switch (type) {
        using enum TokenType;
        case equals:
        case not_equals: return Precedence::equals;

        case less:
        case greater:
        case less_equals:
        case greater_equals: return Precedence::comparison;

        case plus:
        case minus: return Precedence::plus;

        case star:
        case divide:
        case modulo: return Precedence::multiply;

        case dot:
        case open_bracket:
        case open_brace:
        case open_paren: return Precedence::call;

        default: return Precedence::lowest;
    }
}

Expression *Parser::parse_unary_expression(bool lhs) {
    auto token = lexer.next_token();

    switch (token.type) {
        using enum TokenType;

        // Operands
        case open_paren: {
            lexer.eat_token();
            auto expression = parse_expression();
            expect_token(close_paren);
            return expression;
        }

        case identifier: {
            return New<IdentifierExpression>(parse_identifier());
        }

        case open_brace: {
            if (lhs) {
                lexer.eat_token();
                error(reporter, token, "Expected expression");
                return New<BadExpression>(token);
            }
            return parse_compound_expression();
        }

        case integer: {
            auto parse_u64 = [](std::string_view str) -> Maybe<u64> {
                u64 value = 0;
                for (auto digit : str) {
                    auto digit_value = static_cast<u8>(digit - '0');
                    auto old_value = value;
                    value = value * 10 + digit_value;
                    if (value < old_value) {
                        // Overflowed
                        return {};
                    }
                }
                return value;
            };

            lexer.eat_token();
            auto value = parse_u64(token.value);
            if (!value) {
                error(reporter, token, "Integer literal is bigger than 18,446,744,073,709,551,615");
                value = 0;
            }
            return New<IntegerLiteralExpression>(token, *value);
        }

        case float_literal: {
            auto parse_f64 = [](std::string_view str) -> f64 {
                // TODO: Figure out how this approach affects precision
                f64 integer_part = 0;
                f64 decimal_part = 0;
                f64 decimal_divider = 1;
                bool encountered_dot = false;
                for (auto digit : str) {
                    if (digit == '.') {
                        encountered_dot = true;
                        continue;
                    }
                    auto digit_value = digit - '0';
                    if (!encountered_dot) {
                        integer_part = integer_part * 10 + digit_value;
                    } else {
                        decimal_divider *= 10;
                        decimal_part = decimal_part * 10 + digit_value;
                    }
                }
                return integer_part + decimal_part / decimal_divider;
            };

            lexer.eat_token();
            auto value = parse_f64(token.value);
            return New<FloatLiteralExpression>(token, value);
        }

        case keyword_true:
        case keyword_false: {
            lexer.eat_token();
            auto value = (token.type == keyword_true);
            return New<BoolLiteralExpression>(token, value);
        }

        case string: {
            lexer.eat_token();
            return New<StringLiteralExpression>(token);
        }

        // Operators
        case plus:
        case minus:
        case bang:
        case ampersand: {
            lexer.eat_token();
            auto op = token;
            auto right = parse_expression(Precedence::prefix, lhs);
            return New<UnaryOperatorExpression>(op, right);
        }

        // Should size_of be a separete type, like SizeOfExpression?
        case keyword_size_of: {
            lexer.eat_token();
            auto size_of = token;
            expect_token(TokenType::open_paren);
            auto expression = parse_expression();
            expect_token(TokenType::close_paren);
            return New<UnaryOperatorExpression>(size_of, expression);
        }

        case keyword_cast: {
            lexer.eat_token();
            auto cast = token;
            expect_token(TokenType::open_paren);
            auto type = parse_type();
            expect_token(TokenType::close_paren);
            auto expression = parse_expression(Precedence::prefix, lhs);
            return New<CastOperatorExpression>(cast, type, expression);
        }

        // Types
        // TypeIdentifier is parsed as IdentifierExpression
        case star:
        case open_bracket:
        case keyword_struct: {
            auto type = parse_type();
            return New<TypeExpression>(type);
        }

        default: {
            error(reporter, token, "Token \"{}\" can not be parsed as a unary expression", token.type);
            return New<BadExpression>(token);
        }
    }
}

static Maybe<TypePath> expression_to_type_path(Parser &parser, const Expression *expression) {
    auto type_path_storage = create_temp_vector<Identifier *>(16);
    auto type_path = expression_to_type_path(expression, type_path_storage);
    if (type_path) {
        return parser.NewArray(*type_path);
    }
    return {};
}

CompoundExpression *Parser::parse_compound_expression(Maybe<Type *> type) {
    auto open = expect_token(TokenType::open_brace);
    auto members_temp = create_temp_vector<CompoundFields *>(16);
    bool first = true;
    bool expect_identifier = false;
    while (!(next_token_is(TokenType::close_brace) || next_token_is(TokenType::eof))) {
        if (!first) {
            expect_token(TokenType::comma);
        } else {
            first = false;
        }

        auto identifier = Maybe<Identifier *>{};
        auto designated = next_token_is(TokenType::identifier) && peek_token_is(TokenType::assign, 1) &&
                          !peek_token_is(TokenType::assign, 2);
        if (designated) {
            expect_identifier = true;
            identifier = parse_identifier();
            expect_token(TokenType::assign);
        }
        auto expression = parse_expression();
        auto member = New<CompoundFields>(identifier, expression);
        if (expect_identifier && !identifier) {
            error(reporter, expression, "Mixture of 'field=value' and value elements in literal is not allowed");
        }
        members_temp.push_back(member);
    }
    auto values = NewArray(std::span{members_temp});
    auto close = expect_token(TokenType::close_brace);
    return New<CompoundExpression>(type, open, values, close);
}

Expression *Parser::parse_binary_expression(Expression *left, bool lhs) {
    auto token = lexer.next_token();
    lexer.eat_token();

    if (token.type == TokenType::open_paren) {
        auto open = token;
        auto arguments_temp = create_temp_vector<Expression *>(16);
        bool first_argument = true;
        while (!(next_token_is(TokenType::close_paren) || next_token_is(TokenType::eof))) {
            if (!first_argument) {
                expect_token(TokenType::comma);
            } else {
                first_argument = false;
            }
            arguments_temp.push_back(parse_expression());
        }
        auto arguments = NewArray(std::span{arguments_temp});
        auto close = expect_token(TokenType::close_paren);
        return New<CallOperatorExpression>(left, open, arguments, close);
    }

    if (token.type == TokenType::open_bracket) {
        auto open = token;
        Expression *index = nullptr;
        if (!next_token_is(TokenType::colon)) {
            index = parse_expression();
        }
        if (next_token_is(TokenType::colon)) {
            auto colon = expect_token(TokenType::colon);
            Expression *interval_close = nullptr;
            if (!next_token_is(TokenType::close_bracket)) {
                interval_close = parse_expression();
            }
            auto close = expect_token(TokenType::close_bracket);
            return New<SliceExpression>(left, open, index, colon, interval_close, close);
        }
        auto close = expect_token(TokenType::close_bracket);
        return New<IndexExpression>(left, open, index, close);
    }

    if (token.type == TokenType::dot) {
        auto dot = token;
        if (next_token_is(TokenType::star)) {
            auto star = expect_token(TokenType::star);
            return New<DerefExpression>(left, dot, star);
        }
        auto identifier = parse_identifier();
        return New<SelectorExpression>(left, dot, identifier);
    }

    if (token.type == TokenType::open_brace) {
        assert(!lhs); // Should not be called with lhs

        auto is_type_literal = [](const Expression *expression) {
            switch (expression->kind) {
                using enum Expression::Kind;
                case TYPE:
                case IDENTIFIER:
                case SELECTOR: return true;
                default: return false;
            }
        };

        if (!is_type_literal(left)) {
            error(reporter, left, "Not a type literal");
        }

        Type *type = nullptr;
        if (left->is<IdentifierExpression>() || left->is<SelectorExpression>()) {
            auto type_path = expression_to_type_path(*this, left);
            if (type_path) {
                type = New<IdentifierType>(*type_path);
            }
        } else if (left->is<TypeExpression>()) {
            type = left->as<TypeExpression>()->type;
        }
        if (type == nullptr) {
            error(reporter, left, "Not a type");
            type = New<BadType>(left->start_token());
        }

        lexer.uneat_token();
        auto compound = parse_compound_expression(type);
        assert(compound->type);
        return compound;
    }

    auto op = token;
    auto right = parse_expression(token_type_to_precedense(token.type), lhs);
    return New<BinaryOperatorExpression>(left, op, right);
}

Expression *Parser::parse_expression(Precedence precedence, bool lhs) {
    auto *left = parse_unary_expression(lhs);

    while (precedence < token_type_to_precedense(lexer.next_token().type)) {
        if (next_token_is(TokenType::open_brace) && lhs) {
            return left;
        }
        left = parse_binary_expression(left, lhs);
    }

    return left;
}

Token Parser::expect_token(TokenType type) {
    auto token = lexer.next_token();
    lexer.eat_token();

    if (token.type != type && !invalid_statement_) {
        // invalid_statement_ = true;
        error(reporter, token, "Expected {}, got {}", type, token.type);
    }

    return token;
}

bool Parser::next_token_is(TokenType type) {
    return peek_token_is(type, 0);
}

bool Parser::peek_token_is(TokenType type, int peek) {
    return lexer.peek_token(peek).type == type;
}

}; // namespace Ast