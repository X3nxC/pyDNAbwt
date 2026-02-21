#include "sentinel.h"

dnabwt_status_t dnabwt_sentinel_pack(uint64_t sentinel_index, uint8_t header[DNABWT_SENTINEL_HEADER_SIZE]) {
    size_t i;
    if (header == NULL) {
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }
    for (i = 0; i < DNABWT_SENTINEL_HEADER_SIZE; ++i) {
        header[i] = (uint8_t)((sentinel_index >> (8u * i)) & 0xFFu);
    }
    return DNABWT_STATUS_OK;
}

dnabwt_status_t dnabwt_sentinel_unpack(const uint8_t header[DNABWT_SENTINEL_HEADER_SIZE], uint64_t *sentinel_index) {
    uint64_t value = 0;
    size_t i;

    if (header == NULL || sentinel_index == NULL) {
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }

    for (i = 0; i < DNABWT_SENTINEL_HEADER_SIZE; ++i) {
        value |= ((uint64_t)header[i]) << (8u * i);
    }

    *sentinel_index = value;
    return DNABWT_STATUS_OK;
}

size_t dnabwt_sentinel_offset(size_t position_without_sentinel, size_t sentinel_index) {
    if (position_without_sentinel >= sentinel_index) {
        return position_without_sentinel + 1u;
    }
    return position_without_sentinel;
}
