#pragma once

#include "bytecode.h"
#include "common.h"
#include "value.h"

#include <stdint.h>

#define LM_RUNTIME_STACK_CELL_SIZE 16
#define _LM_RUNTIME_HASH_TABLE_INIT_SIZE 4

typedef struct lm_runtime_error_s {
    LM_ERROR_HEADER
    lm_string_t* msg;
} lm_runtime_error_t;

const char* lm__runtime_error_what(lm_error_t* error);

void lm__runtime_error_free(lm_error_t* error);

lm_runtime_error_t* lm_runtime_error_alloc(const char* msg);

typedef struct lm__atom_hash_table_entry_s {
    lm_string_t* str;
    lm_atom_t atom;
    lm_bool occupied;
} lm__atom_hash_table_entry_t;

typedef struct lm__atom_hash_table_s {
    lm__atom_hash_table_entry_t* entries;

    size_t* atom_mapping;

    size_t size;
    size_t capacity;
} lm__atom_hash_table_t;

static uint32_t lm__atom_hash_table_hash(lm_string_t* str) {
    return lm_string_hash(str);
}

void lm__atom_hash_table_init(lm__atom_hash_table_t* table);

void lm__atom_hash_table_deinit(lm__atom_hash_table_t* table);

void lm__atom_hash_table_realloc(lm__atom_hash_table_t* table);

lm_atom_t lm__atom_hash_table_intern(lm__atom_hash_table_t* table, const lm_string_t* str);

const lm_string_t* lm__atom_hash_table_lookup(lm__atom_hash_table_t* table, lm_atom_t atom);

struct lm_context_s;

typedef struct lm_runtime_s {
    lm_list_node_t context_head;
    lm__atom_hash_table_t atom_table;
} lm_runtime_t;

lm_runtime_t* lm_runtime_alloc();

void lm_runtime_free(lm_runtime_t* runtime);

void lm__runtime_detach_context(lm_runtime_t* runtime, struct lm_context_s* context);

lm_atom_t lm_runtime_allocate_atom(lm_runtime_t* runtime, const lm_string_t* str);

typedef void* (*lm__context_local_alloc_pfn)(struct lm_context_s* context, size_t size);
typedef void (*lm__context_local_free_pfn)(struct lm_context_s* context, void* ptr);

typedef struct lm__context_local_allocator_s {
    lm__context_local_alloc_pfn alloc;
    lm__context_local_free_pfn free;
} lm__context_local_allocator_t;

typedef struct lm__stack_frame_var_cell_s {
    lm_list_node_t list_node;

    lm_value_t local_vars[LM_RUNTIME_STACK_CELL_SIZE];
    size_t local_vars_count;
} lm__stack_frame_var_cell_t;

lm__stack_frame_var_cell_t* lm__stack_frame_var_cell_alloc();

void lm__stack_frame_var_cell_free(lm__stack_frame_var_cell_t* cell);

typedef struct lm__stack_frame_var_hash_entry_s {
    lm_atom_t key;
    lm_value_t* value_ptr;
} lm__stack_frame_var_hash_entry_t;

typedef struct lm__stack_frame_var_hash_table_s {
    size_t capacity;
    size_t size;
    lm__stack_frame_var_hash_entry_t* entries;
} lm__stack_frame_var_hash_table_t;

static uint32_t lm__stack_frame_var_hash_table_hash(lm_atom_t key) {
    uint32_t x = key;
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;
    return x;
}

void lm__stack_frame_var_hash_table_init(lm__stack_frame_var_hash_table_t* table);

void lm__stack_frame_var_hash_table_deinit(lm__stack_frame_var_hash_table_t* table);

void lm__stack_frame_var_hash_table_realloc(lm__stack_frame_var_hash_table_t* table);

lm_value_t* lm__stack_frame_var_hash_table_get(lm__stack_frame_var_hash_table_t* table, lm_atom_t key);

void lm__stack_frame_var_hash_table_set(lm__stack_frame_var_hash_table_t* table, lm_atom_t key, lm_value_t* value);

typedef struct lm__stack_frame_s {
    lm_list_node_t list_node;
    struct lm_context_s* context;
    uint8_t* return_address;

    lm_list_node_t var_cells_head;
    lm__stack_frame_var_hash_table_t var_hash_table;
} lm__stack_frame_t;

lm__stack_frame_t* lm__stack_frame_alloc(struct lm_context_s* runtime, uint8_t* return_address);

void lm__stack_frame_free(lm__stack_frame_t* frame);

lm_value_t* lm__stack_frame_add_var(lm__stack_frame_t* frame, lm_atom_t name);

lm_value_t* lm__stack_frame_get_var(lm__stack_frame_t* frame, lm_atom_t name);

typedef struct lm__operand_stack_cell_s {
    lm_list_node_t list_node;

    lm_value_t values[LM_RUNTIME_STACK_CELL_SIZE];
    size_t size;
} lm__operand_stack_cell_t;

lm__operand_stack_cell_t* lm__operand_stack_cell_alloc();

void lm__operand_stack_cell_free(lm__operand_stack_cell_t* cell);

typedef struct lm__operand_stack_s {
    lm_list_node_t cells_head;
} lm__operand_stack_t;

lm__operand_stack_t* lm__operand_stack_alloc();

void lm__operand_stack_free(lm__operand_stack_t* stack);

void lm__operand_stack_push(lm__operand_stack_t* stack, lm_value_t value);

lm_value_t lm__operand_stack_pop(lm__operand_stack_t* stack);

void lm__operand_stack_peek(lm__operand_stack_t* stack);

typedef struct lm_context_s {
    lm_list_node_t list_node;
    lm_runtime_t* runtime;

    lm__byte_array_t* code;

    lm_list_node_t stack_frame_head;
    lm__operand_stack_t* operand_stack;
    uint8_t* pc;

    lm__context_local_allocator_t local_allocator;
    lm__byte_code_generator_t* code_generator;
} lm_context_t;

lm_context_t* lm_context_alloc(lm_runtime_t* runtime);

void lm_context_free(lm_context_t* context);

void lm__context_push_frame(lm_context_t* context, uint8_t* pc);

void lm__context_pop_frame(lm_context_t* context);

void lm__context_generate(lm_context_t* context, lm__ast_statement_t* program);