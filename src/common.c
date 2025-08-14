#include "common.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define LM_DEBUG

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
    return malloc(size);
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
    lm_string_t* str = (lm_string_t*) lm__alloc(sizeof(lm_string_t));

    size_t string_len;
    if (len > 0) {
        string_len = len;
    } else {
        string_len = strlen(data);
    }
    assert(string_len > 0);

    size_t string_capacity = string_len + 1;

    str->data = (char*) lm__alloc(string_capacity);

    memcpy(str->data, data, string_len);
    str->data[string_len] = '\0';

    str->length = string_len;
    str->capacity = string_capacity;

    return str;
}


lm_string_t* lm_string_from(char* allocated, size_t len) {
    lm_string_t* str = lm__alloc(sizeof(lm_string_t));

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

    lm__free(str->data);
    lm__free(str);
}

lm_string_t* lm_string_clone(lm_string_t* str) {
    return lm_string_alloc(str->data, str->length);
}

size_t lm_string_len(lm_string_t* str) {
    return str->length;
}

size_t lm_string_cap(lm_string_t* str) {
    return str->capacity;
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
    lm__free(error);
}

const char* lm__error_vtbl_what(lm_error_t* error) {
    return "nLamina compiler error";
}

lm_error_t* lm_error_alloc(const char* msg) {
    lm_error_t* error = lm__alloc(sizeof(lm_error_t));
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
    lm_list_node_t* node = lm__alloc(sizeof(lm_list_node_t));
    lm_list_node_init(node);
    return node;
}

void lm_list_node_init(lm_list_node_t* node) {
    node->next = node;
    node->prev = node;
}

void lm_list_node_free(lm_list_node_t* node) {
    // _LM_ASSERT(node->next == NULL && node->prev == NULL, "node is not properly detached from list");

    lm__free(node);
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

void lm_list_iterate_safe(lm_list_node_t* head, lm__list_iterator_pfn iterator, void* ctx) {
    lm_list_node_t* node = head->next;
    while (node != head) {
        lm_list_node_t* next = node->next;
        iterator(node, ctx);
        node = next;
    };
}
