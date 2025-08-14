#include "parser.h"

#include <stdio.h>
#include <string.h>

const char* lm__parser_error_what(lm_error_t* error) {
    return ((lm_parser_error_t*) error)->msg->data;
}

void lm__parser_error_free(lm_error_t* error) {
    lm_string_free(((lm_parser_error_t*) error)->msg);
    lm__free(error);
}

lm_parser_error_t* lm_parser_error_alloc(const char* msg, lm_parser_t* parser) {
    lm_parser_error_t* parser_error = lm__alloc(sizeof(lm_lexer_error_t));
    parser_error->vtbl.free = lm__parser_error_free;
    parser_error->vtbl.what = lm__parser_error_what;


    const lm__lexer_stats_t* stats = lm_lexer_get_stats(parser->lexer);

    size_t buf_size = snprintf(NULL, 0, "Parser error: %s, at file %s line %zu col %zu",
                               msg, stats->file->data, stats->line, stats->column);
    char* buf = _LM_ALLOC_ARRAY(char, buf_size + 1);

    int n = snprintf(buf, buf_size + 1, "Parser error: %s, at file %s line %zu col %zu",
                     msg, stats->file->data, stats->line, stats->column);

    _LM_ASSERT(n >= 0, "snprintf failed");

    parser_error->msg = lm_string_from(buf, 0);

    return parser_error;
}

lm_parser_t* lm_parser_alloc(lm_lexer_t* lexer) {
    lm_parser_t* parser = lm__alloc(sizeof(lm_parser_t));
    parser->lexer = lexer;

    lm__parser_next(parser);// read first token

    return parser;
}

void lm_parser_free(lm_parser_t* parser) {
    lm__free(parser);
}

lm__ast_program_t* lm_parser_parse(lm_parser_t* parser) {
    lm_list_node_t head;
    lm_list_node_init(&head);

    while (lm__parser_current(parser).type != LM_TOKEN_TYPE_TERMINATOR) {
        lm__ast_statement_t* statement = lm__parser_parse_statement(parser);
        lm_list_node_init(&statement->list_node);

        lm_list_add_tail(&head, &statement->list_node);
    }

    lm__ast_program_t* program = lm__ast_program_alloc(head);
    return program;
}

void lm__parser_emit_error(lm_parser_t* parser, const char* msg) {
    if (!parser->error_handler) {
        return;
    }

    lm_parser_error_t* error = lm_parser_error_alloc(msg, parser);
    parser->error_handler((lm_error_t*) error);
}


lm_token_t lm__parser_consume(lm_parser_t* parser, lm_token_type_t expected_type) {
    lm_token_t token = lm__parser_current(parser);

    if (token.type != expected_type) {
        size_t buf_size = snprintf(
                NULL, 0, "expected token of type %s, got %s",
                lm_token_type_to_string(expected_type), lm_token_type_to_string(token.type));

        char* buf = _LM_ALLOC_ARRAY(char, buf_size + 1);
        snprintf(buf, buf_size + 1, "expected token of type %s, got %s",
                 lm_token_type_to_string(expected_type), lm_token_type_to_string(token.type));

        lm__parser_emit_error(parser, buf);

        return token;
    }

    lm__parser_next(parser);

    return token;
}

void lm__parser_next(lm_parser_t* parser) {
    parser->current_token = lm_lexer_next_token(parser->lexer);
}

lm_token_t lm__parser_current(lm_parser_t* parser) {
    return parser->current_token;
}

lm__ast_statement_t* lm__parser_parse_block(lm_parser_t* parser) {
    return NULL;
}

lm__ast_statement_t* lm__parser_parse_statement(lm_parser_t* parser) {
    lm__ast_statement_t* stmt = NULL;

    switch (lm__parser_current(parser).type) {
        case LM_TOKEN_TYPE_KEYWORD_VAR:
            stmt = lm__parser_parse_decl(parser);
            break;
        case LM_TOKEN_TYPE_KEYWORD_IF:
            stmt = lm__parser_parse_if_statement(parser);
            break;
        case LM_TOKEN_TYPE_KEYWORD_ELSE:
            _LM_ASSERT(0, "else without if");
            break;
        case LM_TOKEN_TYPE_KEYWORD_WHILE:
            stmt = lm__parser_parse_while_statement(parser);
            break;
        case LM_TOKEN_TYPE_KEYWORD_BREAK:
            stmt = lm__parser_parse_break_statement(parser);
            break;
        case LM_TOKEN_TYPE_KEYWORD_CONTINUE:
            stmt = lm__parser_parse_continue_statement(parser);
            break;
        case LM_TOKEN_TYPE_KEYWORD_FOR:
            stmt = lm__parser_parse_for_statement(parser);
            break;
        case LM_TOKEN_TYPE_KEYWORD_FUNC:
            stmt = lm__parser_parse_func_statement(parser);
            break;
        case LM_TOKEN_TYPE_KEYWORD_RETURN:
            stmt = lm__parser_parse_return_statement(parser);
            break;
        case LM_TOKEN_TYPE_KEYWORD_INCLUDE:
            stmt = lm__parser_parse_include_statement(parser);
            break;
        case LM_TOKEN_TYPE_KEYWORD_DEFINE:
            stmt = lm__parser_parse_define_statement(parser);
            break;
        default:
            stmt = (lm__ast_statement_t*) lm__parser_parse_expression(parser);
            lm__parser_consume(parser, LM_TOKEN_TYPE_SEMICOLON);
            break;
    }

    _LM_ASSERT_NOT_NULL(stmt, "statement cannot be null");

    while (lm__parser_current(parser).type == LM_TOKEN_TYPE_SEMICOLON) {
        lm__parser_next(parser);
    }

    return stmt;
}

