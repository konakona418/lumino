#include "lexer.h"

#include <stdio.h>
#include <string.h>

#define _LM_IS_WHITESPACE(c) (c == ' ' || c == '\t' || c == '\n' || c == '\r')
#define _LM_IS_DIGIT(c) (c >= '0' && c <= '9')
#define _LM_IS_ALPHA(c) (c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '_')
#define _LM_IS_ALPHA_NUMERIC(c) (_LM_IS_ALPHA(c) || _LM_IS_DIGIT(c))
#define _LM_PERHAPS_COMMENT(_cur, _peek) ((_cur == '/' && _peek == '/') || (_cur == '/' && _peek == '*'))
#define _LM_PERHAPS_STRING_LITERAL(_cur) (_cur == '"' || _cur == '\'')
#define _LM_IS_VALID_ESCAPE_SEQUENCE(_peek) (_peek == '\\' || _peek == '0' || _peek == 'n' || _peek == 't' || _peek == 'r' || _peek == '"' || _peek == '\'')

#define _LM_HANDLE_TWO_CHAR_OPS(_first, _second, _type, _lexer)                          \
    if (_first == lm__lexer_current_char(_lexer) && _second == lm__lexer_peek(_lexer)) { \
        lm__lexer_advance(_lexer);                                                       \
        lm__lexer_advance(_lexer);                                                       \
        return (lm_token_t){_type, NULL};                                                \
    }

#define _LM_HANDLE_CHAR(char_lit, token_type) \
    case char_lit:                            \
        lm__lexer_advance(lexer);             \
        return (lm_token_t){token_type, NULL};

const char* lm_token_type_to_string(lm_token_type_t type) {
    switch (type) {
        case LM_TOKEN_TYPE_INVALID:
            return "INVALID";
        case LM_TOKEN_TYPE_TERMINATOR:
            return "TERMINATOR";
        case LM_TOKEN_TYPE_IDENTIFIER:
            return "IDENTIFIER";
        case LM_TOKEN_TYPE_STRING_LITERAL:
            return "STRING_LITERAL";
        case LM_TOKEN_TYPE_INT_LITERAL:
            return "INT_LITERAL";
        case LM_TOKEN_TYPE_FLOAT_LITERAL:
            return "FLOAT_LITERAL";
        case LM_TOKEN_TYPE_OP_ADD:
            return "OP_ADD";
        case LM_TOKEN_TYPE_OP_SUB:
            return "OP_SUB";
        case LM_TOKEN_TYPE_OP_MUL:
            return "OP_MUL";
        case LM_TOKEN_TYPE_OP_DIV:
            return "OP_DIV";
        case LM_TOKEN_TYPE_OP_MOD:
            return "OP_MOD";
        case LM_TOKEN_TYPE_OP_POW:
            return "OP_POW";
        case LM_TOKEN_TYPE_OP_ASSIGN:
            return "OP_ASSIGN";
        case LM_TOKEN_TYPE_OP_EQUAL:
            return "OP_EQUAL";
        case LM_TOKEN_TYPE_OP_NOT_EQUAL:
            return "OP_NOT_EQUAL";
        case LM_TOKEN_TYPE_OP_GREATER_THAN:
            return "OP_GREATER_THAN";
        case LM_TOKEN_TYPE_OP_LESS_THAN:
            return "OP_LESS_THAN";
        case LM_TOKEN_TYPE_OP_GREATER_THAN_EQUAL:
            return "OP_GREATER_THAN_EQUAL";
        case LM_TOKEN_TYPE_OP_LESS_THAN_EQUAL:
            return "OP_LESS_THAN_EQUAL";
        case LM_TOKEN_TYPE_L_PARENTHESIS:
            return "L_PARENTHESIS";
        case LM_TOKEN_TYPE_R_PARENTHESIS:
            return "R_PARENTHESIS";
        case LM_TOKEN_TYPE_L_SQUARE_BRACKET:
            return "L_SQUARE_BRACKET";
        case LM_TOKEN_TYPE_R_SQUARE_BRACKET:
            return "R_SQUARE_BRACKET";
        case LM_TOKEN_TYPE_L_CURLY_BRACKET:
            return "L_CURLY_BRACKET";
        case LM_TOKEN_TYPE_R_CURLY_BRACKET:
            return "R_CURLY_BRACKET";
        case LM_TOKEN_TYPE_COMMA:
            return "COMMA";
        case LM_TOKEN_TYPE_COLON:
            return "COLON";
        case LM_TOKEN_TYPE_SEMICOLON:
            return "SEMICOLON";
        case LM_TOKEN_TYPE_DOT:
            return "DOT";
        case LM_TOKEN_TYPE_KEYWORD_LET:
            return "KEYWORD_VAR";
        case LM_TOKEN_TYPE_KEYWORD_FUNC:
            return "KEYWORD_FUNC";
        case LM_TOKEN_TYPE_KEYWORD_IF:
            return "KEYWORD_IF";
        case LM_TOKEN_TYPE_KEYWORD_ELSE:
            return "KEYWORD_ELSE";
        case LM_TOKEN_TYPE_KEYWORD_WHILE:
            return "KEYWORD_WHILE";
        case LM_TOKEN_TYPE_KEYWORD_FOR:
            return "KEYWORD_FOR";
        case LM_TOKEN_TYPE_KEYWORD_RETURN:
            return "KEYWORD_RETURN";
        case LM_TOKEN_TYPE_KEYWORD_BREAK:
            return "KEYWORD_BREAK";
        case LM_TOKEN_TYPE_KEYWORD_CONTINUE:
            return "KEYWORD_CONTINUE";
        case LM_TOKEN_TYPE_KEYWORD_TRUE:
            return "KEYWORD_TRUE";
        case LM_TOKEN_TYPE_KEYWORD_FALSE:
            return "KEYWORD_FALSE";
        case LM_TOKEN_TYPE_KEYWORD_NULL:
            return "KEYWORD_NULL";
        case LM_TOKEN_TYPE_KEYWORD_IMPORT:
            return "KEYWORD_IMPORT";
        default:
            return "UNKNOWN";
    }
}

