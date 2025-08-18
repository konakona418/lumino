#include "ast.h"

void lm__ast_statement_free(lm__ast_statement_t* stmt) {
    if (stmt == NULL) {
        return;
    }

    _LM_ASSERT(stmt->vtbl.free != NULL, "statement vtbl free function is null");

    stmt->vtbl.free(stmt);
}

void lm__ast_statement_free_iterator(lm_list_node_t* node, void* ctx) {
    lm__ast_statement_t* stmt = lm_list_entry(node, lm__ast_statement_t, list_node);
    lm__ast_statement_free(stmt);
}

lm__ast_program_t* lm__ast_program_alloc(lm_list_node_t* stmts) {
    lm__ast_program_t* program = _LM_ALLOC(lm__ast_program_t);
    lm_list_node_init(&program->list_node);

    program->stmt_type = LM_AST_STATEMENT_TYPE_PROGRAM;
    program->vtbl.free = lm__ast_program_free;

    program->stmts = stmts;

    return program;
}

void lm__ast_program_free(lm__ast_statement_t* stmt) {
    lm__ast_program_t* program = (lm__ast_program_t*) stmt;

    lm_list_iterate_safe(program->stmts, lm__ast_statement_free_iterator, NULL);
    _LM_FREE(program->stmts);

    _LM_FREE(stmt);
}

lm__ast_block_t* lm__ast_block_alloc(lm_list_node_t* stmts) {
    lm__ast_block_t* block = _LM_ALLOC(lm__ast_block_t);
    lm_list_node_init(&block->list_node);

    block->stmt_type = LM_AST_STATEMENT_TYPE_BLOCK;
    block->vtbl.free = lm__ast_block_free;

    block->stmts = stmts;

    return block;
}

void lm__ast_block_free(lm__ast_statement_t* stmt) {
    lm__ast_block_t* block = (lm__ast_block_t*) stmt;

    lm_list_iterate_safe(block->stmts, lm__ast_statement_free_iterator, NULL);

    _LM_FREE(block->stmts);
    _LM_FREE(stmt);
}

lm__ast_if_t* lm__ast_if_alloc(
        lm__ast_statement_t* condition,
        lm__ast_statement_t* then_body,
        lm__ast_statement_t* else_body) {
    lm__ast_if_t* if_stmt = _LM_ALLOC(lm__ast_if_t);
    lm_list_node_init(&if_stmt->list_node);

    if_stmt->stmt_type = LM_AST_STATEMENT_TYPE_IF;
    if_stmt->vtbl.free = lm__ast_if_free;

    if_stmt->condition = condition;
    if_stmt->then_body = then_body;
    if_stmt->else_body = else_body;

    return if_stmt;
}

void lm__ast_if_free(lm__ast_statement_t* stmt) {
    lm__ast_if_t* if_stmt = (lm__ast_if_t*) stmt;

    lm__ast_statement_free(if_stmt->condition);
    lm__ast_statement_free(if_stmt->then_body);
    lm__ast_statement_free(if_stmt->else_body);

    _LM_FREE(stmt);
}

lm__ast_while_t* lm__ast_while_alloc(lm__ast_statement_t* cond, lm__ast_statement_t* body) {
    lm__ast_while_t* while_stmt = _LM_ALLOC(lm__ast_while_t);
    lm_list_node_init(&while_stmt->list_node);

    while_stmt->stmt_type = LM_AST_STATEMENT_TYPE_WHILE;
    while_stmt->vtbl.free = lm__ast_while_free;

    while_stmt->condition_stmt = cond;
    while_stmt->body = body;

    return while_stmt;
}

void lm__ast_while_free(lm__ast_statement_t* stmt) {
    lm__ast_while_t* while_stmt = (lm__ast_while_t*) stmt;

    lm__ast_statement_free(while_stmt->condition_stmt);
    lm__ast_statement_free(while_stmt->body);

    _LM_FREE(stmt);
}

lm__ast_break_t* lm__ast_break_alloc() {
    lm__ast_break_t* break_stmt = _LM_ALLOC(lm__ast_break_t);
    lm_list_node_init(&break_stmt->list_node);

    break_stmt->stmt_type = LM_AST_STATEMENT_TYPE_BREAK;
    break_stmt->vtbl.free = lm__ast_break_free;

    return break_stmt;
}

void lm__ast_break_free(lm__ast_statement_t* stmt) {
    _LM_FREE(stmt);
}

lm__ast_continue_t* lm__ast_continue_alloc() {
    lm__ast_continue_t* continue_stmt = _LM_ALLOC(lm__ast_continue_t);
    lm_list_node_init(&continue_stmt->list_node);

    continue_stmt->stmt_type = LM_AST_STATEMENT_TYPE_CONTINUE;
    continue_stmt->vtbl.free = lm__ast_continue_free;

    return continue_stmt;
}

void lm__ast_continue_free(lm__ast_statement_t* stmt) {
    _LM_FREE(stmt);
}

lm__ast_decl_t* lm__ast_decl_alloc(lm_string_t* name, lm__ast_statement_t* value_stmt) {
    lm__ast_decl_t* decl = _LM_ALLOC(lm__ast_decl_t);
    lm_list_node_init(&decl->list_node);

    decl->stmt_type = LM_AST_STATEMENT_TYPE_DECL;
    decl->vtbl.free = lm__ast_decl_free;

    decl->name = name;
    decl->value_stmt = value_stmt;

    return decl;
}

void lm__ast_decl_free(lm__ast_statement_t* stmt) {
    lm__ast_decl_t* decl = (lm__ast_decl_t*) stmt;

    lm_string_free(decl->name);
    lm__ast_statement_free(decl->value_stmt);
    _LM_FREE(stmt);
}

