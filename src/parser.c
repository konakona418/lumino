#include "parser.h"

#include <stdio.h>
#include <string.h>


#define _LM_BLOCK_SCOPE_VARIABLE_TABLE_INIT_SIZE 4
#define _LM_BLOCK_SCOPE_VARIABLE_TABLE_LOAD_FACTOR 0.7
#define _LM_BLOCK_SCOPE_VARIABLE_TABLE_GROWTH_FACTOR 2

void lm__parser_block_scope_variable_table_init(lm__parser_block_scope_variable_table_t* ht) {
    ht->size = 0;
    ht->capacity = _LM_BLOCK_SCOPE_VARIABLE_TABLE_INIT_SIZE;
    ht->entries = _LM_CALLOC(lm_string_t*, ht->capacity);
}

void lm__parser_block_scope_variable_table_deinit(lm__parser_block_scope_variable_table_t* ht) {
    for (size_t i = 0; i < ht->capacity; ++i) {
        if (ht->entries[i] != NULL) {
            lm_string_free(ht->entries[i]);
        }
    }
    _LM_FREE(ht->entries);
}

void lm__parser_block_scope_variable_table_realloc(lm__parser_block_scope_variable_table_t* ht) {
    size_t old_capacity = ht->capacity;
    lm_string_t** old_strings = ht->entries;

    ht->capacity *= _LM_BLOCK_SCOPE_VARIABLE_TABLE_GROWTH_FACTOR;
    ht->entries = _LM_CALLOC(lm_string_t*, ht->capacity);

    for (size_t i = 0; i < old_capacity; ++i) {
        if (old_strings[i] != NULL) {
            lm__parser_block_scope_variable_table_add_entry(ht, old_strings[i]);
            lm_string_free(old_strings[i]);
        }
    }

    _LM_FREE(old_strings);
}

void lm__parser_block_scope_variable_table_add_entry(lm__parser_block_scope_variable_table_t* ht, const lm_string_t* str) {
    if (ht->size + 1 >= ht->capacity * _LM_BLOCK_SCOPE_VARIABLE_TABLE_LOAD_FACTOR) {
        lm__parser_block_scope_variable_table_realloc(ht);
    }

    uint32_t hash = lm_string_hash(str);
    size_t mask = ht->capacity - 1;
    size_t idx = hash & mask;

    for (;;) {
        if (ht->entries[idx] == NULL) {
            ht->entries[idx] = lm_string_clone(str);
            ht->size++;

            break;
        } else if (lm_string_equal(ht->entries[idx], str)) {
            break;
        }
        idx = (idx + 1) & mask;
    }
}

lm_bool lm__parser_block_scope_variable_table_contains_entry(lm__parser_block_scope_variable_table_t* ht, const lm_string_t* str) {
    uint32_t hash = lm_string_hash(str);
    size_t mask = ht->capacity - 1;
    size_t idx = hash & mask;

    for (;;) {
        if (ht->entries[idx] == NULL) {
            return LM_FALSE;
        }
        if (lm_string_equal(ht->entries[idx], str)) {
            return LM_TRUE;
        }
        idx = (idx + 1) & mask;
    }
}

lm__parser_block_scope_t* lm__parser_block_scope_alloc(lm__parser_block_scope_type_t scope_type) {
    lm__parser_block_scope_t* scope = _LM_ALLOC(lm__parser_block_scope_t);
    scope->scope_type = scope_type;
    lm_list_node_init(&scope->list_node);
    lm__parser_block_scope_variable_table_init(&scope->variables);
    return scope;
}

void lm__parser_block_scope_free_iterator(lm_list_node_t* node, void* ctx) {
    lm__parser_block_scope_free(lm_list_entry(node, lm__parser_block_scope_t, list_node));
}

void lm__parser_block_scope_free(lm__parser_block_scope_t* scope) {
    lm__parser_block_scope_variable_table_deinit(&scope->variables);
    _LM_FREE(scope);
}

void lm__parser_block_scope_add_variable(lm__parser_block_scope_t* scope, const lm_string_t* name) {
    lm__parser_block_scope_variable_table_add_entry(&scope->variables, name);
}

