#pragma once

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

typedef struct lm_parser_s {
    lm_lexer_t* lexer;
} lm_parser_t;

lm_parser_t* lm_parser_alloc(lm_lexer_t* lexer);

void lm_parser_free(lm_parser_t* parser);
