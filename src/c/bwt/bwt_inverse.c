#include <string.h>

#include "bwt.h"
#include "bwt_internal.h"
#include "../code/dna_rle.h"
#include "../code/sentinel.h"
#include "../util/util.h"

dnabwt_status_t dnabwt_inverse_text(const uint8_t *input,
                                    size_t input_len,
                                    uint8_t **output,
                                    size_t *output_len) {
    uint64_t sentinel_index64;
    uint8_t *decoded = NULL;
    size_t decoded_len = 0;
    size_t n;
    size_t sentinel_index;
    uint8_t *bwt_full = NULL;
    dnabwt_fm_index_t index;
    size_t pos;
    uint8_t *text = NULL;
    size_t i;
    dnabwt_status_t status;

    dnabwt_mem_reset_peak();
    dnabwt_log(DNABWT_LOG_INFO, "bwt inverse start: input_len=%zu", input_len);

    if (input == NULL || output == NULL || output_len == NULL) {
        dnabwt_log(DNABWT_LOG_WARN, "bwt inverse invalid argument");
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }
    *output = NULL;
    *output_len = 0;

    if (input_len < DNABWT_SENTINEL_HEADER_SIZE) {
        dnabwt_log(DNABWT_LOG_WARN, "bwt inverse invalid argument: short input");
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }

    status = dnabwt_sentinel_unpack(input, &sentinel_index64);
    if (status != DNABWT_STATUS_OK) {
        dnabwt_log(DNABWT_LOG_WARN, "bwt inverse unpack sentinel failed: %s", dnabwt_status_message(status));
        return status;
    }

    status = dnabwt_rle_decode(input + DNABWT_SENTINEL_HEADER_SIZE,
                               input_len - DNABWT_SENTINEL_HEADER_SIZE,
                               &decoded,
                               &decoded_len);
    if (status != DNABWT_STATUS_OK) {
        dnabwt_log(DNABWT_LOG_WARN, "bwt inverse rle decode failed: %s", dnabwt_status_message(status));
        return status;
    }

    n = decoded_len + 1u;
    sentinel_index = (size_t)sentinel_index64;
    if (sentinel_index >= n) {
        dnabwt_free(decoded);
        dnabwt_log(DNABWT_LOG_WARN, "bwt inverse invalid argument: sentinel out of range");
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }

    bwt_full = (uint8_t *)dnabwt_malloc(n);
    if (bwt_full == NULL) {
        dnabwt_free(decoded);
        dnabwt_log(DNABWT_LOG_ERROR, "bwt inverse alloc failed for bwt_full");
        return DNABWT_STATUS_NO_MEMORY;
    }

    if (sentinel_index > 0) {
        memcpy(bwt_full, decoded, sentinel_index);
    }
    bwt_full[sentinel_index] = 0u;
    if (decoded_len > sentinel_index) {
        memcpy(bwt_full + sentinel_index + 1u, decoded + sentinel_index, decoded_len - sentinel_index);
    }

    memset(&index, 0, sizeof(index));
    status = dnabwt_build_fm_index(bwt_full, n, &index);
    if (status != DNABWT_STATUS_OK) {
        dnabwt_free(bwt_full);
        dnabwt_free(decoded);
        dnabwt_log(DNABWT_LOG_ERROR, "bwt inverse build fm index failed: %s", dnabwt_status_message(status));
        return status;
    }

    text = (uint8_t *)dnabwt_malloc(n);
    if (text == NULL) {
        dnabwt_free_fm_index(&index);
        dnabwt_free(bwt_full);
        dnabwt_free(decoded);
        dnabwt_log(DNABWT_LOG_ERROR, "bwt inverse alloc failed for output text");
        return DNABWT_STATUS_NO_MEMORY;
    }

    pos = sentinel_index;
    for (i = n - 1u; i > 0; --i) {
        uint8_t c;
        size_t rnk;

        c = bwt_full[pos];
        rnk = dnabwt_rank(&index, c, pos + 1u);
        pos = index.c_table[c] + rnk - 1u;
        c = bwt_full[pos];

        if (c == 0u) {
            status = DNABWT_STATUS_INTERNAL_ERROR;
            goto cleanup;
        }
        text[i - 1u] = c;

        if ((i & 0x1FFFu) == 0 && dnabwt_progress_report(n - i, n) != 0) {
            status = DNABWT_STATUS_INTERRUPTED;
            goto cleanup;
        }
        if ((i & 0x3FFFu) == 0 && dnabwt_signal_poll() != 0) {
            status = DNABWT_STATUS_INTERRUPTED;
            goto cleanup;
        }
    }

    text[n - 1u] = '\n';
    *output = text;
    *output_len = n;
    text = NULL;
    status = DNABWT_STATUS_OK;

cleanup:
    dnabwt_progress_report(n, n);
    dnabwt_log(DNABWT_LOG_INFO,
               "bwt inverse finish: status=%s output_len=%zu peak_mem_bytes=%zu",
               dnabwt_status_message(status),
               (status == DNABWT_STATUS_OK) ? *output_len : 0u,
               dnabwt_mem_peak());
    dnabwt_free(text);
    dnabwt_free_fm_index(&index);
    dnabwt_free(bwt_full);
    dnabwt_free(decoded);
    return status;
}