lm_bool lm__parser_block_scope_has_variable(lm__parser_block_scope_t* scope, const lm_string_t* name) {
    return lm__parser_block_scope_variable_table_contains_entry(&scope->variables, name);
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

struct lm__parser_in_scope_iterator_data {
    lm__parser_block_scope_type_t desired_scope_type;
    lm_bool found;
};

lm_bool lm__parser_in_scope_iterator(lm_list_node_t* node, void* ctx) {
    lm__parser_block_scope_t* scope = lm_list_entry(node, lm__parser_block_scope_t, list_node);
    struct lm__parser_in_scope_iterator_data* data = _LM_CAST(struct lm__parser_in_scope_iterator_data, ctx);

    if (scope->scope_type == data->desired_scope_type) {
        data->found = LM_TRUE;
        return LM_PREDICATE_CONTINUE;
    } else if (scope->scope_type == LM__PARSER_BLOCK_SCOPE_TYPE_BLOCK ||
               scope->scope_type == LM__PARSER_BLOCK_SCOPE_TYPE_DONT_CARE) {
        return LM_PREDICATE_CONTINUE;
    }
    return LM_PREDICATE_STOP;
}

lm_bool lm__parser_in_scope(lm_parser_t* parser, lm__parser_block_scope_type_t type) {
    struct lm__parser_in_scope_iterator_data data = {type, LM_FALSE};
    lm_list_reverse_iterate_predicated(&parser->block_scope_head, lm__parser_in_scope_iterator, &data);
    return data.found;
}

void lm__parser_emit_scope_symbol(lm_parser_t* parser, const lm_string_t* name) {
    lm_list_node_t* tail = lm_list_tail(&parser->block_scope_head);
    lm__parser_block_scope_t* scope = lm_list_entry(tail, lm__parser_block_scope_t, list_node);

    lm__parser_block_scope_add_variable(scope, name);
}

struct lm__parser_find_var_data {
    lm_bool* has_var;
    const lm_string_t* name;
};

lm_bool lm__parser_is_symbol_defined_scope_rev_iterator(lm_list_node_t* node, void* ctx) {
    lm__parser_block_scope_t* scope = lm_list_entry(node, lm__parser_block_scope_t, list_node);
    struct lm__parser_find_var_data* data = ctx;

    if (lm__parser_block_scope_has_variable(scope, data->name)) {
        *data->has_var = LM_TRUE;
        return LM_FALSE;// stop rev iter
    }

    return LM_TRUE;// continue rev iter
}

lm_bool lm__parser_is_symbol_defined(lm_parser_t* parser, const lm_string_t* name) {
    struct lm__parser_find_var_data data;
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
    lm_list_node_t* head = lm_list_node_alloc();

    lm__parser_add_scope(parser, LM__PARSER_BLOCK_SCOPE_TYPE_BLOCK);
    lm__parser_consume(parser, LM_TOKEN_TYPE_L_CURLY_BRACKET);

    while (lm__parser_current(parser).type != LM_TOKEN_TYPE_R_CURLY_BRACKET) {
        lm__ast_statement_t* statement = lm__parser_parse_statement(parser);

        lm_list_add_tail(head, &statement->list_node);
    }

    lm__parser_consume(parser, LM_TOKEN_TYPE_R_CURLY_BRACKET);
    lm__parser_remove_scope(parser);

    lm__ast_block_t* block = lm__ast_block_alloc(head);

    return _LM_CAST(lm__ast_statement_t, block);
}

lm__ast_statement_t* lm__parser_parse_statement(lm_parser_t* parser) {
    lm__ast_statement_t* stmt = NULL;

    switch (lm__parser_current(parser).type) {
        case LM_TOKEN_TYPE_KEYWORD_LET:
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
        case LM_TOKEN_TYPE_KEYWORD_IMPORT:
            stmt = lm__parser_parse_import_statement(parser);
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
    lm__ast_expression_t* lhs = lm__parser_parse_simple_expression(parser);

    if (lm__parser_current(parser).type == LM_TOKEN_TYPE_OP_ASSIGN) {
        lm__parser_next(parser);
        lm__ast_expression_t* rhs = lm__parser_parse_simple_expression(parser);

        lm__ast_assign_expr_t* assign_expr =
                lm__ast_assign_expr_alloc(LM_AST_ASSIGN_EXPR_TYPE_NORMAL, lhs, rhs);
        assign_expr->result_discardable = LM_FALSE;

        return _LM_CAST(lm__ast_expression_t, assign_expr);
    } else if (lm__parser_current(parser).type == LM_TOKEN_TYPE_OP_INCREASE_BY ||
               lm__parser_current(parser).type == LM_TOKEN_TYPE_OP_DECREASE_BY ||
               lm__parser_current(parser).type == LM_TOKEN_TYPE_OP_MULTIPLY_BY ||
               lm__parser_current(parser).type == LM_TOKEN_TYPE_OP_DIVIDE_BY) {
        lm_token_type_t op_type = lm__parser_current(parser).type;

        lm__parser_next(parser);
        lm__ast_expression_t* rhs = lm__parser_parse_simple_expression(parser);

        lm__ast_assign_expr_type_t assign_type;
        switch (op_type) {
            case LM_TOKEN_TYPE_OP_INCREASE_BY:
                assign_type = LM_AST_ASSIGN_EXPR_TYPE_ADD;
                break;
            case LM_TOKEN_TYPE_OP_DECREASE_BY:
                assign_type = LM_AST_ASSIGN_EXPR_TYPE_SUB;
                break;
            case LM_TOKEN_TYPE_OP_MULTIPLY_BY:
                assign_type = LM_AST_ASSIGN_EXPR_TYPE_MUL;
                break;
            case LM_TOKEN_TYPE_OP_DIVIDE_BY:
                assign_type = LM_AST_ASSIGN_EXPR_TYPE_DIV;
                break;
            default:
                _LM_ASSERT(0, "error branch reached");
                break;
        }

        lm__ast_assign_expr_t* assign_expr = lm__ast_assign_expr_alloc(assign_type, lhs, rhs);
        assign_expr->result_discardable = LM_FALSE;

        return _LM_CAST(lm__ast_expression_t, assign_expr);
    }

    lhs->result_discardable = LM_TRUE;

    return lhs;
}

lm__ast_expression_t* lm__parser_parse_simple_expression(lm_parser_t* parser) {
    return lm__parser_parse_logical_and_expression(parser);
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
    lm__ast_expression_t* lhs = lm__parser_parse_unary_expression(parser);

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
        lm__ast_expression_t* rhs = lm__parser_parse_unary_expression(parser);

        return _LM_CAST(lm__ast_expression_t, lm__ast_binary_expr_alloc(type, lhs, rhs));
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
        case LM_TOKEN_TYPE_OP_LOGICAL_NOT: {
            lm__parser_next(parser);
            rhs = lm__parser_parse_primary(parser);

            return _LM_CAST(lm__ast_expression_t, lm__ast_unary_expr_alloc(LM_AST_UNARY_EXPR_TYPE_NOT, rhs));
            break;
        }
        default:
            break;
    }

    rhs = lm__parser_parse_primary(parser);

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

                lm_string_free(token.value);
                lm__parser_emit_error(parser, buf);
            } else {
                expr = _LM_CAST(lm__ast_expression_t,
                                lm__ast_var_expr_alloc(token.value));
            }
            break;
        }
        case LM_TOKEN_TYPE_INT_LITERAL: {
            lm_int int_val;
            if (lm_string_stoi(token.value, &int_val)) {
                expr = _LM_CAST(lm__ast_expression_t,
                                lm__ast_literal_expr_alloc(LM_AST_LITERAL_EXPR_TYPE_INT,
                                                           (lm__ast_literal_expr_data_t){.int_val = int_val}));
            } else {
                lm__parser_emit_error(parser, "not a valid int literal");
            }
            lm_string_free(token.value);
            break;
        }
        case LM_TOKEN_TYPE_FLOAT_LITERAL: {
            lm_float float_val;
            if (lm_string_stof(token.value, &float_val)) {
                expr = _LM_CAST(lm__ast_expression_t,
                                lm__ast_literal_expr_alloc(LM_AST_LITERAL_EXPR_TYPE_INT,
                                                           (lm__ast_literal_expr_data_t){.float_val = float_val}));
            } else {
                lm__parser_emit_error(parser, "not a valid float literal");
            }
            lm_string_free(token.value);
            break;
        }
        case LM_TOKEN_TYPE_STRING_LITERAL: {
            lm_string_t* sub = lm_string_substr(token.value, 1,
                                                lm_string_len(token.value) - 2);
            lm_string_free(token.value);

            expr = _LM_CAST(lm__ast_expression_t,
                            lm__ast_literal_expr_alloc(LM_AST_LITERAL_EXPR_TYPE_STRING,
                                                       (lm__ast_literal_expr_data_t){.str_val = sub}));
            break;
        }
        case LM_TOKEN_TYPE_KEYWORD_TRUE: {
            expr = _LM_CAST(lm__ast_expression_t,
                            lm__ast_literal_expr_alloc(LM_AST_LITERAL_EXPR_TYPE_BOOL,
                                                       (lm__ast_literal_expr_data_t){.bool_val = LM_TRUE}));
            break;
        }
        case LM_TOKEN_TYPE_KEYWORD_FALSE: {
            expr = _LM_CAST(lm__ast_expression_t,
                            lm__ast_literal_expr_alloc(LM_AST_LITERAL_EXPR_TYPE_BOOL,
                                                       (lm__ast_literal_expr_data_t){.bool_val = LM_FALSE}));
            break;
        }
        case LM_TOKEN_TYPE_KEYWORD_NULL: {
            expr = _LM_CAST(lm__ast_expression_t,
                            lm__ast_literal_expr_alloc(LM_AST_LITERAL_EXPR_TYPE_NULL, (lm__ast_literal_expr_data_t){}));
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
    lm__parser_consume(parser, LM_TOKEN_TYPE_KEYWORD_LET);

    lm_token_t name = lm__parser_consume(parser, LM_TOKEN_TYPE_IDENTIFIER);

    lm__parser_emit_scope_symbol(parser, name.value);

    if (lm__parser_current(parser).type == LM_TOKEN_TYPE_OP_ASSIGN) {
        lm__parser_consume(parser, LM_TOKEN_TYPE_OP_ASSIGN);
        lm__ast_statement_t* value = _LM_CAST(lm__ast_statement_t, lm__parser_parse_simple_expression(parser));

        lm__parser_consume(parser, LM_TOKEN_TYPE_SEMICOLON);

        return _LM_CAST(lm__ast_statement_t, lm__ast_decl_alloc(name.value, value));
    }

    lm__parser_consume(parser, LM_TOKEN_TYPE_SEMICOLON);

    return _LM_CAST(lm__ast_statement_t, lm__ast_decl_alloc(name.value, NULL));
}

