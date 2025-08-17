#pragma once

#include "common.h"

#include <stdint.h>

typedef enum lm_value_type_e {
    LM_VALUE_TYPE_NONE = 0,
    LM_VALUE_TYPE_INT,
    LM_VALUE_TYPE_FLOAT,
    LM_VALUE_TYPE_BOOLEAN,
    LM_VALUE_TYPE_NULL,
    LM_VALUE_TYPE_UNDEFINED,
} lm_value_type_t;

struct lm_gc_object_s;

typedef struct lm_value_s {
    lm_value_type_t type;
    union {
        lm_bool bool_value;
        lm_int int_value;
        lm_float float_value;
        struct lm_gc_object_s* gc_value;
    } v;
} lm_value_t;

typedef enum lm_gc_object_type_e {
    LM_GC_OBJECT_TYPE_NONE = 0,
    LM_GC_OBJECT_TYPE_BIGINT,
    LM_GC_OBJECT_TYPE_BIGFLOAT,
    LM_GC_OBJECT_TYPE_STRING,
    LM_GC_OBJECT_TYPE_ARRAY,
    LM_GC_OBJECT_TYPE_OBJECT,
    LM_GC_OBJECT_TYPE_FUNCTION,
} lm_gc_object_type_t;

#define _LM_GC_OBJECT_HEADER  \
    lm_list_node_t list_node; \
    lm_gc_object_type_t type; \
    size_t gc_ref_count;      \
    uint8_t gc_color;         \
    uint8_t gc_flags;         \
    void* owner_runtime;      \
    void* owner_context;

typedef struct lm_gc_object_s {
    _LM_GC_OBJECT_HEADER
} lm_gc_object_t;