const char* lm__lexer_error_what(lm_error_t* error) {
    return ((lm_lexer_error_t*) error)->msg->data;
}

void lm__lexer_error_free(lm_error_t* error) {
    if (error == NULL) {
        return;
    }

    lm_string_free(((lm_lexer_error_t*) error)->msg);
    _LM_FREE(error);
}

lm_lexer_error_t* lm_lexer_error_alloc(const char* msg, lm__lexer_stats_t* stats) {
    lm_lexer_error_t* lexer_error = _LM_ALLOC(lm_lexer_error_t);
    lexer_error->vtbl.free = lm__lexer_error_free;
    lexer_error->vtbl.what = lm__lexer_error_what;

    size_t buf_size = snprintf(NULL, 0, "Parser error: %s, at file %s line %zu col %zu",
                               msg, stats->file->data, stats->line, stats->column);
    char* buf = _LM_ALLOC_ARRAY(char, buf_size + 1);

    int n = snprintf(buf, buf_size + 1, "Lexer error: %s, at file %s line %zu col %zu",
                     msg, stats->file->data, stats->line, stats->column);

    _LM_ASSERT(n >= 0, "snprintf failed");

    lexer_error->msg = lm_string_from(buf, 0);

    return lexer_error;
}

lm_lexer_t* lm_lexer_alloc(const char* input, const char* file) {
    lm_lexer_t* lexer = _LM_ALLOC(lm_lexer_t);
    lexer->source = lm_string_alloc(input, 0);
    lexer->len = lm_string_len(lexer->source);
    lexer->pos = 0;
    lexer->stats.file = lm_string_alloc(file, 0);
    lexer->stats.line = 1;
    lexer->stats.column = 1;
    lexer->error_handler = NULL;

    return lexer;
}

void lm_lexer_free(lm_lexer_t* lexer) {
    if (lexer == NULL) {
        return;
    }

    lm_string_free(lexer->source);
    lm_string_free(lexer->stats.file);
    _LM_FREE(lexer);
}

