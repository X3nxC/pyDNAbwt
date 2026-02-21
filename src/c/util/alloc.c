#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

typedef struct dnabwt_alloc_header {
    size_t size;
} dnabwt_alloc_header_t;

static size_t g_mem_current = 0u;
static size_t g_mem_peak = 0u;

static void dnabwt_mem_add(size_t size) {
    g_mem_current += size;
    if (g_mem_current > g_mem_peak) {
        g_mem_peak = g_mem_current;
    }
}

static void dnabwt_mem_sub(size_t size) {
    if (g_mem_current >= size) {
        g_mem_current -= size;
    } else {
        g_mem_current = 0u;
    }
}

static int dnabwt_mul_overflow(size_t a, size_t b) {
    if (a == 0u || b == 0u) {
        return 0;
    }
    return a > (SIZE_MAX / b);
}

static void *dnabwt_alloc_raw(size_t size, int zero_init) {
    dnabwt_alloc_header_t *h;
    size_t total;

    if (size > (SIZE_MAX - sizeof(*h))) {
        return NULL;
    }
    total = sizeof(*h) + size;

    h = zero_init ? (dnabwt_alloc_header_t *)calloc(1u, total) : (dnabwt_alloc_header_t *)malloc(total);
    if (h == NULL) {
        return NULL;
    }

    h->size = size;
    dnabwt_mem_add(size);
    return (void *)(h + 1);
}

void *dnabwt_malloc(size_t size) {
    if (size == 0u) {
        size = 1u;
    }
    return dnabwt_alloc_raw(size, 0);
}

void *dnabwt_calloc(size_t count, size_t size) {
    size_t total;

    if (dnabwt_mul_overflow(count, size)) {
        return NULL;
    }
    total = count * size;
    if (total == 0u) {
        total = 1u;
    }
    return dnabwt_alloc_raw(total, 1);
}

void *dnabwt_realloc(void *ptr, size_t size) {
    dnabwt_alloc_header_t *old_h;
    dnabwt_alloc_header_t *new_h;
    size_t total;
    size_t old_size;

    if (ptr == NULL) {
        return dnabwt_malloc(size);
    }

    if (size == 0u) {
        dnabwt_free(ptr);
        return NULL;
    }

    if (size > (SIZE_MAX - sizeof(*old_h))) {
        return NULL;
    }

    old_h = ((dnabwt_alloc_header_t *)ptr) - 1;
    old_size = old_h->size;
    total = sizeof(*old_h) + size;

    new_h = (dnabwt_alloc_header_t *)realloc(old_h, total);
    if (new_h == NULL) {
        return NULL;
    }

    new_h->size = size;
    if (size > old_size) {
        dnabwt_mem_add(size - old_size);
    } else {
        dnabwt_mem_sub(old_size - size);
    }

    return (void *)(new_h + 1);
}

void dnabwt_free(void *ptr) {
    dnabwt_alloc_header_t *h;

    if (ptr == NULL) {
        return;
    }

    h = ((dnabwt_alloc_header_t *)ptr) - 1;
    dnabwt_mem_sub(h->size);
    free(h);
}

char *dnabwt_strdup(const char *s) {
    size_t len;
    char *out;

    if (s == NULL) {
        return NULL;
    }
    len = strlen(s);
    out = (char *)dnabwt_malloc(len + 1u);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, s, len + 1u);
    return out;
}

void dnabwt_mem_reset_peak(void) {
    g_mem_peak = g_mem_current;
}

size_t dnabwt_mem_current(void) {
    return g_mem_current;
}

size_t dnabwt_mem_peak(void) {
    return g_mem_peak;
}
