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
    size_t old_size = array->size;
    uint8_t* old_data = array->data;

    array->capacity = new_capacity;
    array->data = _LM_CALLOC(uint8_t, new_capacity);

    memcpy(array->data, old_data, old_size);
    _LM_FREE(old_data);
}

void lm__byte_array_reserve(lm__byte_array_t* array, size_t size) {
    size_t required = array->size + size;
    if (required > array->capacity) {
        size_t new_capacity = array->capacity ? array->capacity : 64;

        while (new_capacity < required) {
            new_capacity *= _LM_BYTE_ARRAY_GROWTH_FACTOR;
        }

        lm__byte_array_realloc(array, new_capacity);
    }
}

void lm__byte_array_push(lm__byte_array_t* array, uint8_t value) {
    if (array->size >= array->capacity) {
        lm__byte_array_realloc(array, array->capacity * _LM_BYTE_ARRAY_GROWTH_FACTOR);
    }

    array->data[array->size++] = value;
}

void lm__byte_array_push_array(lm__byte_array_t* array, uint8_t* values, size_t size) {
    if (array->size + size > array->capacity) {
        size_t new_capacity = array->capacity ? array->capacity : 64;
        while (new_capacity < array->size + size) {
            new_capacity *= _LM_BYTE_ARRAY_GROWTH_FACTOR;
        }

        lm__byte_array_realloc(array, new_capacity);
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

uint8_t* lm__byte_array_at(lm__byte_array_t* array, size_t index) {
    return array->data + index;
}

size_t lm__byte_array_size(lm__byte_array_t* array) {
    return array->size;
}

const char* lm__byte_code_error_what(lm_error_t* error) {
    return _LM_CAST(lm_byte_code_error_t, error)->msg->data;
}

void lm__byte_code_error_free(lm_error_t* error) {
    lm_string_free(_LM_CAST(lm_byte_code_error_t, error)->msg);
    _LM_FREE(error);
}

lm_byte_code_error_t* lm__byte_code_error_alloc(const char* msg) {
    lm_byte_code_error_t* error = _LM_ALLOC(lm_byte_code_error_t);
    error->vtbl.free = lm__byte_code_error_free;
    error->vtbl.what = lm__byte_code_error_what;

    size_t buf_size = snprintf(NULL, 0, "Bytecode generator error: %s", msg);
    char* buf = _LM_ALLOC_ARRAY(char, buf_size + 1);

    int n = snprintf(buf, buf_size + 1, "Bytecode generator error: %s", msg);

    _LM_ASSERT(n >= 0, "snprintf failed");

    error->msg = lm_string_from(buf, buf_size + 1);

    return error;
}

lm__byte_code_generator_loop_context_offset_t* lm__byte_code_generator_loop_context_offset_alloc(lm_ssize_t offset) {
    lm__byte_code_generator_loop_context_offset_t* offset_node = _LM_ALLOC(lm__byte_code_generator_loop_context_offset_t);
    lm_list_node_init(&offset_node->list_node);
    offset_node->offset = offset;
    return offset_node;
}

void lm__byte_code_generator_loop_context_offset_free(lm__byte_code_generator_loop_context_offset_t* offset) {
    _LM_FREE(offset);
}

lm__byte_code_generator_loop_context_t* lm__byte_code_generator_loop_context_alloc() {
    lm__byte_code_generator_loop_context_t* context = _LM_ALLOC(lm__byte_code_generator_loop_context_t);
    lm_list_node_init(&context->list_node);
    lm_list_node_init(&context->break_offset_head);
    lm_list_node_init(&context->continue_offset_head);

    return context;
}

void lm__byte_code_generator_loop_context_offset_free_iterator(lm_list_node_t* node, void* ctx) {
    lm__byte_code_generator_loop_context_offset_t* offset =
            lm_list_entry(node, lm__byte_code_generator_loop_context_offset_t, list_node);
    lm__byte_code_generator_loop_context_offset_free(offset);
}

void lm__byte_code_generator_loop_context_free(lm__byte_code_generator_loop_context_t* ctx) {
    lm_list_iterate_safe(&ctx->break_offset_head,
                         lm__byte_code_generator_loop_context_offset_free_iterator, NULL);
    lm_list_iterate_safe(&ctx->continue_offset_head,
                         lm__byte_code_generator_loop_context_offset_free_iterator, NULL);
    _LM_FREE(ctx);
}

void lm__byte_code_generator_loop_context_add_continue_offset(
        lm__byte_code_generator_loop_context_t* loop_ctx,
        lm_ssize_t offset) {
    lm__byte_code_generator_loop_context_offset_t* offset_node =
            lm__byte_code_generator_loop_context_offset_alloc(offset);
    lm_list_add_tail(&loop_ctx->continue_offset_head, &offset_node->list_node);
}

void lm__byte_code_generator_loop_context_add_break_offset(
        lm__byte_code_generator_loop_context_t* loop_ctx,
        lm_ssize_t offset) {
    lm__byte_code_generator_loop_context_offset_t* offset_node =
            lm__byte_code_generator_loop_context_offset_alloc(offset);
    lm_list_add_tail(&loop_ctx->break_offset_head, &offset_node->list_node);
}

void lm__byte_code_generator_emit_error(lm__byte_code_generator_t* generator, const char* msg) {
    if (!generator->error_handler) {
        return;
    }

    lm_byte_code_error_t* error = lm__byte_code_error_alloc(msg);
    generator->error_handler(_LM_CAST(lm_error_t, error));
}

void lm__byte_code_generator_add_loop_context(lm__byte_code_generator_t* generator) {
    lm__byte_code_generator_loop_context_t* ctx = lm__byte_code_generator_loop_context_alloc();
    lm_list_add_tail(&generator->loop_ctx_head, &ctx->list_node);
}

void lm__byte_code_generator_remove_loop_context(lm__byte_code_generator_t* generator) {
    lm_list_node_t* node = lm_list_tail(&generator->loop_ctx_head);
    lm_list_remove(node);

    lm__byte_code_generator_loop_context_free(
            lm_list_entry(node, lm__byte_code_generator_loop_context_t, list_node));
}

void lm__byte_code_generator_add_break_to_loop_context(lm__byte_code_generator_t* generator, lm_ssize_t offset) {
    _LM_ASSERT(!lm_list_empty(&generator->loop_ctx_head), "no loop context");
    lm__byte_code_generator_loop_context_add_break_offset(
            lm_list_entry(
                    lm_list_tail(&generator->loop_ctx_head),
                    lm__byte_code_generator_loop_context_t, list_node),
            offset);
}

void lm__byte_code_generator_add_continue_to_loop_context(lm__byte_code_generator_t* generator, lm_ssize_t offset) {
    _LM_ASSERT(!lm_list_empty(&generator->loop_ctx_head), "no loop context");
    lm__byte_code_generator_loop_context_add_continue_offset(
            lm_list_entry(
                    lm_list_tail(&generator->loop_ctx_head),
                    lm__byte_code_generator_loop_context_t, list_node),
            offset);
}

lm__byte_code_generator_t* lm__byte_code_generator_alloc(lm__byte_code_generator_intern_string_ctx_t intern_string_ctx) {
    lm__byte_code_generator_t* generator = _LM_ALLOC(lm__byte_code_generator_t);
    generator->array = lm__byte_array_alloc();
    lm_list_node_init(&generator->loop_ctx_head);

    generator->intern_string_ctx = intern_string_ctx;
    generator->program = NULL;

    return generator;
}

void lm__byte_code_generator_clear(lm__byte_code_generator_t* generator) {
    lm__byte_array_free(generator->array);

    generator->array = lm__byte_array_alloc();
    generator->program = NULL;
}

void lm__byte_code_generator_hack_last_expr(lm__ast_program_t* program) {
    lm__ast_statement_t* stmt = lm_list_entry(lm_list_tail(program->stmts), lm__ast_statement_t, list_node);
    if (stmt->stmt_type == LM_AST_STATEMENT_TYPE_EXPRESSION) {
        lm__ast_expression_t* expr = _LM_CAST(lm__ast_expression_t, stmt);
        expr->result_discardable = LM_FALSE;
    }
}

void lm__byte_code_generator_generate(
        lm__byte_code_generator_t* generator,
        lm__ast_statement_t* program, lm_bool eval_mode) {
    if (eval_mode) {
        lm__byte_code_generator_hack_last_expr(_LM_CAST(lm__ast_program_t, program));
    }

    generator->program = program;
    lm__byte_code_generator_generate_program(generator, generator->program, eval_mode);
}

void lm__byte_code_generator_generate_statement_iterator(lm_list_node_t* node, void* ctx) {
    lm__ast_statement_t* stmt = lm_list_entry(node, lm__ast_statement_t, list_node);
    lm__byte_code_generator_t* generator = ctx;

    lm__byte_code_generator_generate_statement(generator, stmt);
}

void lm__byte_code_generator_generate_block(lm__byte_code_generator_t* generator, lm__ast_statement_t* stmt) {
    lm__ast_block_t* block = _LM_CAST(lm__ast_block_t, stmt);
    lm_list_iterate(block->stmts, lm__byte_code_generator_generate_statement_iterator, generator);
}

const lm__byte_array_t* lm__byte_code_generator_get_array(lm__byte_code_generator_t* generator) {
    return generator->array;
}

void lm__byte_code_generator_generate_program(lm__byte_code_generator_t* generator, lm__ast_statement_t* stmt, lm_bool eval_mode) {
    lm__ast_program_t* program = _LM_CAST(lm__ast_program_t, stmt);
    lm_list_iterate(program->stmts, lm__byte_code_generator_generate_statement_iterator, generator);
    // lm__byte_code_generator_emit(generator, LM__OPCODE_HALT);
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
            lm__byte_code_generator_generate_block(generator, stmt);
            break;
        case LM_AST_STATEMENT_TYPE_IF:
            lm__byte_code_generator_generate_if_stmt(generator, stmt);
            break;
        case LM_AST_STATEMENT_TYPE_WHILE:
            lm__byte_code_generator_generate_while_stmt(generator, stmt);
            break;
        case LM_AST_STATEMENT_TYPE_BREAK:
            lm__byte_code_generator_generate_break_stmt(generator, stmt);
            break;
        case LM_AST_STATEMENT_TYPE_CONTINUE:
            lm__byte_code_generator_generate_continue_stmt(generator, stmt);
            break;
        case LM_AST_STATEMENT_TYPE_FOR:
        case LM_AST_STATEMENT_TYPE_FUNC:
        case LM_AST_STATEMENT_TYPE_RETURN:
        case LM_AST_STATEMENT_TYPE_INCLUDE:
        case LM_AST_STATEMENT_TYPE_DEFINE:
            _LM_ASSERT(0, "not implemented");
            break;
    }
}

void lm__byte_code_generator_generate_declaration(lm__byte_code_generator_t* generator, lm__ast_statement_t* stmt) {
    lm__ast_decl_t* decl = _LM_CAST(lm__ast_decl_t, stmt);

    if (decl->value_stmt) {
        lm__byte_code_generator_generate_expression(generator, _LM_CAST(lm__ast_expression_t, decl->value_stmt));
    } else {
        lm__byte_code_generator_emit(generator, LM__LOAD_NULL);
    }

    lm_atom_t atom = lm__byte_code_generator_alloc_atom(generator, decl->name);

    lm__byte_code_generator_emit(generator, LM__DECL_VAR);
    lm__byte_code_generator_emit_atom(generator, atom);

    lm__byte_code_generator_emit(generator, LM__STORE_VAR);
    lm__byte_code_generator_emit_atom(generator, atom);
}

void lm__byte_code_generator_generate_if_stmt(lm__byte_code_generator_t* generator, lm__ast_statement_t* stmt) {
    lm__ast_if_t* if_stmt = _LM_CAST(lm__ast_if_t, stmt);
    lm__byte_code_generator_generate_expression(generator, _LM_CAST(lm__ast_expression_t, if_stmt->condition));
    lm__byte_code_generator_emit(generator, LM__OP_JMP_IF_FALSE);

    size_t jump_to_else_param_pos = lm__byte_array_size(generator->array);
    lm__byte_code_generator_emit_ssize_t(generator, 0);
    size_t jump_to_else_begin_pos = lm__byte_array_size(generator->array);

    lm__byte_code_generator_generate_statement(generator, if_stmt->then_body);

    if (if_stmt->else_body) {
        lm__byte_code_generator_emit(generator, LM__OP_JMP);

        size_t jump_to_end_param_pos = lm__byte_array_size(generator->array);
        lm__byte_code_generator_emit_ssize_t(generator, 0);
        size_t jump_to_end_begin_pos = lm__byte_array_size(generator->array);

        ssize_t jump_over_then_body_offset = lm__byte_array_size(generator->array) - jump_to_else_begin_pos;
        *lm__byte_array_at_ssize_t(generator->array, jump_to_else_param_pos) = jump_over_then_body_offset;

        lm__byte_code_generator_generate_statement(generator, if_stmt->else_body);

        ssize_t jump_to_end_offset = lm__byte_array_size(generator->array) - jump_to_end_begin_pos;

        *lm__byte_array_at_ssize_t(generator->array, jump_to_end_param_pos) = jump_to_end_offset;
    } else {
        ssize_t jump_to_end_offset = lm__byte_array_size(generator->array) - jump_to_else_begin_pos;
        *lm__byte_array_at_ssize_t(generator->array, jump_to_else_param_pos) = jump_to_end_offset;
    }
}

struct lm__byte_code_generator_loop_control_map_iterator_data {
    lm__byte_array_t* array;
    lm_ssize_t offset;
};

void lm__byte_code_generator_loop_control_map_iterator(lm_list_node_t* node, void* ctx) {
    lm__byte_code_generator_loop_context_offset_t* ctx_offset =
            lm_list_entry(node, lm__byte_code_generator_loop_context_offset_t, list_node);

    struct lm__byte_code_generator_loop_control_map_iterator_data* data = ctx;

    *lm__byte_array_at_ssize_t(data->array, ctx_offset->offset) =
            data->offset - (ctx_offset->offset + sizeof(lm_ssize_t));
}

enum lm__loop_control_e {
    LM__LOOP_CONTROL_BREAK,
    LM__LOOP_CONTROL_CONTINUE
};

void lm__byte_code_generator_map_loop_control_offsets(lm__byte_code_generator_t* generator, enum lm__loop_control_e which_control, lm_ssize_t offset) {
    lm_list_node_t* node = lm_list_tail(&generator->loop_ctx_head);
    lm__byte_code_generator_loop_context_t* loop_ctx = lm_list_entry(node, lm__byte_code_generator_loop_context_t, list_node);

    struct lm__byte_code_generator_loop_control_map_iterator_data data = {
            .array = generator->array,
            .offset = offset};

    lm_list_node_t* ctrl = NULL;
    if (which_control == LM__LOOP_CONTROL_BREAK) {
        ctrl = &loop_ctx->break_offset_head;
    } else {
        ctrl = &loop_ctx->continue_offset_head;
    }

    lm_list_iterate(ctrl, lm__byte_code_generator_loop_control_map_iterator, &data);
}

void lm__byte_code_generator_generate_while_stmt(lm__byte_code_generator_t* generator, lm__ast_statement_t* stmt) {
    lm__byte_code_generator_add_loop_context(generator);

    lm__ast_while_t* while_stmt = _LM_CAST(lm__ast_while_t, stmt);

    size_t loop_begin_pos = lm__byte_array_size(generator->array);

    lm__byte_code_generator_generate_expression(generator, _LM_CAST(lm__ast_expression_t, while_stmt->condition_stmt));

    lm__byte_code_generator_emit(generator, LM__OP_JMP_IF_FALSE);
    size_t jump_to_end_param_pos = lm__byte_array_size(generator->array);
    lm__byte_code_generator_emit_ssize_t(generator, 0);
    size_t loop_body_begin_pos = lm__byte_array_size(generator->array);

    lm__byte_code_generator_generate_statement(generator, while_stmt->body);

    lm__byte_code_generator_emit(generator, LM__OP_JMP);
    size_t jump_to_begin_param_pos = lm__byte_array_size(generator->array);
    lm__byte_code_generator_emit_ssize_t(generator, 0);
    size_t loop_body_end_pos = lm__byte_array_size(generator->array);

    ssize_t from_end_to_begin_offset = loop_begin_pos - loop_body_end_pos;
    ssize_t from_begin_to_end_offset = loop_body_end_pos - loop_body_begin_pos;

    *lm__byte_array_at_ssize_t(generator->array, jump_to_begin_param_pos) = from_end_to_begin_offset;
    *lm__byte_array_at_ssize_t(generator->array, jump_to_end_param_pos) = from_begin_to_end_offset;

    lm__byte_code_generator_map_loop_control_offsets(generator, LM__LOOP_CONTROL_BREAK, loop_body_end_pos);
    lm__byte_code_generator_map_loop_control_offsets(generator, LM__LOOP_CONTROL_CONTINUE, loop_begin_pos);

    lm__byte_code_generator_remove_loop_context(generator);
}

void lm__byte_code_generator_generate_break_stmt(lm__byte_code_generator_t* generator, lm__ast_statement_t* stmt) {
    lm__byte_code_generator_emit(generator, LM__OP_JMP);
    lm__byte_code_generator_add_break_to_loop_context(generator, lm__byte_array_size(generator->array));
    lm__byte_code_generator_emit_ssize_t(generator, 0);
}

void lm__byte_code_generator_generate_continue_stmt(lm__byte_code_generator_t* generator, lm__ast_statement_t* stmt) {
    lm__byte_code_generator_emit(generator, LM__OP_JMP);
    lm__byte_code_generator_add_continue_to_loop_context(generator, lm__byte_array_size(generator->array));
    lm__byte_code_generator_emit_ssize_t(generator, 0);
}

void lm__byte_code_generator_generate_expression(lm__byte_code_generator_t* generator, lm__ast_expression_t* expr) {
    switch (expr->expr_type) {
        case LM_AST_EXPRESSION_TYPE_ASSIGN: {
            lm__byte_code_generator_generate_assign_expr(generator, expr);
            break;
        }
        case LM_AST_EXPRESSION_TYPE_BINARY: {
            lm__byte_code_generator_generate_binary_expr(generator, expr);
            break;
        }
        case LM_AST_EXPRESSION_TYPE_UNARY: {
            lm__byte_code_generator_generate_unary_expr(generator, expr);
            break;
        }
        case LM_AST_EXPRESSION_TYPE_LITERAL: {
            lm__byte_code_generator_generate_literal_expr(generator, expr);
            break;
        }
        case LM_AST_EXPRESSION_TYPE_VAR: {
            lm__byte_code_generator_generate_var_access_expr(generator, expr);
            break;
        }
        default: {
            lm__byte_code_generator_emit_error(generator, "unknown expression type");
            break;
        }
    }

    if (expr->result_discardable) {
        lm__byte_code_generator_emit(generator, LM__POP);
    }
}


void lm__byte_code_generator_generate_lvalue_expr(lm__byte_code_generator_t* generator, lm__ast_expression_t* expr) {
    switch (expr->expr_type) {
        case LM_AST_EXPRESSION_TYPE_VAR: {
            lm__ast_var_expr_t* var_expr = _LM_CAST(lm__ast_var_expr_t, expr);
            lm_atom_t atom = lm__byte_code_generator_alloc_atom(generator, var_expr->name);

            lm__byte_code_generator_emit(generator, LM__STORE_VAR);
            lm__byte_code_generator_emit_atom(generator, atom);
            break;
        }
        default:
            lm__byte_code_generator_emit_error(generator, "expecting lvalue expression");
            break;
    }
}

void lm__byte_code_generator_generate_var_access_expr(lm__byte_code_generator_t* generator, lm__ast_expression_t* expr) {
    lm__ast_var_expr_t* var_expr = _LM_CAST(lm__ast_var_expr_t, expr);
    lm_atom_t atom = lm__byte_code_generator_alloc_atom(generator, var_expr->name);

    lm__byte_code_generator_emit(generator, LM__LOAD_VAR);
    lm__byte_code_generator_emit_atom(generator, atom);
}

void lm__byte_code_generator_generate_assign_expr(lm__byte_code_generator_t* generator, lm__ast_expression_t* expr) {
    lm__ast_assign_expr_t* assign_expr = _LM_CAST(lm__ast_assign_expr_t, expr);
    if (assign_expr->assign_type == LM_AST_ASSIGN_EXPR_TYPE_NORMAL) {
        lm__byte_code_generator_generate_expression(generator, assign_expr->rhs);
        lm__byte_code_generator_generate_lvalue_expr(generator, assign_expr->lhs);
    } else {
        lm__byte_code_generator_generate_expression(generator, assign_expr->rhs);
        lm__byte_code_generator_generate_expression(generator, assign_expr->lhs);
        lm__opcode_value_t opcode;
        switch (assign_expr->assign_type) {
            case LM_AST_ASSIGN_EXPR_TYPE_ADD:
                opcode = LM__OP_ADD;
                break;
            case LM_AST_ASSIGN_EXPR_TYPE_SUB:
                opcode = LM__OP_SUB;
                break;
            case LM_AST_ASSIGN_EXPR_TYPE_MUL:
                opcode = LM__OP_MUL;
                break;
            case LM_AST_ASSIGN_EXPR_TYPE_DIV:
                opcode = LM__OP_DIV;
                break;
            default:
                _LM_ASSERT(0, "error branch reached");
                break;
        }

        lm__byte_code_generator_emit(generator, opcode);

        lm__byte_code_generator_generate_lvalue_expr(generator, assign_expr->lhs);
    }
}

lm__opcode_value_t lm__byte_code_generator_binary_expr_type_to_opcode(lm__ast_binary_expr_type_t type) {
    switch (type) {
        case LM_AST_BINARY_EXPR_TYPE_ADD:
            return LM__OP_ADD;
        case LM_AST_BINARY_EXPR_TYPE_SUB:
            return LM__OP_SUB;
        case LM_AST_BINARY_EXPR_TYPE_MUL:
            return LM__OP_MUL;
        case LM_AST_BINARY_EXPR_TYPE_DIV:
            return LM__OP_DIV;
        case LM_AST_BINARY_EXPR_TYPE_MOD:
            return LM__OP_MOD;
        case LM_AST_BINARY_EXPR_TYPE_EQ:
            return LM__OP_EQ;
        case LM_AST_BINARY_EXPR_TYPE_NEQ:
            return LM__OP_NEQ;
        case LM_AST_BINARY_EXPR_TYPE_LT:
            return LM__OP_LT;
        case LM_AST_BINARY_EXPR_TYPE_LTE:
            return LM__OP_LTE;
        case LM_AST_BINARY_EXPR_TYPE_GT:
            return LM__OP_GT;
        case LM_AST_BINARY_EXPR_TYPE_GTE:
            return LM__OP_GTE;
        case LM_AST_BINARY_EXPR_TYPE_LAND:
            return LM__OP_AND;
        case LM_AST_BINARY_EXPR_TYPE_LOR:
            return LM__OP_OR;
        default:
            _LM_ASSERT(0, "not a supported binary expr type");
    }
}

void lm__byte_code_generator_generate_binary_expr(lm__byte_code_generator_t* generator, lm__ast_expression_t* expr) {
    lm__ast_binary_expr_t* binary_expr = _LM_CAST(lm__ast_binary_expr_t, expr);
    lm__byte_code_generator_generate_expression(generator, binary_expr->lhs);
    lm__byte_code_generator_generate_expression(generator, binary_expr->rhs);
    lm__byte_code_generator_emit(
            generator,
            lm__byte_code_generator_binary_expr_type_to_opcode(binary_expr->type));
}

lm__opcode_value_t lm__byte_code_generator_unary_expr_type_to_opcode(lm__ast_unary_expr_type_t type) {
    switch (type) {
        case LM_AST_UNARY_EXPR_TYPE_NEG:
            return LM__OP_NEG;
        case LM_AST_UNARY_EXPR_TYPE_NOT:
            return LM__OP_NOT;
        default:
            _LM_ASSERT(0, "not a supported unary expr type");
    }
}

void lm__byte_code_generator_generate_unary_expr(lm__byte_code_generator_t* generator, lm__ast_expression_t* expr) {
    lm__ast_unary_expr_t* unary_expr = _LM_CAST(lm__ast_unary_expr_t, expr);
    lm__byte_code_generator_generate_expression(generator, unary_expr->rhs);
    lm__byte_code_generator_emit(
            generator,
            lm__byte_code_generator_unary_expr_type_to_opcode(unary_expr->type));
}

void lm__byte_code_generator_generate_literal_expr(lm__byte_code_generator_t* generator, lm__ast_expression_t* expr) {
    lm__ast_literal_expr_t* literal_expr = _LM_CAST(lm__ast_literal_expr_t, expr);
    switch (literal_expr->type) {
        case LM_AST_LITERAL_EXPR_TYPE_BOOL:
            lm__byte_code_generator_emit(generator, LM__LOAD_BOOL);
            lm__byte_code_generator_emit_byte(generator, literal_expr->data.bool_val);
            break;
        case LM_AST_LITERAL_EXPR_TYPE_FLOAT:
            lm__byte_code_generator_emit(generator, LM__LOAD_FLOAT);
            lm__byte_code_generator_emit_f32(generator, literal_expr->data.float_val);
            break;
        case LM_AST_LITERAL_EXPR_TYPE_INT:
            lm__byte_code_generator_emit(generator, LM__LOAD_INT);
            lm__byte_code_generator_emit_i32(generator, literal_expr->data.int_val);
            break;
        case LM_AST_LITERAL_EXPR_TYPE_STRING: {
            lm__byte_code_generator_emit(generator, LM__LOAD_STRING);
            lm_atom_t atom = lm__byte_code_generator_alloc_atom(generator, literal_expr->data.str_val);
            lm__byte_code_generator_emit_atom(generator, atom);
            break;
        }
        case LM_AST_LITERAL_EXPR_TYPE_NULL:
            lm__byte_code_generator_emit(generator, LM__LOAD_NULL);
            break;
    }
}

void lm__byte_code_generator_free(lm__byte_code_generator_t* generator) {
    lm__byte_array_free(generator->array);
    _LM_FREE(generator);
}

lm_atom_t lm__byte_code_generator_alloc_atom(lm__byte_code_generator_t* generator, lm_string_t* str) {
    return generator->intern_string_ctx.pfn(str, generator->intern_string_ctx.ctx);
}

void lm__byte_code_generator_emit(lm__byte_code_generator_t* generator, lm__opcode_value_t opcode) {
    lm__byte_array_push(generator->array, (uint8_t) opcode);
}

void lm__byte_code_generator_emit_byte(lm__byte_code_generator_t* generator, uint8_t value) {
    lm__byte_array_push(generator->array, value);
}

void lm__byte_code_generator_emit_i32(lm__byte_code_generator_t* generator, int32_t value) {
    lm__byte_array_push_i32(generator->array, &value);
}

void lm__byte_code_generator_emit_f32(lm__byte_code_generator_t* generator, float value) {
    lm__byte_array_push_f32(generator->array, &value);
}

void lm__byte_code_generator_emit_atom(lm__byte_code_generator_t* generator, lm_atom_t atom) {
    lm__byte_array_push_size_t(generator->array, &atom);
}

void lm__byte_code_generator_emit_size_t(lm__byte_code_generator_t* generator, size_t value) {
    lm__byte_array_push_size_t(generator->array, &value);
}

void lm__byte_code_generator_emit_ssize_t(lm__byte_code_generator_t* generator, ssize_t value) {
    lm__byte_array_push_ssize_t(generator->array, &value);
}

void lm_print_byte_code(uint8_t* byte_code, size_t size) {
    uint8_t* idx = byte_code;
    while (idx != byte_code + size) {
        printf("%d: ", (lm_int) (idx - byte_code));
        switch ((lm__opcode_value_t) *idx) {
            case LM__NOP: {
                printf("NOP\n");
                idx++;
                break;
            }
            case LM__HALT: {
                printf("HALT\n");
                idx++;
                break;
            }
            case LM__PUSH: {
                printf("PUSH\n");
                idx++;
                break;
            }
            case LM__POP: {
                printf("POP\n");
                idx++;
                break;
            }
            case LM__LOAD_UNDEFINED: {
                printf("LOAD_UNDEFINED\n");
                idx++;
                break;
            }
            case LM__LOAD_NULL: {
                printf("LOAD_NULL\n");
                idx++;
                break;
            }
            case LM__LOAD_BOOL: {
                printf("LOAD_BOOL ");
                idx++;

                if (*_LM_CAST(lm_bool, idx)) {
                    printf("[true]\n");
                } else {
                    printf("[false]\n");
                }
                idx++;
                break;
            }
            case LM__LOAD_INT: {
                printf("LOAD_INT ");
                idx++;

                printf("[%d]\n", *_LM_CAST(lm_int, idx));
                idx += sizeof(lm_int);
                break;
            }
            case LM__LOAD_FLOAT: {
                printf("LOAD_FLOAT ");
                idx++;

                printf("[%f]\n", *_LM_CAST(lm_float, idx));
                idx += sizeof(lm_float);
                break;
            }
            case LM__LOAD_STRING: {
                printf("LOAD_STRING ");
                idx++;

                printf("atom[%zu]\n", *_LM_CAST(lm_atom_t, idx));
                idx += sizeof(lm_atom_t);
                break;
            }
            case LM__DECL_VAR: {
                printf("DECL_VAR ");
                idx++;

                printf("atom[%zu]\n", *_LM_CAST(lm_atom_t, idx));
                idx += sizeof(lm_atom_t);
                break;
            }
            case LM__LOAD_VAR: {
                printf("LOAD_VAR ");
                idx++;

                printf("atom[%zu]\n", *_LM_CAST(lm_atom_t, idx));
                idx += sizeof(lm_atom_t);
                break;
            }
            case LM__STORE_VAR: {
                printf("STORE_VAR ");
                idx++;

                printf("atom[%zu]\n", *_LM_CAST(lm_atom_t, idx));
                idx += sizeof(lm_atom_t);
                break;
            }
            case LM__OP_ADD: {
                printf("OP_ADD\n");
                idx++;
                break;
            }
            case LM__OP_SUB: {
                printf("OP_SUB\n");
                idx++;
                break;
            }
            case LM__OP_MUL: {
                printf("OP_MUL\n");
                idx++;
                break;
            }
            case LM__OP_DIV: {
                printf("OP_DIV\n");
                idx++;
                break;
            }
            case LM__OP_MOD: {
                printf("OP_MOD\n");
                idx++;
                break;
            }
            case LM__OP_AND: {
                printf("OP_AND\n");
                idx++;
                break;
            }
            case LM__OP_OR: {
                printf("OP_OR\n");
                idx++;
                break;
            }
            case LM__OP_NEG: {
                printf("OP_NEG\n");
                idx++;
                break;
            }
            case LM__OP_NOT: {
                printf("OP_NOT\n");
                idx++;
                break;
            }
            case LM__OP_EQ: {
                printf("OP_EQ\n");
                idx++;
                break;
            }
            case LM__OP_NEQ: {
                printf("OP_NEQ\n");
                idx++;
                break;
            }
            case LM__OP_LT: {
                printf("OP_LT\n");
                idx++;
                break;
            }
            case LM__OP_LTE: {
                printf("OP_LTE\n");
                idx++;
                break;
            }
            case LM__OP_GT: {
                printf("OP_GT\n");
                idx++;
                break;
            }
            case LM__OP_GTE: {
                printf("OP_GTE\n");
                idx++;
                break;
            }
            case LM__OP_JMP: {
                printf("JMP ");
                idx++;

                lm_ssize_t diff = *_LM_CAST(lm_ssize_t, idx);
                idx += sizeof(lm_ssize_t);

                printf("[%zd (%lld)]\n", diff, (idx - byte_code) + diff);
                break;
            }
            case LM__OP_JMP_IF_FALSE: {
                printf("JMP_IF_FALSE ");
                idx++;

                lm_ssize_t diff = *_LM_CAST(lm_ssize_t, idx);
                idx += sizeof(lm_ssize_t);

                printf("[%zd (%lld)]\n", diff, (idx - byte_code) + diff);
                break;
            }
            case LM__OP_JMP_IF_TRUE: {
                printf("JMP_IF_TRUE ");
                idx++;

                lm_ssize_t diff = *_LM_CAST(lm_ssize_t, idx);
                idx += sizeof(lm_ssize_t);

                printf("[%zd (%lld)]\n", diff, (idx - byte_code) + diff);
                break;
            }
            case LM__OP_CALL:
            case LM__OP_RET:
                _LM_ASSERT(0, "not implemented");
                break;
        }
    }
}
