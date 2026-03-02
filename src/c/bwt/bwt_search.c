#include <string.h>

#include "bwt.h"
#include "bwt_internal.h"
#include "../code/dna_rle.h"
#include "../code/sentinel.h"
#include "../util/util.h"

#define DNABWT_OCC_SLOT_COUNT 5u
#define DNABWT_SLOT_SENTINEL 0u
#define DNABWT_SLOT_A 1u
#define DNABWT_SLOT_G 2u
#define DNABWT_SLOT_C 3u
#define DNABWT_SLOT_T 4u

static uint8_t dnabwt_rle_symbol_from_code(uint8_t code) {
    switch (code) {
        case 0u:
            return 'A';
        case 1u:
            return 'G';
        case 2u:
            return 'C';
        case 3u:
            return 'T';
        default:
            return 0u;
    }
}

static size_t dnabwt_occ_slot_for_symbol(uint8_t symbol) {
    switch (symbol) {
        case 0u:
            return DNABWT_SLOT_SENTINEL;
        case 'A':
            return DNABWT_SLOT_A;
        case 'G':
            return DNABWT_SLOT_G;
        case 'C':
            return DNABWT_SLOT_C;
        case 'T':
            return DNABWT_SLOT_T;
        default:
            return (size_t)-1;
    }
}

static dnabwt_status_t dnabwt_reader_read_u8(dnabwt_reader_t *reader, size_t off, uint8_t *out) {
    return dnabwt_reader_read_at(reader, off, out, 1u);
}

static dnabwt_status_t dnabwt_read_run_at(dnabwt_reader_t *reader,
                                          size_t run_off,
                                          uint8_t *symbol,
                                          uint8_t *run_len) {
    uint8_t b = 0;
    dnabwt_status_t status;

    status = dnabwt_reader_read_u8(reader, run_off, &b);
    if (status != DNABWT_STATUS_OK) {
        return status;
    }

    *symbol = dnabwt_rle_symbol_from_code((uint8_t)(b >> 6u));
    if (*symbol == 0u) {
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }
    *run_len = (uint8_t)((b & 0x3Fu) + 1u);
    return DNABWT_STATUS_OK;
}

static dnabwt_search_cache_t *dnabwt_search_cache_find(dnabwt_search_index_t *index, size_t blk) {
    size_t i;

    for (i = 0; i < index->cache_count; ++i) {
        if (index->caches[i].block_index == (long long)blk) {
            index->caches[i].stamp = ++index->clock;
            return &index->caches[i];
        }
    }
    return NULL;
}

static dnabwt_search_cache_t *dnabwt_search_cache_pick_victim(dnabwt_search_index_t *index) {
    size_t i;
    size_t victim = 0;
    uint64_t oldest = UINT64_MAX;

    for (i = 0; i < index->cache_count; ++i) {
        if (index->caches[i].block_index < 0) {
            return &index->caches[i];
        }
        if (index->caches[i].stamp < oldest) {
            oldest = index->caches[i].stamp;
            victim = i;
        }
    }
    return &index->caches[victim];
}

static dnabwt_status_t dnabwt_search_decode_block(dnabwt_search_index_t *index,
                                                  size_t blk,
                                                  dnabwt_search_cache_t *cache) {
    const dnabwt_search_nav_entry_t *nav;
    size_t start;
    size_t target_len;
    size_t out = 0;
    size_t run_off;
    uint8_t run_skip;
    uint8_t run_valid;
    uint8_t run_symbol = 0u;
    uint8_t run_len = 0u;
    dnabwt_status_t status = DNABWT_STATUS_OK;

    nav = &index->nav[blk];
    start = blk * index->block_size;
    target_len = index->n - start;
    if (target_len > index->block_size) {
        target_len = index->block_size;
    }

    run_off = nav->file_off;
    run_skip = nav->inrun_skip;
    run_valid = nav->run_valid;

    if (run_valid) {
        status = dnabwt_read_run_at(index->reader, run_off, &run_symbol, &run_len);
        if (status != DNABWT_STATUS_OK || run_skip >= run_len) {
            return DNABWT_STATUS_INTERNAL_ERROR;
        }
    }

    while (out < target_len) {
        size_t full_pos = start + out;

        if (full_pos == index->sentinel_index) {
            cache->data[out++] = 0u;
            continue;
        }

        if (!run_valid) {
            if (run_off >= index->reader->file_size) {
                return DNABWT_STATUS_INTERNAL_ERROR;
            }
            status = dnabwt_read_run_at(index->reader, run_off, &run_symbol, &run_len);
            if (status != DNABWT_STATUS_OK) {
                return status;
            }
            run_skip = 0u;
            run_valid = 1u;
        }

        cache->data[out++] = run_symbol;
        run_skip++;
        if (run_skip >= run_len) {
            run_off++;
            run_skip = 0u;
            run_valid = 0u;
        }
    }

    cache->block_index = (long long)blk;
    cache->length = out;
    cache->stamp = ++index->clock;
    return DNABWT_STATUS_OK;
}

