#include "runtime.h"

#include <stdio.h>
#include <string.h>

const char* lm__runtime_error_what(lm_error_t* error) {
    return _LM_CAST(lm_runtime_error_t, error)->msg->data;
}

void lm__runtime_error_free(lm_error_t* error) {
    lm_runtime_error_t* runtime_error = _LM_CAST(lm_runtime_error_t, error);
    lm_string_free(runtime_error->msg);
    _LM_FREE(error);
}

lm_runtime_error_t* lm_runtime_error_alloc(const char* msg) {
    lm_runtime_error_t* runtime_error = _LM_ALLOC(lm_runtime_error_t);
    runtime_error->vtbl.what = lm__runtime_error_what;
    runtime_error->vtbl.free = lm__runtime_error_free;

    size_t buf_size = snprintf(NULL, 0, "Runtime error: %s", msg);
    char* buf = _LM_ALLOC_ARRAY(char, buf_size + 1);

    int n = snprintf(buf, buf_size + 1, "Runtime error: %s", msg);

    _LM_ASSERT(n >= 0, "snprintf failed");

    runtime_error->msg = lm_string_from(buf, buf_size + 1);

    return runtime_error;
}

void lm__atom_hash_table_init(lm__atom_hash_table_t* table) {
    table->size = 0;
    table->capacity = _LM_RUNTIME_HASH_TABLE_INIT_SIZE;
    table->entries = _LM_CALLOC(lm__atom_hash_table_entry_t, table->capacity);
}

void lm__atom_hash_table_deinit(lm__atom_hash_table_t* table) {
    for (size_t i = 0; i < table->capacity; i++) {
        if (table->entries[i].occupied) {
            lm_string_free(table->entries[i].str);
        }
    }

    _LM_FREE(table->entries);
}

#define _LM_RUNTIME_HASH_TABLE_LOAD_FACTOR 0.7
#define _LM_RUNTIME_HASH_TABLE_RESIZE_FACTOR 2

lm_atom_t lm__atom_hash_table_intern_impl(lm__atom_hash_table_t* table, lm_string_t* str, lm_atom_t atom, lm_bool* found) {
    if (table->size + 1 >= table->capacity * _LM_RUNTIME_HASH_TABLE_LOAD_FACTOR) {
        lm__atom_hash_table_realloc(table);
    }

    uint32_t hash = lm__atom_hash_table_hash(str);
    size_t mask = table->capacity - 1;
    size_t idx = hash & mask;

    for (;;) {
        if (!table->entries[idx].occupied) {
            table->entries[idx].str = str;

            table->entries[idx].atom = atom;
            table->entries[idx].occupied = LM_TRUE;

            if (found) {
                *found = LM_FALSE;
            }

            return atom;
        } else if (lm_string_equal(table->entries[idx].str, str)) {
            if (found) {
                *found = LM_TRUE;
            }

            return table->entries[idx].atom;
        }

        idx = (idx + 1) & mask;
    }
}

void lm__atom_hash_table_realloc(lm__atom_hash_table_t* table) {
    size_t old_capacity = table->capacity;
    lm__atom_hash_table_entry_t* old_entries = table->entries;

    table->capacity *= _LM_RUNTIME_HASH_TABLE_RESIZE_FACTOR;
    table->entries = _LM_CALLOC(lm__atom_hash_table_entry_t, table->capacity);

    for (size_t i = 0; i < old_capacity; ++i) {
        if (old_entries[i].occupied) {
            lm__atom_hash_table_intern_impl(table, old_entries[i].str, old_entries[i].atom, NULL);
        }
    }

    _LM_FREE(old_entries);
}

lm_atom_t lm__atom_hash_table_intern(lm__atom_hash_table_t* table, lm_string_t* str) {
    lm_bool found = LM_FALSE;
    lm_atom_t allocated_atom = table->size + 1;

    lm_atom_t atom = lm__atom_hash_table_intern_impl(table, str, allocated_atom, &found);// no LM_ATOM_NIL

    if (!found) {
        table->size++;
    }

    return atom;
}

lm_string_t* lm__atom_hash_table_lookup(lm__atom_hash_table_t* table, lm_atom_t atom) {
    if (atom >= table->capacity || !table->entries[atom].occupied) {
        return NULL;
    }

    return table->entries[atom].str;
}

lm_runtime_t* lm_runtime_alloc() {
    lm_runtime_t* runtime = _LM_ALLOC(lm_runtime_t);
    lm_list_node_init(&runtime->context_head);
    lm__atom_hash_table_init(&runtime->atom_table);
    return runtime;
}

void lm_runtime_free(lm_runtime_t* runtime) {
    _LM_ASSERT(&runtime->context_head == runtime->context_head.next &&
                       &runtime->context_head == runtime->context_head.prev,
               "runtime still holds undestroyed contexts");

    lm__atom_hash_table_deinit(&runtime->atom_table);
    _LM_FREE(runtime);
}

void lm__runtime_detach_context(lm_runtime_t* runtime, lm_context_t* context) {
    lm_list_remove(&context->list_node);
}

lm__stack_frame_var_cell_t* lm__stack_frame_var_cell_alloc() {
    return _LM_ALLOC(lm__stack_frame_var_cell_t);
}

void lm__stack_frame_var_cell_free(lm__stack_frame_var_cell_t* cell) {
    _LM_FREE(cell);
}

void lm__stack_frame_var_cell_free_iterator(lm_list_node_t* node, void* ctx) {
    lm__stack_frame_var_cell_free(lm_list_entry(node, lm__stack_frame_var_cell_t, list_node));
}

