#ifndef DNABWT_BWT_BWT_H
#define DNABWT_BWT_BWT_H

#include <stddef.h>
#include <stdint.h>

#include "../util/status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dnabwt_search_index dnabwt_search_index_t;

dnabwt_status_t dnabwt_transform_text(const uint8_t *input,
                                      size_t input_len,
                                      uint8_t **output,
                                      size_t *output_len);
dnabwt_status_t dnabwt_inverse_text(const uint8_t *input,
                                    size_t input_len,
                                    uint8_t **output,
                                    size_t *output_len);

/* Legacy bytes-mode search entry. */
dnabwt_status_t dnabwt_search_count(const uint8_t *encoded_bwt,
                                    size_t encoded_bwt_len,
                                    const uint8_t *pattern,
                                    size_t pattern_len,
                                    size_t *match_count);

/* New persistent index API backed by encoded file + reader caches. */
dnabwt_status_t dnabwt_search_index_build_from_encoded_file(const char *encoded_path,
                                                            size_t block_size,
                                                            size_t cache_count,
                                                            dnabwt_search_index_t **index_out);
dnabwt_status_t dnabwt_search_index_count(dnabwt_search_index_t *index,
                                          const uint8_t *pattern,
                                          size_t pattern_len,
                                          size_t *match_count);
void dnabwt_search_index_free(dnabwt_search_index_t *index);

#ifdef __cplusplus
}
#endif

#endif