static dnabwt_status_t dnabwt_search_get_block(dnabwt_search_index_t *index,
                                               size_t blk,
                                               const uint8_t **data_out,
                                               size_t *len_out) {
    dnabwt_search_cache_t *cache;

    cache = dnabwt_search_cache_find(index, blk);
    if (cache == NULL) {
        dnabwt_status_t status;
        cache = dnabwt_search_cache_pick_victim(index);
        status = dnabwt_search_decode_block(index, blk, cache);
        if (status != DNABWT_STATUS_OK) {
            return status;
        }
    }

    *data_out = cache->data;
    *len_out = cache->length;
    return DNABWT_STATUS_OK;
}

static size_t dnabwt_search_occ_at(dnabwt_search_index_t *index, uint8_t symbol, size_t pos, dnabwt_status_t *status) {
    size_t slot;
    size_t blk;
    size_t within;
    size_t base;

    *status = DNABWT_STATUS_OK;
    slot = dnabwt_occ_slot_for_symbol(symbol);
    if (slot == (size_t)-1) {
        *status = DNABWT_STATUS_INVALID_ARGUMENT;
        return 0;
    }

    if (pos == 0) {
        return 0;
    }
    if (pos >= index->n) {
        return index->totals[symbol];
    }

    blk = pos / index->block_size;
    within = pos % index->block_size;
    base = index->occ5[blk * DNABWT_OCC_SLOT_COUNT + slot];
    if (within == 0) {
        return base;
    }

    {
        const uint8_t *block = NULL;
        size_t block_len = 0;
        size_t i;
        dnabwt_status_t st = dnabwt_search_get_block(index, blk, &block, &block_len);
        if (st != DNABWT_STATUS_OK) {
            *status = st;
            return 0;
        }
        if (within > block_len) {
            within = block_len;
        }
        for (i = 0; i < within; ++i) {
            if (block[i] == symbol) {
                base++;
            }
        }
    }

    return base;
}