lm__ast_expression_t* lm__parser_parse_expression(lm_parser_t* parser) {
    return lm__parser_parse_assign_expression(parser);
}

lm__ast_expression_t* lm__parser_parse_assign_expression(lm_parser_t* parser) {
    lm__ast_expression_t* lhs = lm__parser_parse_additive_expression(parser);

    if (lm__parser_current(parser).type == LM_TOKEN_TYPE_OP_ASSIGN) {
        lm__parser_next(parser);
        lm__ast_expression_t* rhs = lm__parser_parse_additive_expression(parser);

        return _LM_CAST(lm__ast_expression_t, lm__ast_assign_expr_alloc(lhs, rhs));
    }

    return lhs;
}

lm__ast_expression_t* lm__parser_parse_additive_expression(lm_parser_t* parser) {
    lm__ast_expression_t* lhs = lm__parser_parse_multiplicative_expression(parser);

    while (lm__parser_current(parser).type == LM_TOKEN_TYPE_OP_ADD ||
           lm__parser_current(parser).type == LM_TOKEN_TYPE_OP_SUB) {
        lm__ast_binary_expr_type_t type =
                lm__parser_current(parser).type == LM_TOKEN_TYPE_OP_ADD
                        ? LM_AST_BINARY_EXPR_TYPE_ADD
                        : LM_AST_BINARY_EXPR_TYPE_SUB;

        lm__parser_next(parser);
        lm__ast_expression_t* rhs = lm__parser_parse_multiplicative_expression(parser);

        return _LM_CAST(lm__ast_expression_t, lm__ast_binary_expr_alloc(type, lhs, rhs));
    }

    return lhs;
}

lm__ast_expression_t* lm__parser_parse_multiplicative_expression(lm_parser_t* parser) {
    lm__ast_expression_t* lhs = lm__parser_parse_power_expression(parser);

    while (lm__parser_current(parser).type == LM_TOKEN_TYPE_OP_MUL ||
           lm__parser_current(parser).type == LM_TOKEN_TYPE_OP_DIV ||
           lm__parser_current(parser).type == LM_TOKEN_TYPE_OP_MOD) {
        lm__ast_binary_expr_type_t type;

        switch (lm__parser_current(parser).type) {
            case LM_TOKEN_TYPE_OP_MUL:
                type = LM_AST_BINARY_EXPR_TYPE_MUL;
                break;
            case LM_TOKEN_TYPE_OP_DIV:
                type = LM_AST_BINARY_EXPR_TYPE_DIV;
                break;
            case LM_TOKEN_TYPE_OP_MOD:
                type = LM_AST_BINARY_EXPR_TYPE_MOD;
                break;
            default:
                break;
        }

        lm__parser_next(parser);
        lm__ast_expression_t* rhs = lm__parser_parse_power_expression(parser);

        return _LM_CAST(lm__ast_expression_t, lm__ast_binary_expr_alloc(type, lhs, rhs));
    }

    return lhs;
}

lm__ast_expression_t* lm__parser_parse_power_expression(lm_parser_t* parser) {
    lm__ast_expression_t* lhs = lm__parser_parse_unary_expression(parser);

    while (lm__parser_current(parser).type == LM_TOKEN_TYPE_OP_POW) {
        lm__parser_next(parser);
        lm__ast_expression_t* rhs = lm__parser_parse_power_expression(parser);

        return _LM_CAST(
                lm__ast_expression_t,
                lm__ast_binary_expr_alloc(
                        LM_AST_BINARY_EXPR_TYPE_POW,
                        lhs, rhs));
    }

    return lhs;
}