void lm__lexer_emit_error(lm_lexer_t* lexer, const char* msg) {
    if (!lexer->error_handler) {
        return;
    }

    lm_lexer_error_t* error = lm_lexer_error_alloc(msg, &lexer->stats);
    lexer->error_handler((lm_error_t*) error);
}

lm_token_t lm_lexer_next_token(lm_lexer_t* lexer) {
    if (lm__lexer_is_eof(lexer)) {
        return (lm_token_t){LM_TOKEN_TYPE_TERMINATOR, NULL};
    }

    while (!lm__lexer_is_eof(lexer)) {
        if (_LM_IS_WHITESPACE(lm__lexer_current_char(lexer))) {
            lm__lexer_skip_whitespace(lexer);
            continue;
        }

        if (_LM_PERHAPS_COMMENT(lm__lexer_current_char(lexer), lm__lexer_peek(lexer))) {
            lm__lexer_skip_comment(lexer);
            continue;
        }

        break;
    }

    if (_LM_IS_DIGIT(lm__lexer_current_char(lexer))) {
        return lm__lexer_get_number(lexer);
    }

    if (_LM_IS_ALPHA(lm__lexer_current_char(lexer))) {
        return lm__lexer_get_identifier_or_keyword(lexer);
    }

    if (_LM_PERHAPS_STRING_LITERAL(lm__lexer_current_char(lexer))) {
        return lm__lexer_get_string_literal(lexer);
    }

    if (!lm__lexer_is_next_eof(lexer)) {
        _LM_HANDLE_TWO_CHAR_OPS('=', '=', LM_TOKEN_TYPE_OP_EQUAL, lexer)
        _LM_HANDLE_TWO_CHAR_OPS('!', '=', LM_TOKEN_TYPE_OP_NOT_EQUAL, lexer)
        _LM_HANDLE_TWO_CHAR_OPS('<', '=', LM_TOKEN_TYPE_OP_LESS_THAN_EQUAL, lexer)
        _LM_HANDLE_TWO_CHAR_OPS('>', '=', LM_TOKEN_TYPE_OP_GREATER_THAN_EQUAL, lexer)
        _LM_HANDLE_TWO_CHAR_OPS('&', '&', LM_TOKEN_TYPE_OP_LOGICAL_AND, lexer)
        _LM_HANDLE_TWO_CHAR_OPS('|', '|', LM_TOKEN_TYPE_OP_LOGICAL_OR, lexer)
    }

    switch (lm__lexer_current_char(lexer)) {
        _LM_HANDLE_CHAR(':', LM_TOKEN_TYPE_COLON)
        _LM_HANDLE_CHAR(',', LM_TOKEN_TYPE_COMMA)
        _LM_HANDLE_CHAR('.', LM_TOKEN_TYPE_DOT)
        _LM_HANDLE_CHAR(';', LM_TOKEN_TYPE_SEMICOLON)
        _LM_HANDLE_CHAR('{', LM_TOKEN_TYPE_L_CURLY_BRACKET)
        _LM_HANDLE_CHAR('}', LM_TOKEN_TYPE_R_CURLY_BRACKET)
        _LM_HANDLE_CHAR('(', LM_TOKEN_TYPE_L_PARENTHESIS)
        _LM_HANDLE_CHAR(')', LM_TOKEN_TYPE_R_PARENTHESIS)
        _LM_HANDLE_CHAR('[', LM_TOKEN_TYPE_L_SQUARE_BRACKET)
        _LM_HANDLE_CHAR(']', LM_TOKEN_TYPE_R_SQUARE_BRACKET)
        _LM_HANDLE_CHAR('=', LM_TOKEN_TYPE_OP_ASSIGN)
        _LM_HANDLE_CHAR('+', LM_TOKEN_TYPE_OP_ADD)
        _LM_HANDLE_CHAR('-', LM_TOKEN_TYPE_OP_SUB)
        _LM_HANDLE_CHAR('*', LM_TOKEN_TYPE_OP_MUL)
        _LM_HANDLE_CHAR('/', LM_TOKEN_TYPE_OP_DIV)
        _LM_HANDLE_CHAR('%', LM_TOKEN_TYPE_OP_MOD)
        _LM_HANDLE_CHAR('^', LM_TOKEN_TYPE_OP_POW)
        _LM_HANDLE_CHAR('<', LM_TOKEN_TYPE_OP_LESS_THAN)
        _LM_HANDLE_CHAR('>', LM_TOKEN_TYPE_OP_GREATER_THAN)
        default: {
            char chr = lm__lexer_current_char(lexer);
            if (chr == '\0') {
                return (lm_token_t){LM_TOKEN_TYPE_TERMINATOR, NULL};
            }
            lm_token_t invalid = (lm_token_t){LM_TOKEN_TYPE_INVALID, lm_string_alloc(&chr, 1)};
            lm__lexer_advance(lexer);

            return invalid;
        }
    }
}