static dnabwt_status_t dnabwt_search_build_index(const char *encoded_path,
                                                 size_t block_size,
                                                 size_t cache_count,
                                                 dnabwt_search_index_t **index_out) {
    dnabwt_search_index_t *idx = NULL;
    dnabwt_reader_t *reader = NULL;
    uint8_t header[DNABWT_SENTINEL_HEADER_SIZE];
    uint64_t sentinel_idx64 = 0;
    size_t run_off;
    size_t decoded_len = 0;
    size_t n;
    size_t sentinel_idx;
    dnabwt_status_t status;
    size_t i;

    dnabwt_mem_reset_peak();
    dnabwt_log(DNABWT_LOG_INFO,
               "bwt search index build start: path=%s block_size=%zu cache_count=%zu",
               encoded_path != NULL ? encoded_path : "(null)",
               block_size,
               cache_count);

    if (encoded_path == NULL || index_out == NULL) {
        dnabwt_log(DNABWT_LOG_WARN, "bwt search index build invalid argument");
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }
    *index_out = NULL;

    if (block_size == 0) {
        block_size = 512u;
    }
    if (cache_count == 0) {
        cache_count = 2u;
    }

    status = dnabwt_reader_open(&reader, encoded_path, 1u << 16, 4u);
    if (status != DNABWT_STATUS_OK) {
        return status;
    }
    if (reader->file_size < DNABWT_SENTINEL_HEADER_SIZE) {
        dnabwt_reader_close(reader);
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }

    status = dnabwt_reader_read_at(reader, 0u, header, DNABWT_SENTINEL_HEADER_SIZE);
    if (status != DNABWT_STATUS_OK) {
        dnabwt_reader_close(reader);
        return status;
    }
    status = dnabwt_sentinel_unpack(header, &sentinel_idx64);
    if (status != DNABWT_STATUS_OK) {
        dnabwt_reader_close(reader);
        return status;
    }

    for (run_off = DNABWT_SENTINEL_HEADER_SIZE; run_off < reader->file_size; ++run_off) {
        uint8_t b = 0;
        status = dnabwt_reader_read_u8(reader, run_off, &b);
        if (status != DNABWT_STATUS_OK) {
            dnabwt_reader_close(reader);
            return status;
        }
        decoded_len += (size_t)((b & 0x3Fu) + 1u);
    }

    n = decoded_len + 1u;
    sentinel_idx = (size_t)sentinel_idx64;
    if (sentinel_idx >= n) {
        dnabwt_reader_close(reader);
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }

    idx = (dnabwt_search_index_t *)dnabwt_calloc(1u, sizeof(*idx));
    if (idx == NULL) {
        dnabwt_reader_close(reader);
        return DNABWT_STATUS_NO_MEMORY;
    }

    idx->reader = reader;
    idx->n = n;
    idx->sentinel_index = sentinel_idx;
    idx->block_size = block_size;
    idx->nb = (n + block_size - 1u) / block_size;
    idx->cache_count = cache_count;

    idx->occ5 = (size_t *)dnabwt_calloc(idx->nb * DNABWT_OCC_SLOT_COUNT, sizeof(size_t));
    idx->nav = (dnabwt_search_nav_entry_t *)dnabwt_calloc(idx->nb, sizeof(*idx->nav));
    idx->caches = (dnabwt_search_cache_t *)dnabwt_calloc(cache_count, sizeof(*idx->caches));
    if (idx->occ5 == NULL || idx->nav == NULL || idx->caches == NULL) {
        dnabwt_search_index_free(idx);
        return DNABWT_STATUS_NO_MEMORY;
    }

    for (i = 0; i < cache_count; ++i) {
        idx->caches[i].block_index = -1;
        idx->caches[i].data = (uint8_t *)dnabwt_malloc(block_size);
        if (idx->caches[i].data == NULL) {
            dnabwt_search_index_free(idx);
            return DNABWT_STATUS_NO_MEMORY;
        }
    }

    {
        size_t accum5[DNABWT_OCC_SLOT_COUNT] = {0u, 0u, 0u, 0u, 0u};
        size_t full_pos;
        size_t blk = 0;
        size_t cur_run_off = DNABWT_SENTINEL_HEADER_SIZE;
        uint8_t cur_run_skip = 0u;
        uint8_t cur_run_symbol = 0u;
        uint8_t cur_run_len = 0u;
        uint8_t cur_run_valid = 0u;

        for (full_pos = 0; full_pos < n; ++full_pos) {
            if ((full_pos % block_size) == 0) {
                idx->occ5[blk * DNABWT_OCC_SLOT_COUNT + DNABWT_SLOT_SENTINEL] = accum5[DNABWT_SLOT_SENTINEL];
                idx->occ5[blk * DNABWT_OCC_SLOT_COUNT + DNABWT_SLOT_A] = accum5[DNABWT_SLOT_A];
                idx->occ5[blk * DNABWT_OCC_SLOT_COUNT + DNABWT_SLOT_G] = accum5[DNABWT_SLOT_G];
                idx->occ5[blk * DNABWT_OCC_SLOT_COUNT + DNABWT_SLOT_C] = accum5[DNABWT_SLOT_C];
                idx->occ5[blk * DNABWT_OCC_SLOT_COUNT + DNABWT_SLOT_T] = accum5[DNABWT_SLOT_T];
                idx->nav[blk].file_off = cur_run_off;
                idx->nav[blk].inrun_skip = cur_run_skip;
                idx->nav[blk].run_valid = cur_run_valid;
                blk++;
            }

            if (full_pos == sentinel_idx) {
                accum5[DNABWT_SLOT_SENTINEL] += 1u;
            } else {
                size_t slot;

                if (!cur_run_valid) {
                    if (cur_run_off >= reader->file_size) {
                        dnabwt_search_index_free(idx);
                        return DNABWT_STATUS_INTERNAL_ERROR;
                    }
                    status = dnabwt_read_run_at(reader, cur_run_off, &cur_run_symbol, &cur_run_len);
                    if (status != DNABWT_STATUS_OK) {
                        dnabwt_search_index_free(idx);
                        return status;
                    }
                    cur_run_skip = 0u;
                    cur_run_valid = 1u;
                }

                slot = dnabwt_occ_slot_for_symbol(cur_run_symbol);
                if (slot == (size_t)-1) {
                    dnabwt_search_index_free(idx);
                    return DNABWT_STATUS_INTERNAL_ERROR;
                }
                accum5[slot] += 1u;

                cur_run_skip++;
                if (cur_run_skip >= cur_run_len) {
                    cur_run_off++;
                    cur_run_skip = 0u;
                    cur_run_valid = 0u;
                }
            }

            if ((full_pos & 0x1FFFu) == 0u && dnabwt_progress_report(full_pos, n) != 0) {
                dnabwt_search_index_free(idx);
                return DNABWT_STATUS_INTERRUPTED;
            }
            if ((full_pos & 0x3FFFu) == 0u && dnabwt_signal_poll() != 0) {
                dnabwt_search_index_free(idx);
                return DNABWT_STATUS_INTERRUPTED;
            }
        }

        idx->totals[0u] = accum5[DNABWT_SLOT_SENTINEL];
        idx->totals[(unsigned char)'A'] = accum5[DNABWT_SLOT_A];
        idx->totals[(unsigned char)'G'] = accum5[DNABWT_SLOT_G];
        idx->totals[(unsigned char)'C'] = accum5[DNABWT_SLOT_C];
        idx->totals[(unsigned char)'T'] = accum5[DNABWT_SLOT_T];

        {
            size_t total = 0;
            for (i = 0; i < 256u; ++i) {
                idx->c_table[i] = total;
                total += idx->totals[i];
            }
        }
    }

    dnabwt_progress_report(n, n);
    *index_out = idx;
    dnabwt_log(DNABWT_LOG_INFO,
               "bwt search index build finish: status=%s n=%zu peak_mem_bytes=%zu",
               dnabwt_status_message(DNABWT_STATUS_OK),
               idx->n,
               dnabwt_mem_peak());
    return DNABWT_STATUS_OK;
}