lm__ast_assign_expr_t* lm__ast_assign_expr_alloc(lm__ast_expression_t* lhs, lm__ast_expression_t* rhs) {
    lm__ast_assign_expr_t* expr = _LM_ALLOC(lm__ast_assign_expr_t);
    lm_list_node_init(&expr->list_node);

    expr->stmt_type = LM_AST_STATEMENT_TYPE_EXPRESSION;
    expr->expr_type = LM_AST_EXPRESSION_TYPE_ASSIGN;
    expr->result_discardable = LM_FALSE;
    expr->vtbl.free = lm__ast_assign_expr_free;

    expr->lhs = lhs;
    expr->rhs = rhs;

    return expr;
}

void lm__ast_assign_expr_free(lm__ast_statement_t* expr) {
    lm__ast_assign_expr_t* assign_expr = (lm__ast_assign_expr_t*) expr;

    lm__ast_statement_free(_LM_CAST(lm__ast_statement_t, assign_expr->lhs));
    lm__ast_statement_free(_LM_CAST(lm__ast_statement_t, assign_expr->rhs));
    _LM_FREE(expr);
}

lm__ast_binary_expr_t* lm__ast_binary_expr_alloc(
        lm__ast_binary_expr_type_t type,
        lm__ast_expression_t* lhs,
        lm__ast_expression_t* rhs) {
    lm__ast_binary_expr_t* binary_expr = _LM_ALLOC(lm__ast_binary_expr_t);
    lm_list_node_init(&binary_expr->list_node);

    binary_expr->stmt_type = LM_AST_STATEMENT_TYPE_EXPRESSION;
    binary_expr->expr_type = LM_AST_EXPRESSION_TYPE_BINARY;
    binary_expr->result_discardable = LM_FALSE;
    binary_expr->vtbl.free = lm__ast_binary_expr_free;

    binary_expr->type = type;
    binary_expr->lhs = lhs;
    binary_expr->rhs = rhs;

    return binary_expr;
}

void lm__ast_binary_expr_free(lm__ast_statement_t* expr) {
    lm__ast_binary_expr_t* binary_expr = (lm__ast_binary_expr_t*) expr;

    lm__ast_statement_free(_LM_CAST(lm__ast_statement_t, binary_expr->lhs));
    lm__ast_statement_free(_LM_CAST(lm__ast_statement_t, binary_expr->rhs));
    _LM_FREE(expr);
}


lm__ast_unary_expr_t* lm__ast_unary_expr_alloc(
        lm__ast_unary_expr_type_t type,
        lm__ast_expression_t* rhs) {
    lm__ast_unary_expr_t* unary_expr = _LM_ALLOC(lm__ast_unary_expr_t);
    lm_list_node_init(&unary_expr->list_node);

    unary_expr->stmt_type = LM_AST_STATEMENT_TYPE_EXPRESSION;
    unary_expr->expr_type = LM_AST_EXPRESSION_TYPE_UNARY;
    unary_expr->result_discardable = LM_FALSE;
    unary_expr->vtbl.free = lm__ast_unary_expr_free;

    unary_expr->type = type;
    unary_expr->rhs = rhs;

    return unary_expr;
}

void lm__ast_unary_expr_free(lm__ast_statement_t* expr) {
    lm__ast_unary_expr_t* unary_expr = (lm__ast_unary_expr_t*) expr;
    lm__ast_statement_free(_LM_CAST(lm__ast_statement_t, unary_expr->rhs));
    _LM_FREE(unary_expr);
}

lm__ast_literal_expr_t* lm__ast_literal_expr_alloc(
        lm__ast_literal_expr_type_t type,
        lm__ast_literal_expr_data_t value) {
    lm__ast_literal_expr_t* literal_expr = _LM_ALLOC(lm__ast_literal_expr_t);
    lm_list_node_init(&literal_expr->list_node);

    literal_expr->stmt_type = LM_AST_STATEMENT_TYPE_EXPRESSION;
    literal_expr->expr_type = LM_AST_EXPRESSION_TYPE_LITERAL;
    literal_expr->result_discardable = LM_FALSE;
    literal_expr->vtbl.free = lm__ast_literal_expr_free;

    literal_expr->type = type;
    literal_expr->data = value;

    return literal_expr;
}

void lm__ast_literal_expr_free(lm__ast_statement_t* expr) {
    lm__ast_literal_expr_t* literal_expr = (lm__ast_literal_expr_t*) expr;

    if (literal_expr->type == LM_AST_LITERAL_EXPR_TYPE_STRING) {
        lm_string_free(literal_expr->data.str_val);
    }

    _LM_FREE(expr);
}


lm__ast_var_expr_t* lm__ast_var_expr_alloc(lm_string_t* name) {
    lm__ast_var_expr_t* var_expr = _LM_ALLOC(lm__ast_var_expr_t);
    lm_list_node_init(&var_expr->list_node);

    var_expr->stmt_type = LM_AST_STATEMENT_TYPE_EXPRESSION;
    var_expr->expr_type = LM_AST_EXPRESSION_TYPE_VAR;
    var_expr->result_discardable = LM_FALSE;
    var_expr->vtbl.free = lm__ast_var_expr_free;

    var_expr->name = name;

    return var_expr;
}

void lm__ast_var_expr_free(lm__ast_statement_t* expr) {
    lm__ast_var_expr_t* var_expr = (lm__ast_var_expr_t*) expr;

    lm_string_free(var_expr->name);
    _LM_FREE(expr);
}