const lm__lexer_stats_t* lm_lexer_get_stats(lm_lexer_t* lexer) {
    return &lexer->stats;
}

void lm__lexer_set_stats_next_line(lm_lexer_t* lexer) {
    lexer->stats.line++;
    lexer->stats.column = 1;
}

void lm__lexer_advance(lm_lexer_t* lexer) {
    if (lm__lexer_current_char(lexer) == '\n') {
        lm__lexer_set_stats_next_line(lexer);
    } else {
        lexer->stats.column++;
    }
    lexer->pos++;
}

char lm__lexer_current_char(lm_lexer_t* lexer) {
    lm_bool is_valid;
    char c = lm_string_get_safe(lexer->source, lexer->pos, &is_valid);
    _LM_ASSERT(is_valid, "lexer current pos out of bounds");

    return c;
}

char lm__lexer_peek(lm_lexer_t* lexer) {
    lm_bool is_valid;
    char c = lm_string_get_safe(lexer->source, lexer->pos + 1, &is_valid);
    _LM_ASSERT(is_valid, "lexer peek pos out of bounds");

    return c;
}

lm_bool lm__lexer_is_eof(lm_lexer_t* lexer) {
    return lexer->pos >= lexer->len;
}

lm_bool lm__lexer_is_next_eof(lm_lexer_t* lexer) {
    return lexer->pos + 1 >= lexer->len;
}

void lm__lexer_skip_whitespace(lm_lexer_t* lexer) {
    while (_LM_IS_WHITESPACE(lm__lexer_current_char(lexer))) {
        lm__lexer_advance(lexer);
    }
}

void lm__lexer_skip_comment(lm_lexer_t* lexer) {
    if (lm__lexer_current_char(lexer) == '/' && lm__lexer_peek(lexer) == '/') {
        while (lm__lexer_current_char(lexer) != '\n' && !lm__lexer_is_eof(lexer)) {
            lm__lexer_advance(lexer);
        }

        if (lm__lexer_current_char(lexer) == '\n') {
            lm__lexer_advance(lexer);
        }
    } else if (lm__lexer_current_char(lexer) == '/' && lm__lexer_peek(lexer) == '*') {
        lm__lexer_advance(lexer);
        lm__lexer_advance(lexer);

        while (!(lm__lexer_current_char(lexer) == '*' && lm__lexer_peek(lexer) == '/')) {
            if (lm__lexer_is_eof(lexer)) {
                lm__lexer_emit_error(lexer, "unterminated comment");

                return;
            }
            lm__lexer_advance(lexer);
        }

        lm__lexer_advance(lexer);
        lm__lexer_advance(lexer);
    }
}

#define _LM_KEYWORD_COUNT 13

lm_bool lm__lexer_is_keyword(lm_string_t* str) {
    const char** keywords = (const char*[]){
            "let",
            "for",
            "while",
            "break",
            "continue",
            "func",
            "return",
            "if",
            "else",
            "import",
            "true",
            "false",
            "null",
    };

    for (size_t i = 0; i < _LM_KEYWORD_COUNT - 1; i++) {
        if (strcmp(str->data, keywords[i]) == 0) {
            return LM_TRUE;
        }
    }

    return LM_FALSE;
}

