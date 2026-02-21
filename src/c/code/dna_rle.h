#ifndef DNABWT_CODE_DNA_RLE_H
#define DNABWT_CODE_DNA_RLE_H

#include <stddef.h>
#include <stdint.h>

#include "../util/status.h"

#ifdef __cplusplus
extern "C" {
#endif

size_t dnabwt_rle_encoded_bound(size_t input_len);
dnabwt_status_t dnabwt_rle_encode(const uint8_t *input,
                                  size_t input_len,
                                  uint8_t **output,
                                  size_t *output_len);
dnabwt_status_t dnabwt_rle_encode_segments(const uint8_t *first,
                                           size_t first_len,
                                           const uint8_t *second,
                                           size_t second_len,
                                           uint8_t **output,
                                           size_t *output_len);
dnabwt_status_t dnabwt_rle_decode(const uint8_t *input,
                                  size_t input_len,
                                  uint8_t **output,
                                  size_t *output_len);

#ifdef __cplusplus
}
#endif

#endif
