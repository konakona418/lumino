#include "parser.h"

#include <stdio.h>
#include <string.h>


lm__parser_block_scope_variable_t* lm__parser_block_scope_variable_alloc(const lm_string_t* name) {
    lm__parser_block_scope_variable_t* var = _LM_ALLOC(lm__parser_block_scope_variable_t);
    var->name = name;
    lm_list_node_init(&var->list_node);
    return var;
}

void lm__parser_block_scope_variable_free_iterator(lm_list_node_t* node, void* ctx) {
    lm__parser_block_scope_variable_free(lm_list_entry(node, lm__parser_block_scope_variable_t, list_node));
}

void lm__parser_block_scope_variable_free(lm__parser_block_scope_variable_t* var) {
    _LM_FREE(var);
}

lm__parser_block_scope_t* lm__parser_block_scope_alloc(lm__parser_block_scope_type_t scope_type) {
    lm__parser_block_scope_t* scope = _LM_ALLOC(lm__parser_block_scope_t);
    scope->scope_type = scope_type;
    lm_list_node_init(&scope->list_node);
    lm_list_node_init(&scope->variables_head);
    return scope;
}

void lm__parser_block_scope_free_iterator(lm_list_node_t* node, void* ctx) {
    lm__parser_block_scope_free(lm_list_entry(node, lm__parser_block_scope_t, list_node));
}

void lm__parser_block_scope_free(lm__parser_block_scope_t* scope) {
    lm_list_iterate_safe(&scope->variables_head, lm__parser_block_scope_variable_free_iterator, NULL);
    _LM_FREE(scope);
}

void lm__parser_block_scope_add_variable(lm__parser_block_scope_t* scope, const lm_string_t* name) {
    lm__parser_block_scope_variable_t* var = lm__parser_block_scope_variable_alloc(name);
    lm_list_add_tail(&scope->variables_head, &var->list_node);
}

struct lm__parser_block_scope_find_var_data {
    lm_bool* has_var;
    const lm_string_t* name;
};

void lm__parser_block_scope_has_variable_compare_iterator(lm_list_node_t* node, void* ctx) {
    lm__parser_block_scope_variable_t* var = lm_list_entry(node, lm__parser_block_scope_variable_t, list_node);
    struct lm__parser_block_scope_find_var_data* data = ctx;

    if (lm_string_equal(var->name, data->name)) {
        *data->has_var = LM_TRUE;
    }
}

lm_bool lm__parser_block_scope_has_variable(lm__parser_block_scope_t* scope, const lm_string_t* name) {
    struct lm__parser_block_scope_find_var_data data;

    lm_bool has_var = LM_FALSE;
    data.name = name;
    data.has_var = &has_var;

    lm_list_iterate(&scope->variables_head,
                    lm__parser_block_scope_has_variable_compare_iterator, &data);

    return has_var;
}

const char* lm__parser_error_what(lm_error_t* error) {
    return ((lm_parser_error_t*) error)->msg->data;
}

void lm__parser_error_free(lm_error_t* error) {
    lm_string_free(((lm_parser_error_t*) error)->msg);
    _LM_FREE(error);
}

lm_parser_error_t* lm_parser_error_alloc(const char* msg, lm_parser_t* parser) {
    lm_parser_error_t* parser_error = _LM_ALLOC(lm_parser_error_t);
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
    lm_parser_t* parser = _LM_ALLOC(lm_parser_t);
    parser->lexer = lexer;

    lm_list_node_init(&parser->block_scope_head);

    lm__parser_next(parser);// read first token

    return parser;
}

void lm_parser_free(lm_parser_t* parser) {
    lm_list_iterate_safe(&parser->block_scope_head, lm__parser_block_scope_free_iterator, NULL);

    _LM_FREE(parser);
}

lm__ast_program_t* lm_parser_parse(lm_parser_t* parser) {
    lm_list_node_t* head = lm_list_node_alloc();

    lm__parser_add_scope(parser, LM__PARSER_BLOCK_SCOPE_TYPE_PROGRAM);

    while (lm__parser_current(parser).type != LM_TOKEN_TYPE_TERMINATOR) {
        lm__ast_statement_t* statement = lm__parser_parse_statement(parser);

        lm_list_add_tail(head, &statement->list_node);
    }

    lm__ast_program_t* program = lm__ast_program_alloc(head);

    lm__parser_remove_scope(parser);

    return program;
}

void lm__parser_add_scope(lm_parser_t* parser, lm__parser_block_scope_type_t type) {
    if (type == LM__PARSER_BLOCK_SCOPE_TYPE_NONE) {
        type = LM__PARSER_BLOCK_SCOPE_TYPE_DONT_CARE;
    }

    lm__parser_block_scope_t* scope = lm__parser_block_scope_alloc(type);
    lm_list_add_tail(&parser->block_scope_head, &scope->list_node);
}

void lm__parser_remove_scope(lm_parser_t* parser) {
    lm_list_node_t* tail = lm_list_tail(&parser->block_scope_head);
    lm__parser_block_scope_t* scope = lm_list_entry(tail, lm__parser_block_scope_t, list_node);

    lm_list_remove(tail);

    lm__parser_block_scope_free(scope);
}

void lm__parser_emit_scope_symbol(lm_parser_t* parser, const lm_string_t* name) {
    lm_list_node_t* tail = lm_list_tail(&parser->block_scope_head);
    lm__parser_block_scope_t* scope = lm_list_entry(tail, lm__parser_block_scope_t, list_node);

    lm__parser_block_scope_add_variable(scope, name);
}

