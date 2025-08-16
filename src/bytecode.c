#include "bytecode.h"
#include "common.h"

#include <string.h>

lm__byte_array_t* lm__byte_array_alloc() {
    lm__byte_array_t* array = _LM_ALLOC(lm__byte_array_t);
    array->size = 0;
    array->capacity = _LM_BYTE_ARRAY_INIT_SIZE;
    array->data = _LM_CALLOC(uint8_t, array->capacity);

    return array;
}

void lm__byte_array_free(lm__byte_array_t* array) {
    _LM_FREE(array->data);
    _LM_FREE(array);
}

void lm__byte_array_realloc(lm__byte_array_t* array, size_t new_capacity) {
    _LM_ASSERT(new_capacity > array->capacity, "new_capacity must be greater than array->capacity");

    size_t old_capacity = array->capacity;
    uint8_t* old_data = array->data;

    array->capacity = new_capacity;
    array->data = _LM_CALLOC(uint8_t, new_capacity);

    memcpy(array->data, old_data, old_capacity);
    _LM_FREE(old_data);
}

void lm__byte_array_reserve(lm__byte_array_t* array, size_t size) {
    if (array->size + size > array->capacity) {
        lm__byte_array_realloc(array, array->size + size);
    }
}

void lm__byte_array_push(lm__byte_array_t* array, uint8_t value) {
    if (array->size >= array->capacity) {
        lm__byte_array_realloc(array, array->size * _LM_BYTE_ARRAY_GROWTH_FACTOR);
    }

    array->data[array->size++] = value;
}

void lm__byte_array_push_array(lm__byte_array_t* array, uint8_t* values, size_t size) {
    if (array->size + size >= array->capacity) {
        lm__byte_array_realloc(array, array->size * _LM_BYTE_ARRAY_GROWTH_FACTOR);
    }

    memcpy(array->data + array->size, values, size);
    array->size += size;
}

uint8_t lm__byte_array_pop(lm__byte_array_t* array) {
    _LM_ASSERT(array->size > 0, "byte array is empty");

    return array->data[--array->size];
}

void lm__byte_array_pop_array(lm__byte_array_t* array, uint8_t* values, size_t size) {
    _LM_ASSERT(array->size >= size, "byte array is not large enough");

    memcpy(values, array->data + array->size - size, size);
    array->size -= size;
}

uint8_t* lm__byte_array_data(lm__byte_array_t* array) {
    return array->data;
}

size_t lm__byte_array_size(lm__byte_array_t* array) {
    return array->size;
}

lm__byte_code_generator_t* lm__byte_code_generator_alloc(lm__byte_code_generator_intern_string_ctx_t intern_string_ctx) {
    lm__byte_code_generator_t* generator = _LM_ALLOC(lm__byte_code_generator_t);
    generator->array = lm__byte_array_alloc();
    generator->intern_string_ctx = intern_string_ctx;
    generator->program = NULL;

    return generator;
}

void lm__byte_code_generator_generate(
        lm__byte_code_generator_t* generator,
        lm__ast_statement_t* program) {
    generator->program = program;
    lm__byte_code_generator_generate_program(generator, generator->program);
}

void lm__byte_code_generator_generate_statement_iterator(lm_list_node_t* node, void* ctx) {
    lm__ast_statement_t* stmt = lm_list_entry(node, lm__ast_statement_t, list_node);
    lm__byte_code_generator_t* generator = ctx;

    lm__byte_code_generator_generate_statement(generator, stmt);
}

void lm__byte_code_generator_generate_program(lm__byte_code_generator_t* generator, lm__ast_statement_t* stmt) {
    lm__ast_program_t* program = _LM_CAST(lm__ast_program_t, stmt);
    lm_list_iterate(program->stmts, lm__byte_code_generator_generate_statement_iterator, generator);
}

void lm__byte_code_generator_generate_statement(lm__byte_code_generator_t* generator, lm__ast_statement_t* stmt) {
    switch (stmt->stmt_type) {
        case LM_AST_STATEMENT_TYPE_PROGRAM:
            _LM_ASSERT(0, "invalid branch");
        case LM_AST_STATEMENT_TYPE_DECL:
            lm__byte_code_generator_generate_declaration(generator, stmt);
            break;
        case LM_AST_STATEMENT_TYPE_EXPRESSION:
            lm__byte_code_generator_generate_expression(generator, _LM_CAST(lm__ast_expression_t, stmt));
            break;
        case LM_AST_STATEMENT_TYPE_BLOCK:
        case LM_AST_STATEMENT_TYPE_IF:
        case LM_AST_STATEMENT_TYPE_FOR:
        case LM_AST_STATEMENT_TYPE_WHILE:
        case LM_AST_STATEMENT_TYPE_BREAK:
        case LM_AST_STATEMENT_TYPE_CONTINUE:
        case LM_AST_STATEMENT_TYPE_FUNC:
        case LM_AST_STATEMENT_TYPE_RETURN:
        case LM_AST_STATEMENT_TYPE_INCLUDE:
        case LM_AST_STATEMENT_TYPE_DEFINE:
            break;
    }
}

void lm__byte_code_generator_generate_declaration(lm__byte_code_generator_t* generator, lm__ast_statement_t* stmt) {
}

void lm__byte_code_generator_generate_expression(lm__byte_code_generator_t* generator, lm__ast_expression_t* expr) {
}

void lm__byte_code_generator_free(lm__byte_code_generator_t* generator) {
    lm__byte_array_free(generator->array);
    _LM_FREE(generator);
}

lm_atom_t lm__byte_code_generator_alloc_atom(lm__byte_code_generator_t* generator, lm_string_t* str) {
    return generator->intern_string_ctx.pfn(generator->intern_string_ctx.ctx, str);
}

void lm__byte_code_generator_emit(lm__byte_code_generator_t* generator, lm__opcode_value_t opcode) {
    lm__byte_array_push(generator->array, (uint8_t) opcode);
}

void lm__byte_code_generator_emit_i32(lm__byte_code_generator_t* generator, int32_t value) {
    lm__byte_array_push_i32(generator->array, &value);
}

void lm__byte_code_generator_emit_f32(lm__byte_code_generator_t* generator, float value) {
    lm__byte_array_push_f32(generator->array, &value);
}

void lm__byte_code_generator_emit_atom(lm__byte_code_generator_t* generator, lm_atom_t atom) {
    lm__byte_array_push_u32(generator->array, &atom);
}