#ifndef DNABWT_CORE_API_H
#define DNABWT_CORE_API_H

#include <stddef.h>
#include <stdint.h>

#include "../../c/bwt/bwt.h"
#include "../../c/util/status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*dnabwt_core_progress_cb)(void *user_data, uint64_t processed_chars, uint64_t total_chars);
typedef int (*dnabwt_core_signal_cb)(void *user_data);

typedef struct dnabwt_core_ctx {
    dnabwt_core_progress_cb progress_cb;
    void *progress_user_data;
    dnabwt_core_signal_cb signal_cb;
    void *signal_user_data;
} dnabwt_core_ctx_t;

void dnabwt_core_ctx_init(dnabwt_core_ctx_t *ctx);
void dnabwt_core_ctx_set_progress(dnabwt_core_ctx_t *ctx, dnabwt_core_progress_cb cb, void *user_data);
void dnabwt_core_ctx_set_signal(dnabwt_core_ctx_t *ctx, dnabwt_core_signal_cb cb, void *user_data);

dnabwt_status_t dnabwt_core_transform_text(dnabwt_core_ctx_t *ctx,
                                           const uint8_t *input,
                                           size_t input_len,
                                           uint8_t **output,
                                           size_t *output_len);
dnabwt_status_t dnabwt_core_inverse_text(dnabwt_core_ctx_t *ctx,
                                         const uint8_t *input,
                                         size_t input_len,
                                         uint8_t **output,
                                         size_t *output_len);
dnabwt_status_t dnabwt_core_search_count(dnabwt_core_ctx_t *ctx,
                                         const uint8_t *encoded_bwt,
                                         size_t encoded_bwt_len,
                                         const uint8_t *pattern,
                                         size_t pattern_len,
                                         size_t *match_count);

dnabwt_status_t dnabwt_core_search_index_build_from_encoded_file(dnabwt_core_ctx_t *ctx,
                                                                 const char *encoded_path,
                                                                 size_t block_size,
                                                                 size_t cache_count,
                                                                 dnabwt_search_index_t **index_out);
dnabwt_status_t dnabwt_core_search_index_count(dnabwt_core_ctx_t *ctx,
                                                dnabwt_search_index_t *index,
                                                const uint8_t *pattern,
                                                size_t pattern_len,
                                                size_t *match_count);
void dnabwt_core_search_index_free(dnabwt_search_index_t *index);

dnabwt_status_t dnabwt_core_transform_file(dnabwt_core_ctx_t *ctx,
                                           const char *path,
                                           uint8_t **output,
                                           size_t *output_len);
dnabwt_status_t dnabwt_core_inverse_file(dnabwt_core_ctx_t *ctx,
                                         const char *path,
                                         uint8_t **output,
                                         size_t *output_len);

#ifdef __cplusplus
}
#endif

#endif
