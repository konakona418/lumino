#include "common.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef LM_DEBUG
typedef struct lm__alloc_counter_s {
    size_t alloc_count;
} lm__alloc_counter_t;

lm__alloc_counter_t g_lm__alloc_counter = {0};

void lm__alloc_counter_inc() {
    g_lm__alloc_counter.alloc_count++;
}

void lm__alloc_counter_dec() {
    _LM_ASSERT(g_lm__alloc_counter.alloc_count > 0, "Alloc counter underflow");
    g_lm__alloc_counter.alloc_count--;
}

lm_bool lm__alloc_counter_is_zero() {
    return g_lm__alloc_counter.alloc_count == 0;
}

void* lm__alloc(size_t size) {
    lm__alloc_counter_inc();
#ifndef LM_DEBUG_MEM_DETAILS
    return malloc(size);
#else
    void* ptr = malloc(size);
    printf("%p\n", ptr);
    return ptr;
#endif
}

void* lm__calloc(size_t type_size, size_t size) {
    lm__alloc_counter_inc();
#ifndef LM_DEBUG_MEM_DETAILS
    return calloc(type_size, size);
#else
    void* ptr = calloc(type_size, size);
    printf("%p", ptr);
    return ptr;
#endif
}

void lm__free(void* ptr) {
    if (ptr == NULL) {
        return;
    }

    lm__alloc_counter_dec();
    free(ptr);
}

#else

void lm__alloc_counter_inc() {}

void lm__alloc_counter_dec() {}

lm_bool lm__alloc_counter_is_zero() { return LM_TRUE; }

void* lm__alloc(size_t size) { return malloc(size); }

void lm__free(void* ptr) { free(ptr); }

#endif

lm_string_t* lm_string_alloc(const char* data, size_t len) {
    lm_string_t* str = _LM_ALLOC(lm_string_t);
    lm__string_rc_internal_t* internal = _LM_ALLOC(lm__string_rc_internal_t);
    internal->ref = 1;
    str->internal = internal;

    size_t string_len;
    if (len > 0) {
        string_len = len;
    } else {
        string_len = strlen(data);
    }
    assert(string_len > 0);

    size_t string_capacity = string_len + 1;

    str->data = _LM_ALLOC_ARRAY(char, string_capacity);

    memcpy(str->data, data, string_len);
    str->data[string_len] = '\0';

    str->length = string_len;
    str->capacity = string_capacity;

    return str;
}


lm_string_t* lm_string_from(char* allocated, size_t len) {
    lm_string_t* str = _LM_ALLOC(lm_string_t);
    lm__string_rc_internal_t* internal = _LM_ALLOC(lm__string_rc_internal_t);
    internal->ref = 1;
    str->internal = internal;

    size_t string_len;
    if (len > 0) {
        string_len = len;
    } else {
        string_len = strlen(allocated);
    }

    size_t string_capacity = string_len + 1;

    str->data = allocated;
    str->length = string_len;
    str->capacity = string_capacity;

    return str;
}

void lm_string_free(lm_string_t* str) {
    if (str == NULL) {
        return;
    }

    str->internal->ref--;
    if (str->internal->ref == 0) {
        _LM_FREE(str->internal);
        _LM_FREE(str->data);
    }

    _LM_FREE(str);
}

lm_string_t* lm_string_copy(lm_string_t* str) {
    return lm_string_alloc(str->data, str->length);
}

lm_string_t* lm_string_clone(lm_string_t* str) {
    lm_string_t* new_str = _LM_ALLOC(lm_string_t);
    new_str->internal = str->internal;

    new_str->data = str->data;
    new_str->length = str->length;
    new_str->capacity = str->capacity;

    str->internal->ref++;

    return new_str;
}

size_t lm_string_ref_count(lm_string_t* str) {
    return str->internal->ref;
}

size_t lm_string_len(lm_string_t* str) {
    return str->length;
}

size_t lm_string_cap(lm_string_t* str) {
    return str->capacity;
}

lm_bool lm_string_equal(const lm_string_t* lhs, const lm_string_t* rhs) {
    return strcmp(lhs->data, rhs->data) == 0;
}

char lm_string_get(lm_string_t* str, size_t index) {
    _LM_ASSERT(index < str->capacity, "string index out of bounds");
    return str->data[index];
}

char lm_string_get_safe(lm_string_t* str, size_t index, lm_bool* is_valid) {
    if (index < str->capacity) {
        if (is_valid) {
            *is_valid = LM_TRUE;
        }

        return str->data[index];
    }

    if (is_valid) {
        *is_valid = LM_FALSE;
    }

    return '\0';
}

void lm__error_vtbl_free(lm_error_t* error) {
    _LM_FREE(error);
}

const char* lm__error_vtbl_what(lm_error_t* error) {
    return "nLamina compiler error";
}

lm_error_t* lm_error_alloc(const char* msg) {
    lm_error_t* error = _LM_ALLOC(lm_error_t);
    lm__error_vtbl_t vtbl = {lm__error_vtbl_free, lm__error_vtbl_what};
    error->vtbl = vtbl;

    return error;
}