static dnabwt_status_t dnabwt_search_count_with_index(dnabwt_search_index_t *index,
                                                       const uint8_t *pattern,
                                                       size_t pattern_len,
                                                       size_t *match_count) {
    size_t i;
    size_t l = 0u;
    size_t r;

    dnabwt_mem_reset_peak();
    dnabwt_log(DNABWT_LOG_INFO, "bwt search(index) start: pattern_len=%zu", pattern_len);

    if (index == NULL || pattern == NULL || match_count == NULL) {
        dnabwt_log(DNABWT_LOG_WARN, "bwt search(index) invalid argument");
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }

    for (i = 0; i < pattern_len; ++i) {
        if (pattern[i] != 'A' && pattern[i] != 'G' && pattern[i] != 'C' && pattern[i] != 'T') {
            return DNABWT_STATUS_INVALID_ARGUMENT;
        }
    }

    r = index->n;
    for (i = pattern_len; i > 0; --i) {
        uint8_t c = pattern[i - 1u];
        dnabwt_status_t st = DNABWT_STATUS_OK;
        size_t l_new = index->c_table[c] + dnabwt_search_occ_at(index, c, l, &st);
        size_t r_new;
        if (st != DNABWT_STATUS_OK) {
            return st;
        }
        r_new = index->c_table[c] + dnabwt_search_occ_at(index, c, r, &st);
        if (st != DNABWT_STATUS_OK) {
            return st;
        }

        if (l_new >= r_new) {
            *match_count = 0u;
            return DNABWT_STATUS_OK;
        }
        l = l_new;
        r = r_new;

        if ((i & 0x1FFFu) == 0u && dnabwt_progress_report(pattern_len - i, pattern_len) != 0) {
            return DNABWT_STATUS_INTERRUPTED;
        }
        if ((i & 0x3FFFu) == 0u && dnabwt_signal_poll() != 0) {
            return DNABWT_STATUS_INTERRUPTED;
        }
    }

    *match_count = r - l;
    dnabwt_progress_report(pattern_len, pattern_len);
    dnabwt_log(DNABWT_LOG_INFO,
               "bwt search(index) finish: status=%s match_count=%zu peak_mem_bytes=%zu",
               dnabwt_status_message(DNABWT_STATUS_OK),
               *match_count,
               dnabwt_mem_peak());
    return DNABWT_STATUS_OK;
}

