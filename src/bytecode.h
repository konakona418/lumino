#pragma once

#include "parser.h"

#include <stdint.h>


typedef uint8_t lm__opcode_t;

typedef enum lm__opcode_value_e {
    LM__OPCODE_NOP = 0,
    LM__OPCODE_HALT,

    LM__PUSH,
    LM__POP,

    LM__LOAD_UNDEFINED,
    LM__LOAD_NULL,
    LM__LOAD_BOOL,
    LM__LOAD_INT,
    LM__LOAD_FLOAT,
    LM__LOAD_STRING,

    LM__DECL_VAR,
    LM__LOAD_VAR,
    LM__STORE_VAR,

    LM__OP_ADD,
    LM__OP_SUB,
    LM__OP_MUL,
    LM__OP_DIV,
    LM__OP_MOD,

    LM__OP_AND,
    LM__OP_OR,

    LM__OP_NEG,
    LM__OP_NOT,

    LM__OP_EQ,
    LM__OP_NEQ,
    LM__OP_LT,
    LM__OP_LTE,
    LM__OP_GT,
    LM__OP_GTE,

    LM__OP_JMP,
    LM__OP_JMP_IF_FALSE,
    LM__OP_JMP_IF_TRUE,

    LM__OP_CALL,
    LM__OP_RET,

} lm__opcode_value_t;

#define _LM_BYTE_ARRAY_INIT_SIZE 16
#define _LM_BYTE_ARRAY_GROWTH_FACTOR 2

typedef struct lm__byte_array_s {
    size_t size;
    size_t capacity;

    uint8_t* data;
} lm__byte_array_t;

lm__byte_array_t* lm__byte_array_alloc();

void lm__byte_array_free(lm__byte_array_t* array);

void lm__byte_array_realloc(lm__byte_array_t* array, size_t new_capacity);

void lm__byte_array_reserve(lm__byte_array_t* array, size_t size);

void lm__byte_array_push(lm__byte_array_t* array, uint8_t value);

void lm__byte_array_push_array(lm__byte_array_t* array, uint8_t* values, size_t size);

#define lm__byte_array_push_t(array, type, value_ptr) lm__byte_array_push_array(array, (uint8_t*) value_ptr, sizeof(type))

#define lm__byte_array_push_u16(array, value) lm__byte_array_push_t(array, uint16_t, value)
#define lm__byte_array_push_u32(array, value) lm__byte_array_push_t(array, uint32_t, value)
#define lm__byte_array_push_u64(array, value) lm__byte_array_push_t(array, uint64_t, value)

#define lm__byte_array_push_i32(array, value) lm__byte_array_push_t(array, int32_t, value)
#define lm__byte_array_push_f32(array, value) lm__byte_array_push_t(array, float, value)

uint8_t lm__byte_array_pop(lm__byte_array_t* array);

void lm__byte_array_pop_array(lm__byte_array_t* array, uint8_t* values, size_t size);

#define lm__byte_array_pop_t(array, type, value_ptr) lm__byte_array_pop_array(array, (uint8_t*) value_ptr, sizeof(type))

#define lm__byte_array_pop_u16(array, value) lm__byte_array_pop_t(array, uint16_t, value)
#define lm__byte_array_pop_u32(array, value) lm__byte_array_pop_t(array, uint32_t, value)
#define lm__byte_array_pop_u64(array, value) lm__byte_array_pop_t(array, uint64_t, value)

#define lm__byte_array_pop_i32(array, value) lm__byte_array_pop_t(array, int32_t, value)
#define lm__byte_array_pop_f32(array, value) lm__byte_array_pop_t(array, float, value)

static uint8_t* lm__byte_array_data(lm__byte_array_t* array);

static size_t lm__byte_array_size(lm__byte_array_t* array);

typedef struct lm_byte_code_error_s {
    LM_ERROR_HEADER
    lm_string_t* msg;
} lm_byte_code_error_t;

const char* lm__byte_code_error_what(lm_error_t* error);

void lm__byte_code_error_free(lm_error_t* error);

lm_byte_code_error_t* lm__byte_code_error_alloc(const char* msg);

typedef lm_atom_t (*lm__byte_code_generator_intern_string_pfn)(lm_string_t*, void* ctx);

typedef void (*lm__byte_code_generator_error_handler_pfn)(lm_error_t*);

typedef struct lm__byte_code_generator_intern_string_ctx_s {
    lm__byte_code_generator_intern_string_pfn pfn;
    void* ctx;
} lm__byte_code_generator_intern_string_ctx_t;

typedef struct lm__byte_code_generator_s {
    lm__byte_array_t* array;
    lm__ast_statement_t* program;

    lm__byte_code_generator_intern_string_ctx_t intern_string_ctx;
    lm__byte_code_generator_error_handler_pfn error_handler;
} lm__byte_code_generator_t;

void lm__byte_code_generator_emit_error(lm__byte_code_generator_t* generator, const char* msg);

lm__byte_code_generator_t* lm__byte_code_generator_alloc(lm__byte_code_generator_intern_string_ctx_t intern_string_ctx);

void lm__byte_code_generator_clear(lm__byte_code_generator_t* generator);

void lm__byte_code_generator_generate(lm__byte_code_generator_t* generator, lm__ast_statement_t* program);

const lm__byte_array_t* lm__byte_code_generator_get_array(lm__byte_code_generator_t* generator);

void lm__byte_code_generator_generate_program(lm__byte_code_generator_t* generator, lm__ast_statement_t* stmt);

void lm__byte_code_generator_generate_statement(lm__byte_code_generator_t* generator, lm__ast_statement_t* stmt);

void lm__byte_code_generator_generate_declaration(lm__byte_code_generator_t* generator, lm__ast_statement_t* stmt);

void lm__byte_code_generator_generate_expression(lm__byte_code_generator_t* generator, lm__ast_expression_t* expr);

void lm__byte_code_generator_generate_lvalue_expr(lm__byte_code_generator_t* generator, lm__ast_expression_t* expr);

void lm__byte_code_generator_generate_var_access_expr(lm__byte_code_generator_t* generator, lm__ast_expression_t* expr);

void lm__byte_code_generator_generate_assign_expr(lm__byte_code_generator_t* generator, lm__ast_expression_t* expr);

void lm__byte_code_generator_generate_binary_expr(lm__byte_code_generator_t* generator, lm__ast_expression_t* expr);

void lm__byte_code_generator_generate_unary_expr(lm__byte_code_generator_t* generator, lm__ast_expression_t* expr);

void lm__byte_code_generator_generate_literal_expr(lm__byte_code_generator_t* generator, lm__ast_expression_t* expr);

void lm__byte_code_generator_free(lm__byte_code_generator_t* generator);

lm_atom_t lm__byte_code_generator_alloc_atom(lm__byte_code_generator_t* generator, lm_string_t* str);

void lm__byte_code_generator_emit(lm__byte_code_generator_t* generator, lm__opcode_value_t opcode);

void lm__byte_code_generator_emit_byte(lm__byte_code_generator_t* generator, uint8_t value);

void lm__byte_code_generator_emit_i32(lm__byte_code_generator_t* generator, int32_t value);

void lm__byte_code_generator_emit_f32(lm__byte_code_generator_t* generator, float value);

void lm__byte_code_generator_emit_atom(lm__byte_code_generator_t* generator, lm_atom_t atom);

void lm_print_byte_code(uint8_t* byte_code);
