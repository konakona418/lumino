#include "runtime.h"

#include <stdio.h>

void error_handler(lm_error_t* error) {
    printf("%s\n", lm_error_what(error));
    lm_error_free(error);
}

void test_atom() {
    lm__atom_hash_table_t ht;
    lm__atom_hash_table_init(&ht);

    lm_string_t* str1 = lm_string_alloc("abc", 0);
    lm_string_t* str2 = lm_string_alloc("abc", 0);
    lm_string_t* str3 = lm_string_alloc("def", 0);
    lm_string_t* str4 = lm_string_alloc("ghi", 0);
    lm_string_t* str5 = lm_string_alloc("ghi", 0); /**/

    lm_atom_t a1 = lm__atom_hash_table_intern(&ht, str1);
    lm_atom_t a2 = lm__atom_hash_table_intern(&ht, str2);
    lm_atom_t a3 = lm__atom_hash_table_intern(&ht, str3);
    lm_atom_t a4 = lm__atom_hash_table_intern(&ht, str4);
    lm_atom_t a5 = lm__atom_hash_table_intern(&ht, str5); /**/

    lm_string_free(str1);
    lm_string_free(str2);
    lm_string_free(str3);
    lm_string_free(str4);
    lm_string_free(str5); /**/

    lm__atom_hash_table_deinit(&ht);
}

void test_rc() {
    lm_ref_counted_t* rc = lm_make_ref_counted(int);
    lm_ref_counted_t* dup = lm_ref_counted_clone(rc);

    _LM_ASSERT(rc->internal->ref == 2, "");

    lm_ref_counted_free(rc);
    lm_ref_counted_free(dup);
}

void test_rc_string() {
    lm_string_t* str = lm_string_alloc("test", 0);
    lm_string_t* dup = lm_string_clone(str);

    _LM_ASSERT(lm_string_ref_count(str) == 2, "");

    lm_string_free(str);
    lm_string_free(dup);
}

int main(int argc, char** argv) {
    test_rc_string();
    /*test_atom();
    _LM_ASSERT(lm__alloc_counter_is_zero(), "Memory leak detected!");
    test_rc();
    _LM_ASSERT(lm__alloc_counter_is_zero(), "Memory leak detected!");*/

    lm_runtime_t* runtime = lm_runtime_alloc();
    lm_context_t* ctx = lm_context_alloc(runtime);

    /*const char code[] =
            "var a = 1 + 2; var b = 3;"
            "var c = 0;"
            "if (a != b) {"
            "    c = 1;"
            "} else {"
            "    c = 2;"
            "}"
            "c;";*/
    const char code[] =
            "let sum; let i = 0; sum = 0;"
            "while (i < 10) {"
            "   if (i == 3) {"
            "       i = i + 1;"
            "       continue;"
            "   }"
            "   if (i == 5) {"
            "       break;"
            "   }"
            "   sum += i;"
            "   i += 1;"
            "}"
            "sum;";

    lm_value_t val = lm_eval(ctx, code, 0);
    const lm__byte_array_t* byte_code = lm_context_get_code(ctx);
    lm_print_byte_code(byte_code->data, byte_code->size);

    _LM_ASSERT(val.v.int_value == 7, "Invalid result!");

    lm_context_free(ctx);
    lm_runtime_free(runtime);

    _LM_ASSERT(lm__alloc_counter_is_zero(), "Memory leak detected!");
}
