#pragma once

#include <assert.h>
#include <stddef.h>

#define _LM_ASSERT(_cond, _msg) (assert(((_msg) && (_cond))))

struct lm_error_s;

typedef void (*lm__error_vtbl_free_pfn)(struct lm_error_s* error);
typedef const char* (*lm__error_vtbl_what_pfn)(struct lm_error_s* error);

typedef struct lm__error_vtbl_s {
    lm__error_vtbl_free_pfn free;
    lm__error_vtbl_what_pfn what;
} lm__error_vtbl_t;

#define LM_ERROR_HEADER \
    lm__error_vtbl_t vtbl;

typedef struct lm_error_s {
    LM_ERROR_HEADER
} lm_error_t;

void lm__error_vtbl_free(lm_error_t* error);

const char* lm__error_vtbl_what(lm_error_t* error);

lm_error_t* lm_error_alloc(const char* msg);

void lm_error_free(lm_error_t* error);

typedef struct lm_string_s {
    char* data;
    size_t length;
    size_t capacity;
} lm_string_t;

typedef char lm_bool;

#define LM_TRUE 1
#define LM_FALSE 0

lm_string_t* lm_string_alloc(const char* data, size_t len);

lm_string_t* lm_string_from(char* allocated, size_t len);

void lm_string_free(lm_string_t* str);

lm_string_t* lm_string_clone(lm_string_t* str);

size_t lm_string_len(lm_string_t* str);

size_t lm_string_cap(lm_string_t* str);

char lm_string_get(lm_string_t* str, size_t index);

char lm_string_get_safe(lm_string_t* str, size_t index, lm_bool* is_valid);

void* lm__alloc(size_t size);

void lm__free(void* ptr);

lm_bool lm__alloc_counter_is_zero();
