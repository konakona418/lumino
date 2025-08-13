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

const char* lm_error_what(lm_error_t* error);

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

typedef struct lm_list_node_s {
    struct lm_list_node_s* next;
    struct lm_list_node_s* prev;
} lm_list_node_t;

lm_list_node_t* lm_list_node_alloc();

void lm_list_node_init(lm_list_node_t* node);

void lm_list_node_free(lm_list_node_t* node);

void lm_list_add_head(lm_list_node_t* list, lm_list_node_t* node);

void lm_list_add_tail(lm_list_node_t* list, lm_list_node_t* node);

lm_list_node_t* lm_list_replace(lm_list_node_t* list, lm_list_node_t* node);

void lm_list_remove(lm_list_node_t* node);

typedef void (*lm__list_iterator_pfn)(lm_list_node_t* node, void* ctx);

void lm_list_iterate(lm_list_node_t* list, lm__list_iterator_pfn iterator, void* ctx);

void lm_list_iterate_safe(lm_list_node_t* list, lm__list_iterator_pfn iterator, void* ctx);

#define container_of(ptr, type, member)                    \
    ({                                                     \
        const typeof(((type*) 0)->member)* __mptr = (ptr); \
        (type*) ((char*) __mptr - offsetof(type, member)); \
    })

#define lm_list_entry(node, type, member) \
    container_of(node, type, member)