void dnabwt_search_index_free(dnabwt_search_index_t *index) {
    size_t i;

    if (index == NULL) {
        return;
    }

    if (index->caches != NULL) {
        for (i = 0; i < index->cache_count; ++i) {
            dnabwt_free(index->caches[i].data);
        }
    }

    dnabwt_free(index->caches);
    dnabwt_free(index->occ5);
    dnabwt_free(index->nav);
    dnabwt_reader_close(index->reader);
    dnabwt_free(index);
}

dnabwt_status_t dnabwt_search_index_build_from_encoded_file(const char *encoded_path,
                                                            size_t block_size,
                                                            size_t cache_count,
                                                            dnabwt_search_index_t **index_out) {
    return dnabwt_search_build_index(encoded_path, block_size, cache_count, index_out);
}

dnabwt_status_t dnabwt_search_index_count(dnabwt_search_index_t *index,
                                          const uint8_t *pattern,
                                          size_t pattern_len,
                                          size_t *match_count) {
    return dnabwt_search_count_with_index(index, pattern, pattern_len, match_count);
}

/* ---------------- Legacy bytes mode ---------------- */

dnabwt_status_t dnabwt_build_fm_index(const uint8_t *bwt, size_t n, dnabwt_fm_index_t *index) {
    size_t i;

    if (bwt == NULL || index == NULL || n == 0) {
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }

    memset(index, 0, sizeof(*index));
    index->occ_stride = 256u;
    index->occ = (size_t *)dnabwt_calloc((n + 1u) * index->occ_stride, sizeof(size_t));
    if (index->occ == NULL) {
        return DNABWT_STATUS_NO_MEMORY;
    }

    for (i = 0; i < n; ++i) {
        uint8_t c = bwt[i];
        size_t row = (i + 1u) * index->occ_stride;
        size_t prev = i * index->occ_stride;
        memcpy(index->occ + row, index->occ + prev, index->occ_stride * sizeof(size_t));
        index->occ[row + c] += 1u;
    }

    {
        size_t total = 0;
        for (i = 0; i < 256u; ++i) {
            index->c_table[i] = total;
            total += index->occ[n * index->occ_stride + i];
        }
    }

    index->n = n;
    return DNABWT_STATUS_OK;
}

size_t dnabwt_rank(const dnabwt_fm_index_t *index, uint8_t symbol, size_t pos) {
    if (index == NULL || index->occ == NULL) {
        return 0;
    }
    if (pos > index->n) {
        pos = index->n;
    }
    return index->occ[pos * index->occ_stride + symbol];
}

void dnabwt_free_fm_index(dnabwt_fm_index_t *index) {
    if (index == NULL) {
        return;
    }
    dnabwt_free(index->occ);
    memset(index, 0, sizeof(*index));
}

