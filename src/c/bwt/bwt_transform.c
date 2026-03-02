#include <stdlib.h>
#include <string.h>

#include "bwt.h"
#include "bwt_internal.h"
#include "../code/dna_rle.h"
#include "../code/sentinel.h"
#include "../util/util.h"

static const uint8_t *g_sa_text = NULL;
static size_t g_sa_n = 0;

static int dnabwt_sa_cmp(const void *a, const void *b) {
    size_t i = *(const size_t *)a;
    size_t j = *(const size_t *)b;
    size_t k;

    for (k = 0; k < g_sa_n; ++k) {
        uint8_t ci = g_sa_text[(i + k) % g_sa_n];
        uint8_t cj = g_sa_text[(j + k) % g_sa_n];
        if (ci < cj) {
            return -1;
        }
        if (ci > cj) {
            return 1;
        }
    }
    return 0;
}

dnabwt_status_t dnabwt_validate_dna_text(const uint8_t *text, size_t len, size_t *dna_len_out) {
    size_t i;
    size_t dna_len;

    if (text == NULL || dna_len_out == NULL) {
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }

    dna_len = len;
    if (len > 0 && text[len - 1u] == '\n') {
        dna_len = len - 1u;
    }

    for (i = 0; i < dna_len; ++i) {
        if (text[i] != 'A' && text[i] != 'G' && text[i] != 'C' && text[i] != 'T') {
            return DNABWT_STATUS_INVALID_ARGUMENT;
        }
    }
    for (i = dna_len; i < len; ++i) {
        if (text[i] != '\n') {
            return DNABWT_STATUS_INVALID_ARGUMENT;
        }
    }

    *dna_len_out = dna_len;
    return DNABWT_STATUS_OK;
}

dnabwt_status_t dnabwt_build_suffix_array(const uint8_t *text, size_t n, size_t **sa_out) {
    size_t *sa;
    size_t i;

    if (text == NULL || sa_out == NULL || n == 0) {
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }
    *sa_out = NULL;

    sa = (size_t *)dnabwt_malloc(n * sizeof(*sa));
    if (sa == NULL) {
        return DNABWT_STATUS_NO_MEMORY;
    }

    for (i = 0; i < n; ++i) {
        sa[i] = i;
    }

    g_sa_text = text;
    g_sa_n = n;
    qsort(sa, n, sizeof(*sa), dnabwt_sa_cmp);

    *sa_out = sa;
    return DNABWT_STATUS_OK;
}

dnabwt_status_t dnabwt_transform_text(const uint8_t *input,
                                      size_t input_len,
                                      uint8_t **output,
                                      size_t *output_len) {
    size_t dna_len;
    size_t n;
    uint8_t *text = NULL;
    size_t *sa = NULL;
    uint8_t *bwt = NULL;
    size_t sentinel_index = 0;
    uint8_t *rle = NULL;
    size_t rle_len = 0;
    uint8_t *packed = NULL;
    dnabwt_status_t status;
    size_t i;

    dnabwt_mem_reset_peak();
    dnabwt_log(DNABWT_LOG_INFO, "bwt transform start: input_len=%zu", input_len);

    if (output == NULL || output_len == NULL || input == NULL) {
        dnabwt_log(DNABWT_LOG_WARN, "bwt transform invalid argument");
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }
    *output = NULL;
    *output_len = 0;

    status = dnabwt_validate_dna_text(input, input_len, &dna_len);
    if (status != DNABWT_STATUS_OK) {
        dnabwt_log(DNABWT_LOG_WARN, "bwt transform input validation failed: %s", dnabwt_status_message(status));
        return status;
    }

    n = dna_len + 1u;
    text = (uint8_t *)dnabwt_malloc(n);
    if (text == NULL) {
        dnabwt_log(DNABWT_LOG_ERROR, "bwt transform alloc failed for text");
        return DNABWT_STATUS_NO_MEMORY;
    }
    memcpy(text, input, dna_len);
    text[dna_len] = 0u;

    status = dnabwt_build_suffix_array(text, n, &sa);
    if (status != DNABWT_STATUS_OK) {
        dnabwt_free(text);
        dnabwt_log(DNABWT_LOG_ERROR, "bwt transform suffix array build failed: %s", dnabwt_status_message(status));
        return status;
    }

    bwt = (uint8_t *)dnabwt_malloc(n);
    if (bwt == NULL) {
        dnabwt_free(sa);
        dnabwt_free(text);
        dnabwt_log(DNABWT_LOG_ERROR, "bwt transform alloc failed for bwt buffer");
        return DNABWT_STATUS_NO_MEMORY;
    }

    for (i = 0; i < n; ++i) {
        size_t pos = sa[i];
        bwt[i] = text[(pos + n - 1u) % n];
        if (bwt[i] == 0u) {
            sentinel_index = i;
        }

        if ((i & 0x1FFFu) == 0 && dnabwt_progress_report(i, n) != 0) {
            status = DNABWT_STATUS_INTERRUPTED;
            goto cleanup;
        }
        if ((i & 0x3FFFu) == 0 && dnabwt_signal_poll() != 0) {
            status = DNABWT_STATUS_INTERRUPTED;
            goto cleanup;
        }
    }
    dnabwt_progress_report(n, n);

    status = dnabwt_rle_encode_segments(
        bwt,
        sentinel_index,
        bwt + sentinel_index + 1u,
        n - sentinel_index - 1u,
        &rle,
        &rle_len);
    if (status != DNABWT_STATUS_OK) {
        goto cleanup;
    }

    packed = (uint8_t *)dnabwt_malloc(DNABWT_SENTINEL_HEADER_SIZE + rle_len);
    if (packed == NULL) {
        status = DNABWT_STATUS_NO_MEMORY;
        goto cleanup;
    }

    status = dnabwt_sentinel_pack((uint64_t)sentinel_index, packed);
    if (status != DNABWT_STATUS_OK) {
        goto cleanup;
    }
    if (rle_len > 0) {
        memcpy(packed + DNABWT_SENTINEL_HEADER_SIZE, rle, rle_len);
    }

    *output = packed;
    *output_len = DNABWT_SENTINEL_HEADER_SIZE + rle_len;
    packed = NULL;
    status = DNABWT_STATUS_OK;

cleanup:
    dnabwt_log(DNABWT_LOG_INFO,
               "bwt transform finish: status=%s output_len=%zu peak_mem_bytes=%zu",
               dnabwt_status_message(status),
               (status == DNABWT_STATUS_OK) ? *output_len : 0u,
               dnabwt_mem_peak());
    dnabwt_free(packed);
    dnabwt_free(rle);
    dnabwt_free(bwt);
    dnabwt_free(sa);
    dnabwt_free(text);
    return status;
}