lm__ast_statement_t* lm__parser_parse_if_statement(lm_parser_t* parser) {
    lm__parser_consume(parser, LM_TOKEN_TYPE_KEYWORD_IF);
    lm__parser_consume(parser, LM_TOKEN_TYPE_L_PARENTHESIS);

    lm__ast_expression_t* cond = lm__parser_parse_simple_expression(parser);

    lm__parser_consume(parser, LM_TOKEN_TYPE_R_PARENTHESIS);

    lm__ast_statement_t* then_body = NULL;
    if (lm__parser_current(parser).type == LM_TOKEN_TYPE_L_CURLY_BRACKET) {
        then_body = lm__parser_parse_block(parser);
    } else {
        then_body = lm__parser_parse_statement(parser);
    }

    lm__ast_statement_t* else_body = NULL;
    if (lm__parser_current(parser).type == LM_TOKEN_TYPE_KEYWORD_ELSE) {
        lm__parser_consume(parser, LM_TOKEN_TYPE_KEYWORD_ELSE);
        if (lm__parser_current(parser).type == LM_TOKEN_TYPE_L_CURLY_BRACKET) {
            else_body = lm__parser_parse_block(parser);
        } else {
            else_body = lm__parser_parse_statement(parser);
        }
    }

    lm__ast_if_t* stmt = lm__ast_if_alloc(_LM_CAST(lm__ast_statement_t, cond), then_body, else_body);

    return _LM_CAST(lm__ast_statement_t, stmt);
}