const char* lm_error_what(lm_error_t* error) {
    _LM_ASSERT(error != NULL, "error is null");
    _LM_ASSERT(error->vtbl.what != NULL, "function ptr 'what' in vtable of error is null");

    return error->vtbl.what(error);
}

void lm_error_free(lm_error_t* error) {
    if (error == NULL) {
        return;
    }

    lm__error_vtbl_free_pfn free_fn = error->vtbl.free;

    _LM_ASSERT(free_fn != NULL, "function ptr 'free' in vtable of error is null");

    free_fn(error);
}


lm_list_node_t* lm_list_node_alloc() {
    lm_list_node_t* node = _LM_ALLOC(lm_list_node_t);
    lm_list_node_init(node);
    return node;
}

void lm_list_node_init(lm_list_node_t* node) {
    node->next = node;
    node->prev = node;
}

void lm_list_node_free(lm_list_node_t* node) {
    // _LM_ASSERT(node->next == NULL && node->prev == NULL, "node is not properly detached from list");

    _LM_FREE(node);
}

void lm_list_add_head(lm_list_node_t* list, lm_list_node_t* node) {
    _LM_ASSERT(list != NULL && node != NULL, "list or node is null");

    lm_list_node_t* orig_next = list->next;

    list->next = node;
    node->prev = list;

    node->next = orig_next;
    orig_next->prev = node;
}

void lm_list_add_tail(lm_list_node_t* list, lm_list_node_t* node) {
    _LM_ASSERT(list != NULL && node != NULL, "list or node is null");

    lm_list_node_t* orig_prev = list->prev;

    list->prev = node;
    node->prev = orig_prev;

    node->next = list;
    orig_prev->next = node;
}

lm_list_node_t* lm_list_head(lm_list_node_t* list) {
    return list->next;
}

lm_list_node_t* lm_list_tail(lm_list_node_t* list) {
    return list->prev;
}

lm_list_node_t* lm_list_replace(lm_list_node_t* list, lm_list_node_t* node) {
    _LM_ASSERT(list != NULL && node != NULL, "list or node is null");

    lm_list_node_t* replaced = list;

    list->prev->next = node;
    node->prev = list->prev;

    list->next->prev = node;
    node->next = list->next;

    replaced->next = NULL;
    replaced->prev = NULL;

    return replaced;
}

void lm_list_remove(lm_list_node_t* node) {
    _LM_ASSERT(node != NULL, "node is null");

    node->prev->next = node->next;
    node->next->prev = node->prev;

    node->next = NULL;
    node->prev = NULL;
}

void lm_list_iterate(lm_list_node_t* head, lm__list_iterator_pfn iterator, void* ctx) {
    lm_list_node_t* node = head->next;
    while (node != head) {
        iterator(node, ctx);
        node = node->next;
    };
}

void lm_list_iterate_predicated(lm_list_node_t* head, lm__list_iterator_predicated_pfn iterator, void* ctx) {
    lm_list_node_t* node = head->next;
    while (node != head) {
        lm_bool result = iterator(node, ctx);
        if (!result) {
            break;
        }
        node = node->next;
    };
}

void lm_list_iterate_safe(lm_list_node_t* head, lm__list_iterator_pfn iterator, void* ctx) {
    lm_list_node_t* node = head->next;
    while (node != head) {
        lm_list_node_t* next = node->next;
        iterator(node, ctx);
        node = next;
    };
}

void lm_list_reverse_iterate(lm_list_node_t* head, lm__list_iterator_pfn iterator, void* ctx) {
    lm_list_node_t* node = head->prev;
    while (node != head) {
        iterator(node, ctx);
        node = node->prev;
    }
}

void lm_list_reverse_iterate_predicated(lm_list_node_t* head, lm__list_iterator_predicated_pfn iterator, void* ctx) {
    lm_list_node_t* node = head->prev;
    while (node != head) {
        lm_bool result = iterator(node, ctx);
        if (!result) {
            break;
        }
        node = node->prev;
    }
}

lm_ref_counted_t* lm_ref_counted_alloc(void* data) {
    _LM_ASSERT_NOT_NULL(data, "data is null");

    lm_ref_counted_t* rc = _LM_ALLOC(lm_ref_counted_t);
    lm__ref_counted_internal_t* rc_internal = _LM_ALLOC(lm__ref_counted_internal_t);

    rc_internal->ref = 1;
    rc_internal->weak_ref = 0;

    rc->internal = rc_internal;
    rc->data_ptr = data;

    return rc;
}

void lm_ref_counted_free(lm_ref_counted_t* rc) {
    rc->internal->ref--;

    if (rc->internal->ref == 0) {
        _LM_FREE(rc->internal);
        _LM_FREE(rc->data_ptr);
    }

    _LM_FREE(rc);
}

lm_ref_counted_t* lm_ref_counted_clone(lm_ref_counted_t* rc) {
    lm_ref_counted_t* new_rc = _LM_ALLOC(lm_ref_counted_t);

    new_rc->internal = rc->internal;
    new_rc->data_ptr = rc->data_ptr;

    rc->internal->ref++;

    return new_rc;
}
