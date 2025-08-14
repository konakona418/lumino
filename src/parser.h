#pragma once

#include "ast.h"
#include "common.h"
#include "lexer.h"


struct lm_parser_s;

typedef struct lm_parser_error_s {
    LM_ERROR_HEADER
    lm_string_t* msg;
} lm_parser_error_t;

const char* lm__parser_error_what(lm_error_t* error);

void lm__parser_error_free(lm_error_t* error);

lm_parser_error_t* lm_parser_error_alloc(const char* msg, struct lm_parser_s* parser);

typedef enum lm__parser_block_scope_type_e {
    LM__PARSER_BLOCK_SCOPE_TYPE_NONE = 0,
    LM__PARSER_BLOCK_SCOPE_TYPE_DONT_CARE,
    LM__PARSER_BLOCK_SCOPE_TYPE_PROGRAM,
    LM__PARSER_BLOCK_SCOPE_TYPE_BLOCK,
} lm__parser_block_scope_type_t;

typedef struct lm__parser_block_scope_variable_s {
    lm_list_node_t list_node;

    const lm_string_t* name;
} lm__parser_block_scope_variable_t;

lm__parser_block_scope_variable_t* lm__parser_block_scope_variable_alloc(const lm_string_t* name);

void lm__parser_block_scope_variable_free(lm__parser_block_scope_variable_t* var);

typedef struct lm__parser_block_scope_s {
    lm_list_node_t list_node;

    lm__parser_block_scope_type_t scope_type;
    lm_list_node_t variables_head;
} lm__parser_block_scope_t;

lm__parser_block_scope_t* lm__parser_block_scope_alloc(lm__parser_block_scope_type_t scope_type);

void lm__parser_block_scope_free(lm__parser_block_scope_t* scope);

void lm__parser_block_scope_add_variable(lm__parser_block_scope_t* scope, const lm_string_t* name);

lm_bool lm__parser_block_scope_has_variable(lm__parser_block_scope_t* scope, const lm_string_t* name);

typedef void (*lm__parser_error_handler_pfn)(lm_error_t* error);

typedef struct lm_parser_s {
    lm_lexer_t* lexer;

    lm_token_t current_token;
    lm_list_node_t block_scope_head;

    lm__parser_error_handler_pfn error_handler;
} lm_parser_t;

lm_parser_t* lm_parser_alloc(lm_lexer_t* lexer);

void lm_parser_free(lm_parser_t* parser);

lm__ast_program_t* lm_parser_parse(lm_parser_t* parser);

void lm__parser_add_scope(lm_parser_t* parser, lm__parser_block_scope_type_t type);

void lm__parser_remove_scope(lm_parser_t* parser);

void lm__parser_emit_scope_symbol(lm_parser_t* parser, const lm_string_t* name);

lm_bool lm__parser_is_symbol_defined(lm_parser_t* parser, const lm_string_t* name);

void lm__parser_emit_error(lm_parser_t* parser, const char* msg);

lm_token_t lm__parser_consume(lm_parser_t* parser, lm_token_type_t expected_type);

lm_token_t lm__parser_current(lm_parser_t* parser);

void lm__parser_next(lm_parser_t* parser);

lm__ast_statement_t* lm__parser_parse_block(lm_parser_t* parser);

lm__ast_statement_t* lm__parser_parse_statement(lm_parser_t* parser);

lm__ast_statement_t* lm__parser_parse_decl(lm_parser_t* parser);

lm__ast_statement_t* lm__parser_parse_if_statement(lm_parser_t* parser);

lm__ast_statement_t* lm__parser_parse_while_statement(lm_parser_t* parser);

lm__ast_statement_t* lm__parser_parse_for_statement(lm_parser_t* parser);

lm__ast_statement_t* lm__parser_parse_break_statement(lm_parser_t* parser);

lm__ast_statement_t* lm__parser_parse_continue_statement(lm_parser_t* parser);

lm__ast_statement_t* lm__parser_parse_return_statement(lm_parser_t* parser);

lm__ast_statement_t* lm__parser_parse_func_statement(lm_parser_t* parser);

lm__ast_statement_t* lm__parser_parse_include_statement(lm_parser_t* parser);

lm__ast_statement_t* lm__parser_parse_define_statement(lm_parser_t* parser);

lm__ast_expression_t* lm__parser_parse_expression(lm_parser_t* parser);

lm__ast_expression_t* lm__parser_parse_assign_expression(lm_parser_t* parser);

lm__ast_expression_t* lm__parser_parse_additive_expression(lm_parser_t* parser);

lm__ast_expression_t* lm__parser_parse_multiplicative_expression(lm_parser_t* parser);

lm__ast_expression_t* lm__parser_parse_power_expression(lm_parser_t* parser);

lm__ast_expression_t* lm__parser_parse_unary_expression(lm_parser_t* parser);

lm__ast_expression_t* lm__parser_parse_primary(lm_parser_t* parser);
