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

    size_t buf_size = strlen(msg) + 128;
    char* buf = lm__alloc(buf_size);

    const lm__lexer_stats_t* stats = lm_lexer_get_stats(parser->lexer);
    int n = snprintf(buf, buf_size, "Parser error: %s, at file %s line %zu col %zu",
                     msg, stats->file->data, stats->line, stats->column);

    _LM_ASSERT(n >= 0, "snprintf failed");

    parser_error->msg = lm_string_from(buf, 0);

    return parser_error;
}

lm_parser_t* lm_parser_alloc(lm_lexer_t* lexer) {
    lm_parser_t* parser = lm__alloc(sizeof(lm_parser_t));
    parser->lexer = lexer;
    return parser;
}

void lm_parser_free(lm_parser_t* parser) {
    lm__free(parser);
}
