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
    table->atom_mapping = _LM_CALLOC(size_t, table->capacity);
}

void lm__atom_hash_table_deinit(lm__atom_hash_table_t* table) {
    for (size_t i = 0; i < table->capacity; i++) {
        if (table->entries[i].occupied) {
            lm_string_free(table->entries[i].str);
        }
    }

    _LM_FREE(table->atom_mapping);
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

            _LM_ASSERT(atom < table->capacity, "atom out of range");
            table->atom_mapping[atom] = idx;

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
    size_t* old_atom_mapping = table->atom_mapping;

    table->capacity *= _LM_RUNTIME_HASH_TABLE_RESIZE_FACTOR;
    table->entries = _LM_CALLOC(lm__atom_hash_table_entry_t, table->capacity);
    table->atom_mapping = _LM_CALLOC(size_t, table->capacity);

    for (size_t i = 0; i < old_capacity; ++i) {
        if (old_entries[i].occupied) {
            lm__atom_hash_table_intern_impl(table, old_entries[i].str, old_entries[i].atom, NULL);
        }
    }

    _LM_FREE(old_atom_mapping);
    _LM_FREE(old_entries);
}

lm_atom_t lm__atom_hash_table_intern(lm__atom_hash_table_t* table, const lm_string_t* str) {
    lm_string_t* dup = lm_string_clone(str);// increase ref count

    lm_bool found = LM_FALSE;
    lm_atom_t allocated_atom = table->size + 1;

    lm_atom_t atom = lm__atom_hash_table_intern_impl(table, dup, allocated_atom, &found);// no LM_ATOM_NIL

    if (!found) {
        table->size++;
    } else {
        lm_string_free(dup);
    }

    return atom;
}