void lm__stack_frame_var_hash_table_init(lm__stack_frame_var_hash_table_t* table) {
    table->size = 0;
    table->capacity = _LM_RUNTIME_HASH_TABLE_INIT_SIZE;
    table->entries = _LM_CALLOC(lm__stack_frame_var_hash_entry_t, table->capacity);
}

void lm__stack_frame_var_hash_table_deinit(lm__stack_frame_var_hash_table_t* table) {
    _LM_FREE(table->entries);
}

void lm__stack_frame_var_hash_table_realloc(lm__stack_frame_var_hash_table_t* table) {
    size_t old_capacity = table->capacity;
    lm__stack_frame_var_hash_entry_t* old_entries = table->entries;

    table->capacity *= _LM_RUNTIME_HASH_TABLE_RESIZE_FACTOR;
    lm__stack_frame_var_hash_entry_t* new_entries = _LM_CALLOC(lm__stack_frame_var_hash_entry_t, table->capacity);

    table->entries = new_entries;
    table->size = 0;

    for (size_t i = 0; i < table->capacity; ++i) {
        if (old_entries[i].key != LM_ATOM_NIL) {
            lm__stack_frame_var_hash_table_set(table, old_entries[i].key, old_entries[i].value_ptr);
        }
    }

    _LM_FREE(old_entries);
}

lm_value_t* lm__stack_frame_var_hash_table_get(lm__stack_frame_var_hash_table_t* table, lm_atom_t key) {
    size_t mask = table->capacity - 1;
    size_t idx = lm__stack_frame_var_hash_table_hash(key) & mask;

    while (table->entries[idx].key != LM_ATOM_NIL) {
        if (table->entries[idx].key == key) {
            return table->entries[idx].value_ptr;
        }
        idx = (idx + 1) & mask;
    }
    return NULL;
}

void lm__stack_frame_var_hash_table_set(lm__stack_frame_var_hash_table_t* table, lm_atom_t key, lm_value_t* value) {
    if (table->size + 1 > table->capacity * _LM_RUNTIME_HASH_TABLE_LOAD_FACTOR) {
        lm__stack_frame_var_hash_table_realloc(table);
    }

    size_t mask = table->capacity - 1;
    size_t idx = lm__stack_frame_var_hash_table_hash(key) & mask;

    while (table->entries[idx].key != LM_ATOM_NIL && table->entries[idx].key != key) {
        idx = (idx + 1) & mask;
    }

    table->entries[idx].key = key;
    table->entries[idx].value_ptr = value;
    table->size++;
}

lm__stack_frame_t* lm__stack_frame_alloc(lm_context_t* context, uint8_t* return_address) {
    lm__stack_frame_t* frame = _LM_ALLOC(lm__stack_frame_t);
    frame->return_address = return_address;
    frame->context = context;
    lm__stack_frame_var_hash_table_init(&frame->var_hash_table);
    lm_list_node_init(&frame->list_node);
    lm_list_node_init(&frame->var_cells_head);

    return frame;
}

void lm__stack_frame_free(lm__stack_frame_t* frame) {
    lm_list_iterate_safe(&frame->var_cells_head, lm__stack_frame_var_cell_free_iterator, NULL);
    lm__stack_frame_var_hash_table_deinit(&frame->var_hash_table);

    _LM_FREE(frame);
}

lm_value_t* lm__stack_frame_add_var(lm__stack_frame_t* frame, lm_atom_t name) {
    lm__stack_frame_var_cell_t* cell =
            lm_list_entry(lm_list_tail(&frame->var_cells_head),
                          lm__stack_frame_var_cell_t, list_node);

    lm_value_t* value_ref = NULL;
    if (cell->local_vars_count == LM_RUNTIME_STACK_CELL_SIZE) {
        cell = lm__stack_frame_var_cell_alloc();
        lm_list_add_tail(&cell->list_node, &frame->var_cells_head);
        value_ref = &cell->local_vars[0];
    } else {
        value_ref = &cell->local_vars[cell->local_vars_count++];
    }

    lm__stack_frame_var_hash_table_set(&frame->var_hash_table, name, value_ref);
    return value_ref;
}

lm_value_t* lm__stack_frame_get_var(lm__stack_frame_t* frame, lm_atom_t name) {
    return lm__stack_frame_var_hash_table_get(&frame->var_hash_table, name);
}

lm_context_t* lm_context_alloc(lm_runtime_t* runtime) {
    _LM_ASSERT_NOT_NULL(runtime, "runtime cannot be null");

    lm_context_t* context = _LM_ALLOC(lm_context_t);

    lm_list_node_init(&context->list_node);
    lm_list_node_init(&context->stack_frame_head);
    context->runtime = runtime;
    context->pc = NULL;
    // todo: context->local_allocator = runtime->local_allocator;

    return context;
}

void lm_context_free(lm_context_t* context) {
    // todo: detach stack frames

    lm__runtime_detach_context(context->runtime, context);
    _LM_FREE(context);
}

void lm__context_push_frame(lm_context_t* context, uint8_t* pc) {
    lm__stack_frame_t* frame = lm__stack_frame_alloc(context, pc);
    lm_list_add_tail(&context->stack_frame_head, &frame->list_node);
}

void lm__context_pop_frame(lm_context_t* context) {
    lm_list_node_t* tail = lm_list_tail(&context->stack_frame_head);
    lm__stack_frame_t* frame = lm_list_entry(tail, lm__stack_frame_t, list_node);

    lm_list_remove(tail);
    lm__stack_frame_free(frame);
}