lm__ast_expression_t* lm__parser_parse_unary_expression(lm_parser_t* parser) {
    lm__ast_expression_t* rhs = NULL;

    switch (lm__parser_current(parser).type) {
        case LM_TOKEN_TYPE_OP_SUB: {
            lm__parser_next(parser);
            rhs = lm__parser_parse_primary(parser);

            return _LM_CAST(lm__ast_expression_t, lm__ast_unary_expr_alloc(LM_AST_UNARY_EXPR_TYPE_NEG, rhs));
        }
        case LM_TOKEN_TYPE_OP_ADD: {
            lm__parser_next(parser);
            break;
        }
        default:
            break;
    }

    rhs = lm__parser_parse_primary(parser);

    // todo: bang(!) operator

    return rhs;
}

lm__ast_expression_t* lm__parser_parse_primary(lm_parser_t* parser) {
    lm__ast_expression_t* expr = NULL;

    lm_token_t token = lm__parser_current(parser);
    switch (token.type) {
        case LM_TOKEN_TYPE_IDENTIFIER: {
            expr = _LM_CAST(lm__ast_expression_t,
                            lm__ast_var_expr_alloc(token.value));
            break;
        }
        case LM_TOKEN_TYPE_NUMBER_LITERAL: {
            expr = _LM_CAST(lm__ast_expression_t,
                            lm__ast_literal_expr_alloc(LM_AST_LITERAL_EXPR_TYPE_INT, token.value));
            break;
        }
        case LM_TOKEN_TYPE_STRING_LITERAL: {
            expr = _LM_CAST(lm__ast_expression_t,
                            lm__ast_literal_expr_alloc(LM_AST_LITERAL_EXPR_TYPE_STRING, token.value));
            break;
        }
        case LM_TOKEN_TYPE_KEYWORD_TRUE: {
            expr = _LM_CAST(lm__ast_expression_t,
                            lm__ast_literal_expr_alloc(LM_AST_LITERAL_EXPR_TYPE_BOOL, token.value));
            break;
        }
        case LM_TOKEN_TYPE_KEYWORD_FALSE: {
            expr = _LM_CAST(lm__ast_expression_t,
                            lm__ast_literal_expr_alloc(LM_AST_LITERAL_EXPR_TYPE_BOOL, token.value));
            break;
        }
        case LM_TOKEN_TYPE_KEYWORD_NULL: {
            expr = _LM_CAST(lm__ast_expression_t,
                            lm__ast_literal_expr_alloc(LM_AST_LITERAL_EXPR_TYPE_NULL, token.value));
            break;
        }
        case LM_TOKEN_TYPE_L_PARENTHESIS: {
            lm__parser_next(parser);
            expr = lm__parser_parse_expression(parser);
            lm__parser_consume(parser, LM_TOKEN_TYPE_R_PARENTHESIS);

            break;
        }
        default:
            lm__parser_emit_error(parser, "unexpected token");
            return expr;
    }

    lm__parser_next(parser);

    return expr;
}


lm__ast_statement_t* lm__parser_parse_decl(lm_parser_t* parser) {
    lm__parser_consume(parser, LM_TOKEN_TYPE_KEYWORD_VAR);

    lm_token_t name = lm__parser_consume(parser, LM_TOKEN_TYPE_IDENTIFIER);

    if (lm__parser_current(parser).type == LM_TOKEN_TYPE_OP_ASSIGN) {
        lm__parser_consume(parser, LM_TOKEN_TYPE_OP_ASSIGN);
        lm__ast_statement_t* value = _LM_CAST(lm__ast_statement_t, lm__parser_parse_expression(parser));

        lm__parser_consume(parser, LM_TOKEN_TYPE_SEMICOLON);

        return _LM_CAST(lm__ast_statement_t, lm__ast_decl_alloc(name.value, value));
    }

    lm__parser_consume(parser, LM_TOKEN_TYPE_SEMICOLON);

    return _LM_CAST(lm__ast_statement_t, lm__ast_decl_alloc(name.value, NULL));
}

lm__ast_statement_t* lm__parser_parse_if_statement(lm_parser_t* parser) {
    return NULL;
}

lm__ast_statement_t* lm__parser_parse_while_statement(lm_parser_t* parser) {
    return NULL;
}

lm__ast_statement_t* lm__parser_parse_for_statement(lm_parser_t* parser) {
    return NULL;
}

lm__ast_statement_t* lm__parser_parse_break_statement(lm_parser_t* parser) {
    return NULL;
}

lm__ast_statement_t* lm__parser_parse_continue_statement(lm_parser_t* parser) {
    return NULL;
}

lm__ast_statement_t* lm__parser_parse_return_statement(lm_parser_t* parser) {
    return NULL;
}

lm__ast_statement_t* lm__parser_parse_func_statement(lm_parser_t* parser) {
    return NULL;
}

lm__ast_statement_t* lm__parser_parse_include_statement(lm_parser_t* parser) {
    return NULL;
}

lm__ast_statement_t* lm__parser_parse_define_statement(lm_parser_t* parser) {
    return NULL;
}