static dnabwt_status_t dnabwt_unpack_bwt(const uint8_t *encoded,
                                         size_t encoded_len,
                                         uint8_t **bwt_full,
                                         size_t *n_out) {
    uint64_t sentinel_index64;
    size_t sentinel_index;
    uint8_t *decoded = NULL;
    size_t decoded_len = 0;
    size_t n;
    uint8_t *full;
    dnabwt_status_t status;

    if (encoded == NULL || bwt_full == NULL || n_out == NULL) {
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }
    *bwt_full = NULL;
    *n_out = 0;

    if (encoded_len < DNABWT_SENTINEL_HEADER_SIZE) {
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }

    status = dnabwt_sentinel_unpack(encoded, &sentinel_index64);
    if (status != DNABWT_STATUS_OK) {
        return status;
    }

    status = dnabwt_rle_decode(encoded + DNABWT_SENTINEL_HEADER_SIZE,
                               encoded_len - DNABWT_SENTINEL_HEADER_SIZE,
                               &decoded,
                               &decoded_len);
    if (status != DNABWT_STATUS_OK) {
        return status;
    }

    n = decoded_len + 1u;
    sentinel_index = (size_t)sentinel_index64;
    if (sentinel_index >= n) {
        dnabwt_free(decoded);
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }

    full = (uint8_t *)dnabwt_malloc(n);
    if (full == NULL) {
        dnabwt_free(decoded);
        return DNABWT_STATUS_NO_MEMORY;
    }

    if (sentinel_index > 0) {
        memcpy(full, decoded, sentinel_index);
    }
    full[sentinel_index] = 0u;
    if (decoded_len > sentinel_index) {
        memcpy(full + sentinel_index + 1u, decoded + sentinel_index, decoded_len - sentinel_index);
    }

    dnabwt_free(decoded);
    *bwt_full = full;
    *n_out = n;
    return DNABWT_STATUS_OK;
}

dnabwt_status_t dnabwt_search_count(const uint8_t *encoded_bwt,
                                    size_t encoded_bwt_len,
                                    const uint8_t *pattern,
                                    size_t pattern_len,
                                    size_t *match_count) {
    uint8_t *bwt_full = NULL;
    size_t n = 0;
    dnabwt_fm_index_t index;
    dnabwt_status_t status;
    size_t l = 0;
    size_t r;
    size_t i;

    dnabwt_mem_reset_peak();
    dnabwt_log(DNABWT_LOG_INFO, "bwt search(bytes) start: encoded_len=%zu pattern_len=%zu", encoded_bwt_len, pattern_len);

    if (encoded_bwt == NULL || pattern == NULL || match_count == NULL) {
        dnabwt_log(DNABWT_LOG_WARN, "bwt search(bytes) invalid argument");
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }

    for (i = 0; i < pattern_len; ++i) {
        if (pattern[i] != 'A' && pattern[i] != 'G' && pattern[i] != 'C' && pattern[i] != 'T') {
            dnabwt_log(DNABWT_LOG_WARN, "bwt search(bytes) invalid pattern");
            return DNABWT_STATUS_INVALID_ARGUMENT;
        }
    }

    status = dnabwt_unpack_bwt(encoded_bwt, encoded_bwt_len, &bwt_full, &n);
    if (status != DNABWT_STATUS_OK) {
        dnabwt_log(DNABWT_LOG_WARN, "bwt search(bytes) unpack failed: %s", dnabwt_status_message(status));
        return status;
    }

    memset(&index, 0, sizeof(index));
    status = dnabwt_build_fm_index(bwt_full, n, &index);
    if (status != DNABWT_STATUS_OK) {
        dnabwt_free(bwt_full);
        dnabwt_log(DNABWT_LOG_ERROR, "bwt search(bytes) build fm index failed: %s", dnabwt_status_message(status));
        return status;
    }

    r = n;
    for (i = pattern_len; i > 0; --i) {
        uint8_t c = pattern[i - 1u];
        size_t l_new = index.c_table[c] + dnabwt_rank(&index, c, l);
        size_t r_new = index.c_table[c] + dnabwt_rank(&index, c, r);
        if (l_new >= r_new) {
            *match_count = 0;
            dnabwt_free_fm_index(&index);
            dnabwt_free(bwt_full);
            dnabwt_log(DNABWT_LOG_INFO,
                       "bwt search(bytes) finish: status=%s match_count=0 peak_mem_bytes=%zu",
                       dnabwt_status_message(DNABWT_STATUS_OK),
                       dnabwt_mem_peak());
            return DNABWT_STATUS_OK;
        }
        l = l_new;
        r = r_new;
    }

    *match_count = r - l;
    dnabwt_log(DNABWT_LOG_INFO,
               "bwt search(bytes) finish: status=%s match_count=%zu peak_mem_bytes=%zu",
               dnabwt_status_message(DNABWT_STATUS_OK),
               *match_count,
               dnabwt_mem_peak());
    dnabwt_free_fm_index(&index);
    dnabwt_free(bwt_full);
    return DNABWT_STATUS_OK;
}
