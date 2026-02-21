#ifndef DNABWT_CODE_SENTINEL_H
#define DNABWT_CODE_SENTINEL_H

#include <stddef.h>
#include <stdint.h>

#include "../util/status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DNABWT_SENTINEL_HEADER_SIZE 8u

dnabwt_status_t dnabwt_sentinel_pack(uint64_t sentinel_index, uint8_t header[DNABWT_SENTINEL_HEADER_SIZE]);
dnabwt_status_t dnabwt_sentinel_unpack(const uint8_t header[DNABWT_SENTINEL_HEADER_SIZE], uint64_t *sentinel_index);
size_t dnabwt_sentinel_offset(size_t position_without_sentinel, size_t sentinel_index);

#ifdef __cplusplus
}
#endif

#endif
