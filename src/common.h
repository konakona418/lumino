#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define LM_DEBUG
#define LM_DEBUG_MEM_DETAILS
#define LM_DEBUG_MEM_DETAILS_JSON

#ifdef __GNUC__
#define _LM_IS_GNUC
#endif

#ifdef __clang__
#define _LM_IS_CLANG
#endif

#if defined(_LM_IS_GNUC) || defined(_LM_IS_CLANG)
#define _LM_IS_GNUC_OR_CLANG
#endif

#ifdef _LM_IS_GNUC_OR_CLANG
#define _LM_HAS_STATEMENT_EXPR_EXTENSION
#endif

#ifdef LM_DEBUG

#define _LM_ASSERT(_cond, _msg) (assert(((_msg) && (_cond))))
#define _LM_ASSERT_NOT_NULL(_ptr, _msg) _LM_ASSERT((_ptr), _msg)
#define _LM_ASSERT_NULL(_ptr, _msg) _LM_ASSERT((_ptr == NULL), _msg)

#else

#define _LM_ASSERT(_cond, _msg)
#define _LM_ASSERT_NOT_NULL(_ptr, _msg)
#define _LM_ASSERT_NULL(_ptr, _msg)

#endif

struct lm_error_s;

typedef void (*lm__error_vtbl_free_pfn)(struct lm_error_s* error);
typedef const char* (*lm__error_vtbl_what_pfn)(struct lm_error_s* error);

#define _LM_CAST(_type, _ptr) ((_type*) (_ptr))

#ifdef __clang__
#define _LM_NULLABLE _Nullable
#define _LM_NONNULL _Nonnull
#else
#define _LM_NULLABLE
#define _LM_NONNULL
#endif

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

typedef struct lm__string_rc_internal_s {
    size_t ref;
} lm__string_rc_internal_t;

typedef struct lm_string_s {
    lm__string_rc_internal_t* internal;

    char* data;
    size_t length;
    size_t capacity;
} lm_string_t;

typedef char lm_bool;
typedef int32_t lm_int;
typedef float lm_float;

#define LM_TRUE 1
#define LM_FALSE 0

lm_string_t* lm_string_alloc(const char* data, size_t len);

lm_string_t* lm_string_from(char* allocated, size_t len);

void lm_string_free(lm_string_t* str);

lm_string_t* lm_string_copy(const lm_string_t* str);

lm_string_t* lm_string_substr(const lm_string_t* str, size_t begin_offset, size_t len);

lm_string_t* lm_string_clone(const lm_string_t* str);

size_t lm_string_ref_count(const lm_string_t* str);

size_t lm_string_len(const lm_string_t* str);

size_t lm_string_cap(const lm_string_t* str);

lm_bool lm_string_equal(const lm_string_t* lhs, const lm_string_t* rhs);

char lm_string_get(const lm_string_t* str, size_t index);

char lm_string_get_safe(const lm_string_t* str, size_t index, lm_bool* is_valid);

uint32_t lm_string_hash(const lm_string_t* str);

lm_bool lm_string_stoi(const lm_string_t* str, lm_int* val);

lm_bool lm_string_stof(const lm_string_t* str, lm_float* val);

typedef uint32_t lm_atom_t;
#define LM_ATOM_NIL 0

#define lm_move(_p_ptr_dest, _p_ptr_src) ({ *_p_ptr_dest = *_p_ptr_src; *_p_ptr_src = NULL; })

void* lm__alloc(size_t size);

void* lm__calloc(size_t type_size, size_t size);

void lm__free(void* ptr);


#if defined(LM_DEBUG) && defined(LM_DEBUG_MEM_DETAILS)

#ifdef _LM_HAS_STATEMENT_EXPR_EXTENSION
#ifdef LM_DEBUG_MEM_DETAILS_JSON

#define _LM_ALLOC(type)                                                   \
    ({                                                                    \
        void* ptr = lm__alloc(sizeof(type));                              \
        fprintf(stdout, "{\"action\":\"alloc\", \"type\":\"" #type "\", " \
                        "\"size\":%zu, \"addr\":\"%p\"}\n",               \
                sizeof(type), ptr);                                       \
        (type*) ptr;                                                      \
    })

#define _LM_CALLOC(type, size)                                             \
    ({                                                                     \
        void* ptr = lm__calloc(sizeof(type), size);                        \
        fprintf(stdout, "{\"action\":\"calloc\", \"type\":\"" #type "\", " \
                        "\"size\":%zu, \"count\":%zu, \"addr\":\"%p\"}\n", \
                sizeof(type), (size_t) (size), ptr);                       \
        (type*) ptr;                                                       \
    })

#define _LM_FREE(ptr)                                                     \
    do {                                                                  \
        fprintf(stdout, "{\"action\":\"free\", \"addr\":\"%p\"}\n", ptr); \
        lm__free(ptr);                                                    \
    } while (0)

#define _LM_ALLOC_ARRAY(type, count)                                               \
    ({                                                                             \
        void* ptr = lm__alloc(sizeof(type) * (count));                             \
        fprintf(stdout, "{\"action\":\"alloc_array\", \"type\":\"" #type "\", "    \
                        "\"element_size\":%zu, \"count\":%zu, \"addr\":\"%p\"}\n", \
                sizeof(type), (size_t) (count), ptr);                              \
        (type*) ptr;                                                               \
    })