lm_token_type_t lm__lexer_get_keyword_type(lm_string_t* str) {
    const char** keywords = (const char*[]){
            "let",
            "for",
            "while",
            "break",
            "continue",
            "func",
            "return",
            "if",
            "else",
            "import",
            "true",
            "false",
            "null",
    };

    const lm_token_type_t* keyword_types = (lm_token_type_t[]){
            LM_TOKEN_TYPE_KEYWORD_LET,
            LM_TOKEN_TYPE_KEYWORD_FOR,
            LM_TOKEN_TYPE_KEYWORD_WHILE,
            LM_TOKEN_TYPE_KEYWORD_BREAK,
            LM_TOKEN_TYPE_KEYWORD_CONTINUE,
            LM_TOKEN_TYPE_KEYWORD_FUNC,
            LM_TOKEN_TYPE_KEYWORD_RETURN,
            LM_TOKEN_TYPE_KEYWORD_IF,
            LM_TOKEN_TYPE_KEYWORD_ELSE,
            LM_TOKEN_TYPE_KEYWORD_IMPORT,
            LM_TOKEN_TYPE_KEYWORD_TRUE,
            LM_TOKEN_TYPE_KEYWORD_FALSE,
            LM_TOKEN_TYPE_KEYWORD_NULL,
    };

    for (size_t i = 0; i < _LM_KEYWORD_COUNT - 1; i++) {
        if (strcmp(str->data, keywords[i]) == 0) {
            return keyword_types[i];
        }
    }

    _LM_ASSERT(LM_FALSE, "not a keyword");
}


lm_token_t lm__lexer_get_identifier_or_keyword(lm_lexer_t* lexer) {
    size_t start_pos = lexer->pos;

    while (_LM_IS_ALPHA_NUMERIC(lm__lexer_current_char(lexer))) {
        lm__lexer_advance(lexer);
    }

    lm_string_t* identifier = lm_string_alloc(lexer->source->data + start_pos, lexer->pos - start_pos);

    if (lm__lexer_is_keyword(identifier)) {
        lm_token_type_t keyword_type = lm__lexer_get_keyword_type(identifier);
        lm_string_free(identifier);

        return (lm_token_t){keyword_type, NULL};
    }

    return (lm_token_t){LM_TOKEN_TYPE_IDENTIFIER, identifier};
}

lm_token_t lm__lexer_get_number(lm_lexer_t* lexer) {
    size_t start_pos = lexer->pos;

    lm_bool is_float = LM_FALSE;

    while (_LM_IS_DIGIT(lm__lexer_current_char(lexer))) {
        lm__lexer_advance(lexer);
    }

    if (lm__lexer_current_char(lexer) == '.') {
        is_float = LM_TRUE;

        lm__lexer_advance(lexer);

        while (_LM_IS_DIGIT(lm__lexer_current_char(lexer))) {
            lm__lexer_advance(lexer);
        }
    }

    return (lm_token_t){
            .type = is_float ? LM_TOKEN_TYPE_FLOAT_LITERAL : LM_TOKEN_TYPE_INT_LITERAL,
            .value = lm_string_alloc(lexer->source->data + start_pos, lexer->pos - start_pos)};
}

lm_token_t lm__lexer_get_string_literal(lm_lexer_t* lexer) {
    size_t start_pos = lexer->pos;
    char start_char = lm__lexer_current_char(lexer);

    lm__lexer_advance(lexer);

    while (lm__lexer_current_char(lexer) != start_char) {
        if (lm__lexer_current_char(lexer) == '\n' || lm__lexer_is_eof(lexer)) {
            lm__lexer_emit_error(lexer, "unterminated string literal");

            return (lm_token_t){LM_TOKEN_TYPE_INVALID, NULL};
        }

        if (lm__lexer_current_char(lexer) == '\\') {
            if (!_LM_IS_VALID_ESCAPE_SEQUENCE(lm__lexer_peek(lexer))) {
                lm__lexer_emit_error(lexer, "invalid escape sequence");
            }
            lm__lexer_advance(lexer);
            lm__lexer_advance(lexer);

            continue;
        }

        lm__lexer_advance(lexer);
    }
    lm__lexer_advance(lexer);
    return (lm_token_t){
            .type = LM_TOKEN_TYPE_STRING_LITERAL,
            .value = lm_string_alloc(lexer->source->data + start_pos, lexer->pos - start_pos)};
}
