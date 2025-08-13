#include "lexer.h"

#include <stdio.h>

void lexer_error_handler(lm_error_t* error) {
    printf("%s\n", error->vtbl.what(error));
    lm__lexer_error_free(error);
}

int main(int argc, char** argv) {
    printf("Hello, from nLamina!\n");
    lm_lexer_t* lexer = lm_lexer_alloc("var a = '\\m';", "<main>");
    lexer->error_handler = lexer_error_handler;

    while (1) {
        lm_token_t token = lm_lexer_next_token(lexer);
        if (token.type == LM_TOKEN_TYPE_TERMINATOR) {
            break;
        }
        printf("token(type = %s, value = %s)\n",
               lm_token_type_to_string(token.type),
               token.value ? token.value->data : "N/A");
        lm_string_free(token.value);
    }
    lm_lexer_free(lexer);

    _LM_ASSERT(lm__alloc_counter_is_zero(), "Memory leak detected!");
}