lm_bool lm__parser_is_symbol_defined_scope_rev_iterator(lm_list_node_t* node, void* ctx) {
    lm__parser_block_scope_t* scope = lm_list_entry(node, lm__parser_block_scope_t, list_node);
    struct lm__parser_block_scope_find_var_data* data = ctx;

    if (lm__parser_block_scope_has_variable(scope, data->name)) {
        *data->has_var = LM_TRUE;
        return LM_FALSE;// stop rev iter
    }

    return LM_TRUE;// continue rev iter
}

lm_bool lm__parser_is_symbol_defined(lm_parser_t* parser, const lm_string_t* name) {
    struct lm__parser_block_scope_find_var_data data;
    lm_bool has_var = LM_FALSE;
    data.has_var = &has_var;
    data.name = name;

    lm_list_reverse_iterate_predicated(&parser->block_scope_head,
                                       lm__parser_is_symbol_defined_scope_rev_iterator, &data);

    return has_var;
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
    lm__ast_expression_t* lhs = lm__parser_parse_logical_and_expression(parser);

    if (lm__parser_current(parser).type == LM_TOKEN_TYPE_OP_ASSIGN) {
        lm__parser_next(parser);
        lm__ast_expression_t* rhs = lm__parser_parse_logical_and_expression(parser);

        return _LM_CAST(lm__ast_expression_t, lm__ast_assign_expr_alloc(lhs, rhs));
    }

    return lhs;
}

lm__ast_expression_t* lm__parser_parse_logical_and_expression(lm_parser_t* parser) {
    lm__ast_expression_t* lhs = lm__parser_parse_logical_or_expression(parser);

    while (lm__parser_current(parser).type == LM_TOKEN_TYPE_OP_LOGICAL_AND) {
        lm__parser_next(parser);
        lm__ast_expression_t* rhs = lm__parser_parse_logical_or_expression(parser);

        return _LM_CAST(lm__ast_expression_t,
                        lm__ast_binary_expr_alloc(LM_AST_BINARY_EXPR_TYPE_LAND, lhs, rhs));
    }

    return lhs;
}

lm__ast_expression_t* lm__parser_parse_logical_or_expression(lm_parser_t* parser) {
    lm__ast_expression_t* lhs = lm__parser_parse_comparative_expression(parser);

    while (lm__parser_current(parser).type == LM_TOKEN_TYPE_OP_LOGICAL_OR) {
        lm__parser_next(parser);
        lm__ast_expression_t* rhs = lm__parser_parse_comparative_expression(parser);

        return _LM_CAST(lm__ast_expression_t,
                        lm__ast_binary_expr_alloc(LM_AST_BINARY_EXPR_TYPE_LOR, lhs, rhs));
    }

    return lhs;
}

lm__ast_expression_t* lm__parser_parse_comparative_expression(lm_parser_t* parser) {
    lm__ast_expression_t* lhs = lm__parser_parse_relational_expression(parser);

    while (lm__parser_current(parser).type == LM_TOKEN_TYPE_OP_EQUAL ||
           lm__parser_current(parser).type == LM_TOKEN_TYPE_OP_NOT_EQUAL) {
        lm__ast_binary_expr_type_t type =
                lm__parser_current(parser).type == LM_TOKEN_TYPE_OP_EQUAL
                        ? LM_AST_BINARY_EXPR_TYPE_EQ
                        : LM_AST_BINARY_EXPR_TYPE_NEQ;

        lm__parser_next(parser);
        lm__ast_expression_t* rhs = lm__parser_parse_relational_expression(parser);

        return _LM_CAST(lm__ast_expression_t,
                        lm__ast_binary_expr_alloc(type, lhs, rhs));
    }

    return lhs;
}

lm__ast_expression_t* lm__parser_parse_relational_expression(lm_parser_t* parser) {
    lm__ast_expression_t* lhs = lm__parser_parse_additive_expression(parser);

    while (lm__parser_current(parser).type == LM_TOKEN_TYPE_OP_LESS_THAN ||
           lm__parser_current(parser).type == LM_TOKEN_TYPE_OP_LESS_THAN_EQUAL ||
           lm__parser_current(parser).type == LM_TOKEN_TYPE_OP_GREATER_THAN ||
           lm__parser_current(parser).type == LM_TOKEN_TYPE_OP_GREATER_THAN_EQUAL) {
        lm__ast_binary_expr_type_t type;

        switch (lm__parser_current(parser).type) {
            case LM_TOKEN_TYPE_OP_LESS_THAN:
                type = LM_AST_BINARY_EXPR_TYPE_LT;
                break;
            case LM_TOKEN_TYPE_OP_LESS_THAN_EQUAL:
                type = LM_AST_BINARY_EXPR_TYPE_LTE;
                break;
            case LM_TOKEN_TYPE_OP_GREATER_THAN:
                type = LM_AST_BINARY_EXPR_TYPE_GT;
                break;
            case LM_TOKEN_TYPE_OP_GREATER_THAN_EQUAL:
                type = LM_AST_BINARY_EXPR_TYPE_GTE;
                break;
            default:
                break;
        }

        lm__parser_next(parser);
        lm__ast_expression_t* rhs = lm__parser_parse_additive_expression(parser);

        return _LM_CAST(lm__ast_expression_t,
                        lm__ast_binary_expr_alloc(type, lhs, rhs));
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
            if (!lm__parser_is_symbol_defined(parser, token.value)) {
                int buf_size = snprintf(NULL, 0, "undefined symbol: %s", token.value->data);
                char buf[buf_size + 1];
                snprintf(buf, buf_size + 1, "undefined symbol: %s", token.value->data);
                lm__parser_emit_error(parser, buf);
            } else {
                expr = _LM_CAST(lm__ast_expression_t,
                                lm__ast_var_expr_alloc(token.value));
            }
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

    lm__parser_emit_scope_symbol(parser, name.value);

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
