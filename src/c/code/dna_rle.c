#include <string.h>
#include <limits.h>

#include "dna_rle.h"
#include "../util/util.h"

#define DNABWT_RLE_MAX_RUN ((size_t)1u << 27u)

static int dnabwt_symbol_to_code(uint8_t symbol, uint8_t *code) {
    switch (symbol) {
        case 'A':
            *code = 0u;
            return 1;
        case 'G':
            *code = 1u;
            return 1;
        case 'C':
            *code = 2u;
            return 1;
        case 'T':
            *code = 3u;
            return 1;
        case 'N':
            *code = 4u;
            return 1;
        case 0u:
            *code = 5u;
            return 1;
        default:
            return 0;
    }
}

static int dnabwt_code_to_symbol(uint8_t code, uint8_t *symbol) {
    switch (code) {
        case 0u:
            *symbol = 'A';
            return 1;
        case 1u:
            *symbol = 'G';
            return 1;
        case 2u:
            *symbol = 'C';
            return 1;
        case 3u:
            *symbol = 'T';
            return 1;
        case 4u:
            *symbol = 'N';
            return 1;
        case 5u:
            *symbol = 0u;
            return 1;
        default:
            return 0;
    }
}

static size_t dnabwt_rle_bytes_for_run(size_t run) {
    if (run <= (1u << 3u)) {
        return 1u;
    }
    if (run <= (1u << 11u)) {
        return 2u;
    }
    if (run <= (1u << 19u)) {
        return 3u;
    }
    if (run <= DNABWT_RLE_MAX_RUN) {
        return 4u;
    }
    return 0u;
}

static void dnabwt_rle_write_run(uint8_t code, size_t run, uint8_t *output, size_t *cursor) {
    size_t bytes = dnabwt_rle_bytes_for_run(run);
    size_t run_minus_one = run - 1u;
    size_t j;

    output[*cursor] = (uint8_t)((code << 5u) | ((uint8_t)(bytes - 1u) << 3u) | (uint8_t)(run_minus_one & 0x07u));
    for (j = 1u; j < bytes; ++j) {
        output[*cursor + j] = (uint8_t)((run_minus_one >> (3u + 8u * (j - 1u))) & 0xFFu);
    }
    *cursor += bytes;
}

static dnabwt_status_t dnabwt_rle_read_run(const uint8_t *input,
                                           size_t input_len,
                                           size_t *cursor,
                                           uint8_t *symbol,
                                           size_t *run_len) {
    uint8_t b0;
    uint8_t code;
    size_t bytes;
    size_t run_minus_one;
    size_t j;

    if (input == NULL || cursor == NULL || symbol == NULL || run_len == NULL) {
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }
    if (*cursor >= input_len) {
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }

    b0 = input[*cursor];
    code = (uint8_t)(b0 >> 5u);
    bytes = (size_t)(((b0 >> 3u) & 0x03u) + 1u);
    if ((input_len - *cursor) < bytes) {
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }
    if (!dnabwt_code_to_symbol(code, symbol)) {
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }

    run_minus_one = (size_t)(b0 & 0x07u);
    for (j = 1u; j < bytes; ++j) {
        run_minus_one |= ((size_t)input[*cursor + j]) << (3u + 8u * (j - 1u));
    }

    *run_len = run_minus_one + 1u;
    *cursor += bytes;
    return DNABWT_STATUS_OK;
}

static dnabwt_status_t dnabwt_rle_encode_single(const uint8_t *input,
                                                size_t input_len,
                                                uint8_t *output,
                                                size_t *cursor) {
    size_t i = 0;

    while (i < input_len) {
        uint8_t code;
        size_t run = 1;

        if (!dnabwt_symbol_to_code(input[i], &code)) {
            return DNABWT_STATUS_INVALID_ARGUMENT;
        }

        while (i + run < input_len && input[i + run] == input[i] && run < DNABWT_RLE_MAX_RUN) {
            ++run;
        }

        dnabwt_rle_write_run(code, run, output, cursor);
        i += run;
    }

    return DNABWT_STATUS_OK;
}

size_t dnabwt_rle_encoded_bound(size_t input_len) {
    if (input_len > (SIZE_MAX / 4u)) {
        return SIZE_MAX;
    }
    return input_len * 4u;
}

dnabwt_status_t dnabwt_rle_encode(const uint8_t *input,
                                  size_t input_len,
                                  uint8_t **output,
                                  size_t *output_len) {
    return dnabwt_rle_encode_segments(input, input_len, NULL, 0u, output, output_len);
}

dnabwt_status_t dnabwt_rle_encode_segments(const uint8_t *first,
                                           size_t first_len,
                                           const uint8_t *second,
                                           size_t second_len,
                                           uint8_t **output,
                                           size_t *output_len) {
    uint8_t *buf;
    size_t cursor = 0;
    dnabwt_status_t status;

    if (output == NULL || output_len == NULL) {
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }
    *output = NULL;
    *output_len = 0;

    if ((first_len > 0 && first == NULL) || (second_len > 0 && second == NULL)) {
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }

    buf = (uint8_t *)dnabwt_malloc(dnabwt_rle_encoded_bound(first_len + second_len));
    if (buf == NULL && (first_len + second_len) > 0) {
        return DNABWT_STATUS_NO_MEMORY;
    }

    status = dnabwt_rle_encode_single(first, first_len, buf, &cursor);
    if (status != DNABWT_STATUS_OK) {
        dnabwt_free(buf);
        return status;
    }
    status = dnabwt_rle_encode_single(second, second_len, buf, &cursor);
    if (status != DNABWT_STATUS_OK) {
        dnabwt_free(buf);
        return status;
    }

    if (cursor == 0) {
        dnabwt_free(buf);
        buf = NULL;
    } else {
        uint8_t *shrink = (uint8_t *)dnabwt_realloc(buf, cursor);
        if (shrink != NULL) {
            buf = shrink;
        }
    }

    *output = buf;
    *output_len = cursor;
    return DNABWT_STATUS_OK;
}

dnabwt_status_t dnabwt_rle_decode(const uint8_t *input,
                                  size_t input_len,
                                  uint8_t **output,
                                  size_t *output_len) {
    size_t total = 0;
    uint8_t *buf;
    size_t i;
    size_t cursor = 0;

    if (output == NULL || output_len == NULL) {
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }
    *output = NULL;
    *output_len = 0;

    if (input_len > 0 && input == NULL) {
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }

    i = 0;
    while (i < input_len) {
        uint8_t symbol = 0u;
        size_t run = 0u;
        dnabwt_status_t status = dnabwt_rle_read_run(input, input_len, &i, &symbol, &run);
        if (status != DNABWT_STATUS_OK) {
            return status;
        }
        if (SIZE_MAX - total < run) {
            return DNABWT_STATUS_INVALID_ARGUMENT;
        }
        total += run;
    }

    buf = (uint8_t *)dnabwt_malloc(total == 0 ? 1u : total);
    if (buf == NULL) {
        return DNABWT_STATUS_NO_MEMORY;
    }

    i = 0;
    while (i < input_len) {
        uint8_t symbol = 0u;
        size_t run = 0u;
        dnabwt_status_t status = dnabwt_rle_read_run(input, input_len, &i, &symbol, &run);
        if (status != DNABWT_STATUS_OK) {
            dnabwt_free(buf);
            return status;
        }
        memset(buf + cursor, symbol, run);
        cursor += run;
    }

    *output = buf;
    *output_len = total;
    return DNABWT_STATUS_OK;
}