const lm_string_t* lm__atom_hash_table_lookup(lm__atom_hash_table_t* table, lm_atom_t atom) {
    size_t mapped = table->atom_mapping[atom];
    _LM_ASSERT(mapped < table->capacity, "atom out of range");

    if (mapped >= table->capacity || !table->entries[mapped].occupied) {
        return NULL;
    }

    return table->entries[mapped].str;
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

lm_atom_t lm_runtime_allocate_atom(lm_runtime_t* runtime, const lm_string_t* str) {
    return lm__atom_hash_table_intern(&runtime->atom_table, str);
}

lm__stack_frame_var_cell_t* lm__stack_frame_var_cell_alloc() {
    lm__stack_frame_var_cell_t* cell = _LM_ALLOC(lm__stack_frame_var_cell_t);

    lm_list_node_init(&cell->list_node);
    cell->local_vars_count = 0;

    return cell;
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

    lm__stack_frame_var_cell_t* cell = lm__stack_frame_var_cell_alloc();
    lm_list_add_tail(&frame->var_cells_head, &cell->list_node);

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

lm__operand_stack_cell_t* lm__operand_stack_cell_alloc() {
    lm__operand_stack_cell_t* cell = _LM_ALLOC(lm__operand_stack_cell_t);
    lm_list_node_init(&cell->list_node);
    cell->size = 0;

    return cell;
}

void lm__operand_stack_cell_free(lm__operand_stack_cell_t* cell) {
    _LM_FREE(cell);
}

lm__operand_stack_t* lm__operand_stack_alloc() {
    lm__operand_stack_t* stack = _LM_ALLOC(lm__operand_stack_t);
    lm_list_node_init(&stack->cells_head);

    return stack;
}

void lm__operand_stack_cell_free_iterator(lm_list_node_t* node, void* ctx) {
    lm__operand_stack_cell_free(lm_list_entry(node, lm__operand_stack_cell_t, list_node));
}

void lm__operand_stack_free(lm__operand_stack_t* stack) {
    lm_list_iterate_safe(&stack->cells_head, lm__operand_stack_cell_free_iterator, NULL);
    _LM_FREE(stack);
}

void lm__operand_stack_push(lm__operand_stack_t* stack, lm_value_t value) {
    lm__operand_stack_cell_t* tail = NULL;

    if (lm_list_empty(&stack->cells_head)) {
        tail = lm__operand_stack_cell_alloc();
        lm_list_add_tail(&tail->list_node, &stack->cells_head);
    } else {
        tail = lm_list_entry(lm_list_tail(&stack->cells_head),
                             lm__operand_stack_cell_t, list_node);
    }

    if (tail->size == LM_RUNTIME_STACK_CELL_SIZE) {
        tail = lm__operand_stack_cell_alloc();
        lm_list_add_tail(&stack->cells_head, &tail->list_node);
    }

    tail->values[tail->size++] = value;
}

lm_value_t lm__operand_stack_pop(lm__operand_stack_t* stack) {
    _LM_ASSERT(!lm_list_empty(&stack->cells_head), "stack is empty");

    lm__operand_stack_cell_t* tail =
            lm_list_entry(lm_list_tail(&stack->cells_head),
                          lm__operand_stack_cell_t, list_node);

    if (tail->size == 0) {
        lm_list_remove(lm_list_tail(&stack->cells_head));
        lm__operand_stack_cell_free(tail);

        tail = lm_list_entry(lm_list_tail(&stack->cells_head),
                             lm__operand_stack_cell_t, list_node);
    }

    return tail->values[--tail->size];
}

void lm__operand_stack_peek(lm__operand_stack_t* stack) {
    lm__operand_stack_cell_t* tail =
            lm_list_entry(lm_list_tail(&stack->cells_head),
                          lm__operand_stack_cell_t, list_node);

    if (tail->size == 0) {
        lm_list_remove(lm_list_tail(&stack->cells_head));
        lm__operand_stack_cell_free(tail);

        tail = lm_list_entry(lm_list_tail(&stack->cells_head),
                             lm__operand_stack_cell_t, list_node);
    }

    lm_value_t value = tail->values[tail->size - 1];
    lm__operand_stack_push(stack, value);
}

lm_bool lm__operand_stack_is_empty(lm__operand_stack_t* stack) {
    if (lm_list_empty(&stack->cells_head)) {
        return LM_TRUE;
    }

    lm__operand_stack_cell_t* tail =
            lm_list_entry(lm_list_tail(&stack->cells_head),
                          lm__operand_stack_cell_t, list_node);

    return tail->size == 0;
}

lm_atom_t lm__context_alloc_atom(lm_string_t* str, void* ctx) {
    lm_runtime_t* runtime = ctx;
    return lm_runtime_allocate_atom(runtime, str);
}

lm_context_t* lm_context_alloc(lm_runtime_t* runtime) {
    _LM_ASSERT_NOT_NULL(runtime, "runtime cannot be null");

    lm_context_t* context = _LM_ALLOC(lm_context_t);

    lm_list_node_init(&context->list_node);
    lm_list_node_init(&context->stack_frame_head);
    context->runtime = runtime;
    context->operand_stack = lm__operand_stack_alloc();
    context->pc = NULL;
    context->code = lm__byte_array_alloc();

    lm__byte_code_generator_intern_string_ctx_t generator_ctx = {
            .ctx = runtime,
            .pfn = lm__context_alloc_atom};

    context->code_generator = lm__byte_code_generator_alloc(generator_ctx);

    lm__context_push_frame(context, context->pc);

    // todo: context->local_allocator = runtime->local_allocator;

    return context;
}

void lm_context_free(lm_context_t* context) {
    lm__context_pop_frame(context);

    lm__operand_stack_free(context->operand_stack);
    lm__byte_code_generator_free(context->code_generator);
    lm__byte_array_free(context->code);

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

void lm__context_generate(lm_context_t* context, lm__ast_statement_t* program, lm_bool eval_mode) {
    //lm__byte_code_generator_clear(context->code_generator);
    lm__byte_code_generator_generate(context->code_generator, program, eval_mode);
    const lm__byte_array_t* array = lm__byte_code_generator_get_array(context->code_generator);

    lm__byte_array_push_array(context->code, array->data, array->size);
}

void lm__context_push_operand(lm_context_t* context, lm_value_t value) {
    lm__operand_stack_push(context->operand_stack, value);
}

lm_value_t lm__context_pop_operand(lm_context_t* context) {
    return lm__operand_stack_pop(context->operand_stack);
}

void lm__context_peek_operand(lm_context_t* context, lm_value_t* value) {
    lm__operand_stack_peek(context->operand_stack);
}

lm__stack_frame_t* lm__context_get_current_frame(lm_context_t* context) {
    return lm_list_entry(lm_list_tail(&context->stack_frame_head), lm__stack_frame_t, list_node);
}

lm__opcode_value_t lm__context_consume_instr(lm_context_t* context) {
    return (lm__opcode_value_t) lm__context_consume_byte(context);
}

uint8_t lm__context_consume_byte(lm_context_t* context) {
    return *_LM_CAST(uint8_t, context->pc++);
}

lm_int lm__context_consume_int(lm_context_t* context) {
    lm_int value = *_LM_CAST(lm_int, context->pc);
    context->pc += sizeof(lm_int);
    return value;
}

lm_float lm__context_consume_float(lm_context_t* context) {
    lm_float value = *_LM_CAST(lm_float, context->pc);
    context->pc += sizeof(lm_float);
    return value;
}

lm_atom_t lm__context_consume_atom(lm_context_t* context) {
    lm_atom_t value = *_LM_CAST(lm_atom_t, context->pc);
    context->pc += sizeof(lm_atom_t);
    return value;
}

void lm__context_run(lm_context_t* context) {
    uint8_t* terminal = lm__byte_array_data(context->code) + lm__byte_array_size(context->code);
    context->pc = lm__byte_array_data(context->code);
    while (context->pc != terminal) {
        lm__opcode_value_t opcode = lm__context_consume_instr(context);
        switch (opcode) {
            case LM__OPCODE_NOP: {
                continue;
            }
            case LM__OPCODE_HALT: {
                return;
            }
            case LM__POP: {
                lm_value_t value = lm__context_pop_operand(context);
                // todo: free value
                break;
            }
            case LM__LOAD_UNDEFINED: {
                lm__context_push_operand(context, lm_value_make_undefined());
                break;
            }
            case LM__LOAD_NULL: {
                lm__context_push_operand(context, lm_value_make_null());
                break;
            }
            case LM__LOAD_BOOL: {
                lm_bool value = lm__context_consume_byte(context);
                lm__context_push_operand(context, lm_value_make_boolean(value));
                break;
            }
            case LM__LOAD_INT: {
                lm_int value = lm__context_consume_int(context);
                lm__context_push_operand(context, lm_value_make_int(value));
                break;
            }
            case LM__LOAD_FLOAT: {
                lm_float value = lm__context_consume_float(context);
                lm__context_push_operand(context, lm_value_make_float(value));
                break;
            }
            case LM__LOAD_STRING: {
                lm_atom_t name = lm__context_consume_atom(context);
                // todo: intern string
                break;
            }
            case LM__DECL_VAR: {
                lm_atom_t name = lm__context_consume_atom(context);
                lm__stack_frame_add_var(lm__context_get_current_frame(context), name);
                break;
            }
            case LM__LOAD_VAR: {
                lm_atom_t name = lm__context_consume_atom(context);
                lm_value_t* var = lm__stack_frame_get_var(lm__context_get_current_frame(context), name);
                lm__context_push_operand(context, *var);
                break;
            }
            case LM__STORE_VAR: {
                lm_atom_t name = lm__context_consume_atom(context);
                lm_value_t* var = lm__stack_frame_get_var(lm__context_get_current_frame(context), name);
                lm_value_t value = lm__context_pop_operand(context);

                *var = value;
                break;
            }
            case LM__OP_ADD:
            case LM__OP_SUB:
            case LM__OP_MUL:
            case LM__OP_DIV:
            case LM__OP_MOD: {
                lm_value_t rhs = lm__context_pop_operand(context);
                lm_value_t lhs = lm__context_pop_operand(context);

                lm_value_t result = lm_value_dispatch_arith(lhs, rhs, opcode);
                lm__context_push_operand(context, result);

                // todo: free lhs and rhs if needed

                break;
            }
            case LM__OP_AND:
            case LM__OP_OR: {
                lm_value_t rhs = lm__context_pop_operand(context);
                lm_value_t lhs = lm__context_pop_operand(context);

                lm_value_t result = lm_value_dispatch_logical(lhs, rhs, opcode);
                lm__context_push_operand(context, result);

                break;
            }
            case LM__OP_NEG:
            case LM__OP_NOT: {
                lm_value_t rhs = lm__context_pop_operand(context);

                lm_value_t result = lm_value_dispatch_unary(rhs, opcode);
                lm__context_push_operand(context, result);

                break;
            }
            case LM__OP_EQ:
            case LM__OP_NEQ:
            case LM__OP_LT:
            case LM__OP_LTE:
            case LM__OP_GT:
            case LM__OP_GTE: {
                lm_value_t rhs = lm__context_pop_operand(context);
                lm_value_t lhs = lm__context_pop_operand(context);

                lm_value_t result = lm_value_dispatch_comparison(lhs, rhs, opcode);
                lm__context_push_operand(context, result);

                break;
            }
            case LM__OP_JMP: {
                lm_int offset = lm__context_consume_int(context);
                context->pc += offset;

                break;
            }
            case LM__OP_JMP_IF_FALSE: {
                lm_int offset = lm__context_consume_int(context);

                lm_value_t value = lm__context_pop_operand(context);
                value = lm_value_to_boolean(value);

                if (!value.v.bool_value) {
                    context->pc += offset;
                }
                break;
            }
            case LM__OP_JMP_IF_TRUE: {
                lm_int offset = lm__context_consume_int(context);

                lm_value_t value = lm__context_pop_operand(context);
                value = lm_value_to_boolean(value);

                if (value.v.bool_value) {
                    context->pc += offset;
                }
                break;
            }
            case LM__OP_CALL:
            case LM__OP_RET:
            case LM__PUSH:
                _LM_ASSERT(0, "not implemented");
        }
    }
}

lm__ast_statement_t* lm__context_generate_ast(lm_context_t* context, const char* str, const char* file_name) {
    lm_lexer_t* lexer = lm_lexer_alloc(str, file_name);
    lm_parser_t* parser = lm_parser_alloc(lexer);

    lm__ast_statement_t* program = _LM_CAST(lm__ast_statement_t, lm_parser_parse(parser));

    lm_parser_free(parser);
    lm_lexer_free(lexer);

    return program;
}

lm_value_t lm_eval(lm_context_t* context, const char* str, lm_eval_scope_t scope) {
    lm__ast_statement_t* program = lm__context_generate_ast(context, str, "<eval>");
    lm__context_generate(context, program, LM_TRUE);
    lm__ast_statement_free(program);

    lm_print_byte_code(context->code->data, context->code->size);

    lm__context_run(context);

    if (!lm__operand_stack_is_empty(context->operand_stack)) {
        return lm__operand_stack_pop(context->operand_stack);
    }

    return lm_value_make_undefined();
}