lm__ast_statement_t* lm__parser_parse_while_statement(lm_parser_t* parser) {
    lm__parser_consume(parser, LM_TOKEN_TYPE_KEYWORD_WHILE);

    lm__parser_add_scope(parser, LM__PARSER_BLOCK_SCOPE_TYPE_LOOP);

    lm__parser_consume(parser, LM_TOKEN_TYPE_L_PARENTHESIS);
    lm__ast_statement_t* cond = _LM_CAST(lm__ast_statement_t, lm__parser_parse_simple_expression(parser));
    lm__parser_consume(parser, LM_TOKEN_TYPE_R_PARENTHESIS);

    lm__ast_statement_t* body = NULL;

    if (lm__parser_current(parser).type == LM_TOKEN_TYPE_L_CURLY_BRACKET) {
        body = lm__parser_parse_block(parser);
    } else {
        body = lm__parser_parse_statement(parser);
    }

    lm__parser_remove_scope(parser);

    return _LM_CAST(lm__ast_statement_t, lm__ast_while_alloc(cond, body));
}

lm__ast_statement_t* lm__parser_parse_for_statement(lm_parser_t* parser) {
    return NULL;
}

lm__ast_statement_t* lm__parser_parse_break_statement(lm_parser_t* parser) {
    if (!lm__parser_in_scope(parser, LM__PARSER_BLOCK_SCOPE_TYPE_LOOP)) {
        lm__parser_emit_error(parser, "break statement can only be used inside a loop");
        return NULL;
    }

    lm__parser_consume(parser, LM_TOKEN_TYPE_KEYWORD_BREAK);
    return _LM_CAST(lm__ast_statement_t, lm__ast_break_alloc());
}

lm__ast_statement_t* lm__parser_parse_continue_statement(lm_parser_t* parser) {
    if (!lm__parser_in_scope(parser, LM__PARSER_BLOCK_SCOPE_TYPE_LOOP)) {
        lm__parser_emit_error(parser, "continue statement can only be used inside a loop");
        return NULL;
    }

    lm__parser_consume(parser, LM_TOKEN_TYPE_KEYWORD_CONTINUE);
    return _LM_CAST(lm__ast_statement_t, lm__ast_continue_alloc());
}

lm__ast_statement_t* lm__parser_parse_return_statement(lm_parser_t* parser) {
    return NULL;
}

lm__ast_statement_t* lm__parser_parse_func_statement(lm_parser_t* parser) {
    return NULL;
}

lm__ast_statement_t* lm__parser_parse_import_statement(lm_parser_t* parser) {
    return NULL;
}
