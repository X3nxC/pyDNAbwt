#ifndef DNABWT_BWT_BWT_INTERNAL_H
#define DNABWT_BWT_BWT_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "../io/io.h"
#include "../util/status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dnabwt_fm_index {
    size_t n;
    size_t *occ;
    size_t c_table[256];
    size_t occ_stride;
} dnabwt_fm_index_t;

typedef struct dnabwt_search_nav_entry {
    size_t file_off;
    size_t inrun_skip;
    uint8_t run_valid;
} dnabwt_search_nav_entry_t;

typedef struct dnabwt_search_cache {
    long long block_index;
    size_t length;
    uint64_t stamp;
    uint8_t *data;
} dnabwt_search_cache_t;

typedef struct dnabwt_search_index {
    dnabwt_reader_t *reader;
    size_t n;
    size_t sentinel_index;
    size_t block_size;
    size_t nb;
    size_t cache_count;
    uint64_t clock;
    size_t c_table[256];
    size_t totals[256];
    size_t *occ5; /* nb x 6, order: $, A, G, C, T, N */
    dnabwt_search_nav_entry_t *nav;
    dnabwt_search_cache_t *caches;
} dnabwt_search_index_t;

dnabwt_status_t dnabwt_validate_dna_text(const uint8_t *text, size_t len, size_t *dna_len_out);
dnabwt_status_t dnabwt_build_suffix_array(const uint8_t *text, size_t n, size_t **sa_out);

/* Legacy in-memory helpers. */
dnabwt_status_t dnabwt_build_fm_index(const uint8_t *bwt, size_t n, dnabwt_fm_index_t *index);
size_t dnabwt_rank(const dnabwt_fm_index_t *index, uint8_t symbol, size_t pos);
void dnabwt_free_fm_index(dnabwt_fm_index_t *index);

#ifdef __cplusplus
}
#endif

#endif
