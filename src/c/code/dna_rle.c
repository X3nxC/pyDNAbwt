#include <string.h>

#include "dna_rle.h"
#include "../util/util.h"

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
        default:
            return 0;
    }
}

static uint8_t dnabwt_code_to_symbol(uint8_t code) {
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

        while (i + run < input_len && input[i + run] == input[i] && run < 64u) {
            ++run;
        }

        output[*cursor] = (uint8_t)((code << 6u) | ((uint8_t)run - 1u));
        *cursor += 1u;
        i += run;
    }

    return DNABWT_STATUS_OK;
}

size_t dnabwt_rle_encoded_bound(size_t input_len) {
    return input_len;
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

    for (i = 0; i < input_len; ++i) {
        total += (size_t)((input[i] & 0x3Fu) + 1u);
    }

    buf = (uint8_t *)dnabwt_malloc(total == 0 ? 1u : total);
    if (buf == NULL) {
        return DNABWT_STATUS_NO_MEMORY;
    }

    for (i = 0; i < input_len; ++i) {
        uint8_t code = (uint8_t)(input[i] >> 6u);
        uint8_t symbol = dnabwt_code_to_symbol(code);
        size_t run = (size_t)((input[i] & 0x3Fu) + 1u);
        if (symbol == 0u) {
            dnabwt_free(buf);
            return DNABWT_STATUS_INVALID_ARGUMENT;
        }
        memset(buf + cursor, symbol, run);
        cursor += run;
    }

    *output = buf;
    *output_len = total;
    return DNABWT_STATUS_OK;
}
