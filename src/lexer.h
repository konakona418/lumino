#pragma once

#include "common.h"

typedef struct lm__lexer_stats_s {
    lm_string_t* file;

    size_t line;
    size_t column;
} lm__lexer_stats_t;

typedef struct lm_lexer_error_s {
    LM_ERROR_HEADER
    lm_string_t* msg;
} lm_lexer_error_t;

const char* lm__lexer_error_what(lm_error_t* error);

void lm__lexer_error_free(lm_error_t* error);

lm_lexer_error_t* lm_lexer_error_alloc(const char* msg, lm__lexer_stats_t* stats);

typedef enum lm_token_type_e {
    LM_TOKEN_TYPE_NONE,
    LM_TOKEN_TYPE_INVALID,
    LM_TOKEN_TYPE_TERMINATOR,

    LM_TOKEN_TYPE_IDENTIFIER,

    LM_TOKEN_TYPE_STRING_LITERAL,
    LM_TOKEN_TYPE_INT_LITERAL,
    LM_TOKEN_TYPE_FLOAT_LITERAL,

    LM_TOKEN_TYPE_OP_ADD,
    LM_TOKEN_TYPE_OP_SUB,
    LM_TOKEN_TYPE_OP_MUL,
    LM_TOKEN_TYPE_OP_DIV,
    LM_TOKEN_TYPE_OP_MOD,
    LM_TOKEN_TYPE_OP_POW,
    LM_TOKEN_TYPE_OP_ASSIGN,

    LM_TOKEN_TYPE_OP_EQUAL,
    LM_TOKEN_TYPE_OP_NOT_EQUAL,
    LM_TOKEN_TYPE_OP_GREATER_THAN,
    LM_TOKEN_TYPE_OP_LESS_THAN,
    LM_TOKEN_TYPE_OP_GREATER_THAN_EQUAL,
    LM_TOKEN_TYPE_OP_LESS_THAN_EQUAL,

    LM_TOKEN_TYPE_OP_LOGICAL_AND,
    LM_TOKEN_TYPE_OP_LOGICAL_OR,

    LM_TOKEN_TYPE_L_PARENTHESIS,
    LM_TOKEN_TYPE_R_PARENTHESIS,
    LM_TOKEN_TYPE_L_SQUARE_BRACKET,
    LM_TOKEN_TYPE_R_SQUARE_BRACKET,
    LM_TOKEN_TYPE_L_CURLY_BRACKET,
    LM_TOKEN_TYPE_R_CURLY_BRACKET,

    LM_TOKEN_TYPE_COMMA,
    LM_TOKEN_TYPE_COLON,
    LM_TOKEN_TYPE_SEMICOLON,
    LM_TOKEN_TYPE_DOT,

    LM_TOKEN_TYPE_KEYWORD_VAR,
    LM_TOKEN_TYPE_KEYWORD_FUNC,
    LM_TOKEN_TYPE_KEYWORD_IF,
    LM_TOKEN_TYPE_KEYWORD_ELSE,
    LM_TOKEN_TYPE_KEYWORD_WHILE,
    LM_TOKEN_TYPE_KEYWORD_FOR,
    LM_TOKEN_TYPE_KEYWORD_RETURN,
    LM_TOKEN_TYPE_KEYWORD_BREAK,
    LM_TOKEN_TYPE_KEYWORD_CONTINUE,
    LM_TOKEN_TYPE_KEYWORD_TRUE,
    LM_TOKEN_TYPE_KEYWORD_FALSE,
    LM_TOKEN_TYPE_KEYWORD_NULL,
    LM_TOKEN_TYPE_KEYWORD_INCLUDE,
    LM_TOKEN_TYPE_KEYWORD_DEFINE,
} lm_token_type_t;

const char* lm_token_type_to_string(lm_token_type_t type);

typedef struct lm_token_s {
    lm_token_type_t type;
    lm_string_t* value;
} lm_token_t;

typedef void (*lm__lexer_error_handler_pfn)(lm_error_t* error);

typedef struct lm_lexer_s {
    lm_string_t* source;

    size_t len;
    size_t pos;

    lm__lexer_stats_t stats;
    lm__lexer_error_handler_pfn error_handler;
} lm_lexer_t;

lm_lexer_t* lm_lexer_alloc(const char* input, const char* file);

void lm_lexer_free(lm_lexer_t* lexer);

void lm__lexer_emit_error(lm_lexer_t* lexer, const char* msg);

lm_token_t lm_lexer_next_token(lm_lexer_t* lexer);

const lm__lexer_stats_t* lm_lexer_get_stats(lm_lexer_t* lexer);

void lm__lexer_set_stats_next_line(lm_lexer_t* lexer);

void lm__lexer_advance(lm_lexer_t* lexer);

char lm__lexer_current_char(lm_lexer_t* lexer);

char lm__lexer_peek(lm_lexer_t* lexer);

lm_bool lm__lexer_is_eof(lm_lexer_t* lexer);

lm_bool lm__lexer_is_next_eof(lm_lexer_t* lexer);

void lm__lexer_skip_whitespace(lm_lexer_t* lexer);

void lm__lexer_skip_comment(lm_lexer_t* lexer);

lm_token_t lm__lexer_get_identifier_or_keyword(lm_lexer_t* lexer);

lm_token_t lm__lexer_get_number(lm_lexer_t* lexer);

lm_token_t lm__lexer_get_string_literal(lm_lexer_t* lexer);

lm_bool lm__lexer_is_keyword(lm_string_t* str);

lm_token_type_t lm__lexer_get_keyword_type(lm_string_t* str);
