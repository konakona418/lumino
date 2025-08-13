#include "common.h"
#include <stddef.h>

typedef enum lm__ast_statement_type_e {
    LM_AST_STATEMENT_TYPE_PROGRAM,
    LM_AST_STATEMENT_TYPE_BLOCK,
    LM_AST_STATEMENT_TYPE_EXPRESSION,

    LM_AST_STATEMENT_TYPE_IF,

    LM_AST_STATEMENT_TYPE_FOR,
    LM_AST_STATEMENT_TYPE_WHILE,

    LM_AST_STATEMENT_TYPE_BREAK,
    LM_AST_STATEMENT_TYPE_CONTINUE,

    LM_AST_STATEMENT_TYPE_FUNC,
    LM_AST_STATEMENT_TYPE_RETURN,

    LM_AST_STATEMENT_TYPE_DECL,
} lm__ast_statement_type_t;

#define LM_AST_STATEMENT_HEADER \
    lm__ast_statement_type_t stmt_type;

typedef struct lm__ast_statement_s {
    LM_AST_STATEMENT_HEADER
} lm__ast_statement_t;

typedef struct lm__ast_program_s {
    LM_AST_STATEMENT_HEADER

    lm_list_node_t stmts;
} lm__ast_program_t;

typedef struct lm__ast_block_s {
    LM_AST_STATEMENT_HEADER

    lm_list_node_t stmts;
} lm__ast_block_t;

typedef struct lm__ast_if_s {
    LM_AST_STATEMENT_HEADER

    lm__ast_statement_t* condition;
    lm__ast_statement_t* then_body;
    lm__ast_statement_t* else_body;
} lm__ast_if_t;

typedef struct lm__ast_for_s {
    LM_AST_STATEMENT_HEADER

    lm__ast_statement_t* init_stmt;
    lm__ast_statement_t* condition_stmt;
    lm__ast_statement_t* post_stmt;
    lm__ast_statement_t* body;
} lm__ast_for_t;

typedef struct lm__ast_while_s {
    LM_AST_STATEMENT_HEADER

    lm__ast_statement_t* condition_stmt;
    lm__ast_statement_t* body;
} lm__ast_while_t;

typedef struct lm__ast_break_s {
    LM_AST_STATEMENT_HEADER
} lm__ast_break_t;

typedef struct lm__ast_continue_s {
    LM_AST_STATEMENT_HEADER
} lm__ast_continue_t;

typedef struct lm__ast_func_s {
    LM_AST_STATEMENT_HEADER

    lm_string_t* name;
    // todo
} lm__ast_func_t;

typedef struct lm__ast_return_s {
    LM_AST_STATEMENT_HEADER

    lm__ast_statement_t* value_stmt;
} lm__ast_return_t;

typedef struct lm__ast_decl_s {
    LM_AST_STATEMENT_HEADER

    lm_string_t* name;
    lm__ast_statement_t* value_stmt;
} lm__ast_decl_t;

typedef enum lm__ast_expression_type_e {
    LM_AST_EXPRESSION_TYPE_ASSIGN,
    LM_AST_EXPRESSION_TYPE_BINARY,
    LM_AST_EXPRESSION_TYPE_UNARY,
    LM_AST_EXPRESSION_TYPE_LITERAL,
    LM_AST_EXPRESSION_TYPE_VAR
} lm__ast_expression_type_t;

#define LM_AST_EXPRESSION_HEADER \
    lm__ast_expression_type_t expr_type;

typedef struct lm__ast_expression_s {
    LM_AST_STATEMENT_HEADER
    LM_AST_EXPRESSION_HEADER
} lm__ast_expression_t;

typedef struct lm__ast_assign_expr_s {
    LM_AST_STATEMENT_HEADER
    LM_AST_EXPRESSION_HEADER

    lm__ast_expression_t* lhs;
    lm__ast_expression_t* rhs;
} lm__ast_assign_expr_t;

typedef enum lm__ast_binary_expr_type_e {
    LM_AST_BINARY_EXPR_TYPE_ADD,
    LM_AST_BINARY_EXPR_TYPE_SUB,
    LM_AST_BINARY_EXPR_TYPE_MUL,
    LM_AST_BINARY_EXPR_TYPE_DIV,
    LM_AST_BINARY_EXPR_TYPE_MOD,
    LM_AST_BINARY_EXPR_TYPE_EQ,
    LM_AST_BINARY_EXPR_TYPE_NEQ,
    LM_AST_BINARY_EXPR_TYPE_LT,
    LM_AST_BINARY_EXPR_TYPE_LTE,
    LM_AST_BINARY_EXPR_TYPE_GT,
    LM_AST_BINARY_EXPR_TYPE_GTE,
    LM_AST_BINARY_EXPR_TYPE_POW,
} lm__ast_binary_expr_type_t;

typedef struct lm__ast_binary_expr_s {
    LM_AST_STATEMENT_HEADER
    LM_AST_EXPRESSION_HEADER

    lm__ast_binary_expr_type_t type;
    lm__ast_expression_t* lhs;
    lm__ast_expression_t* rhs;
} lm__ast_binary_expr_t;

typedef enum lm__ast_unary_expr_type_e {
    LM_AST_UNARY_EXPR_TYPE_NEG,
} lm__ast_unary_expr_type_t;

typedef struct lm__ast_unary_expr_s {
    LM_AST_STATEMENT_HEADER
    LM_AST_EXPRESSION_HEADER

    lm__ast_unary_expr_type_t type;
    lm__ast_expression_t* rhs;
} lm__ast_unary_expr_t;

typedef enum lm__ast_literal_expr_type_e {
    LM_AST_LITERAL_EXPR_TYPE_BOOL,
    LM_AST_LITERAL_EXPR_TYPE_FLOAT,
    LM_AST_LITERAL_EXPR_TYPE_INT,
    LM_AST_LITERAL_EXPR_TYPE_STRING,
    LM_AST_LITERAL_EXPR_TYPE_NULL,
} lm__ast_literal_expr_type_t;

typedef struct lm__ast_literal_expr_s {
    LM_AST_STATEMENT_HEADER
    LM_AST_EXPRESSION_HEADER

    lm__ast_literal_expr_type_t type;
    // todo
} lm__ast_literal_expr_t;

typedef struct lm__ast_var_expr_s {
    LM_AST_STATEMENT_HEADER
    LM_AST_EXPRESSION_HEADER

    lm_string_t* name;
} lm__ast_var_expr_t;