#else//#ifdef LM_DEBUG_MEM_DETAILS_JSON

#define _LM_ALLOC(type)                                                   \
    ({                                                                    \
        type* ptr = (type*) lm__alloc(sizeof(type));                      \
        printf("alloc: " #type " (%zu) bytes @ %p\n", sizeof(type), ptr); \
        ptr;                                                              \
    })

#define _LM_CALLOC(type, size)                                                        \
    ({                                                                                \
        type* ptr = (type*) lm__calloc(sizeof(type), size);                           \
        printf("alloc: " #type " (%zu) bytes * %zu @ %p\n", sizeof(type), size, ptr); \
        ptr;                                                                          \
    })

#define _LM_FREE(ptr)               \
    ({                              \
        printf("free: %p \n", ptr); \
        lm__free(ptr);              \
    })

#define _LM_ALLOC_ARRAY(type, count)                                                   \
    ({                                                                                 \
        type* ptr = (type*) lm__alloc(sizeof(type) * count);                           \
        printf("alloc: " #type " (%zu) bytes * %zu @ %p\n", sizeof(type), count, ptr); \
        ptr;                                                                           \
    })

#endif//#ifdef LM_DEBUG_MEM_DETAILS_JSON

#else//#ifdef _LM_HAS_STATEMENT_EXPR_EXTENSION

#warning statement extension feature not supported, defaulting to non-verbose log output.

#define _LM_ALLOC(type) (printf("alloc: " #type " (%zu) bytes\n", sizeof(type)), (type*) lm__alloc(sizeof(type)))
#define _LM_CALLOC(type, size) (printf("alloc: " #type " (%zu) bytes\n", sizeof(type)), (type*) lm__calloc(sizeof(type), size))
#define _LM_FREE(ptr) (printf("free: %p\n", ptr), lm__free(ptr))
#define _LM_ALLOC_ARRAY(type, count)                                      \
    (printf("alloc: " #type " (%zu) bytes * %zu\n", sizeof(type), count), \
     (type*) lm__alloc(sizeof(type) * count))

#endif//#ifdef _LM_HAS_STATEMENT_EXPR_EXTENSION

#else//#if defined(LM_DEBUG) && defined(LM_DEBUG_MEM_DETAILS)


#define _LM_ALLOC(type) (type*) lm__alloc(sizeof(type))
#define _LM_CALLOC(type, size) (type*) lm__calloc(sizeof(type), size)
#define _LM_FREE(ptr) lm__free(ptr)
#define _LM_ALLOC_ARRAY(type, count) (type*) lm__alloc(sizeof(type) * count)

#endif//#if defined(LM_DEBUG) && defined(LM_DEBUG_MEM_DETAILS)

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

lm_bool lm_list_empty(lm_list_node_t* list);

lm_list_node_t* lm_list_head(lm_list_node_t* list);

lm_list_node_t* lm_list_tail(lm_list_node_t* list);

lm_list_node_t* lm_list_replace(lm_list_node_t* list, lm_list_node_t* node);

void lm_list_remove(lm_list_node_t* node);

typedef void (*lm__list_iterator_pfn)(lm_list_node_t* node, void* ctx);

typedef lm_bool (*lm__list_iterator_predicated_pfn)(lm_list_node_t* node, void* ctx);

#define LM_PREDICATE_CONTINUE 1
#define LM_PREDICATE_STOP 0

void lm_list_iterate(lm_list_node_t* head, lm__list_iterator_pfn iterator, void* ctx);

void lm_list_iterate_predicated(lm_list_node_t* head, lm__list_iterator_predicated_pfn iterator, void* ctx);

void lm_list_iterate_safe(lm_list_node_t* head, lm__list_iterator_pfn iterator, void* ctx);

void lm_list_reverse_iterate(lm_list_node_t* head, lm__list_iterator_pfn iterator, void* ctx);

void lm_list_reverse_iterate_predicated(lm_list_node_t* head, lm__list_iterator_predicated_pfn iterator, void* ctx);

#define container_of(ptr, type, member)                    \
    ({                                                     \
        const typeof(((type*) 0)->member)* __mptr = (ptr); \
        (type*) ((char*) __mptr - offsetof(type, member)); \
    })

#define lm_list_entry(node, type, member) \
    container_of(node, type, member)

typedef struct lm__ref_counted_internal_s {
    size_t ref;
    size_t weak_ref;
} lm__ref_counted_internal_t;

typedef struct lm_ref_counted_s {
    lm__ref_counted_internal_t* internal;
    void* data_ptr;
} lm_ref_counted_t;

lm_ref_counted_t* lm_ref_counted_alloc(void* data);

void lm_ref_counted_free(lm_ref_counted_t* rc);

lm_ref_counted_t* lm_ref_counted_clone(lm_ref_counted_t* rc);

#define lm_make_ref_counted(_type) (lm_ref_counted_alloc(_LM_ALLOC(_type)))

#define lm_ref_counted_unwrap(_type, _rc_ptr) ((_type*) (_rc)->data)