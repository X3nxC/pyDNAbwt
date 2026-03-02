#include <string.h>

#include "core_api.h"

#include "../../c/io/io.h"
#include "../../c/util/util.h"

static dnabwt_status_t dnabwt_core_read_all(const char *path, uint8_t **buffer, size_t *size) {
    dnabwt_reader_t *reader = NULL;
    uint8_t *data = NULL;
    dnabwt_status_t status;

    if (path == NULL || buffer == NULL || size == NULL) {
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }
    *buffer = NULL;
    *size = 0;

    status = dnabwt_reader_open(&reader, path, 1u << 16, 4u);
    if (status != DNABWT_STATUS_OK) {
        return status;
    }

    data = (uint8_t *)dnabwt_malloc(reader->file_size == 0 ? 1u : reader->file_size);
    if (data == NULL) {
        dnabwt_reader_close(reader);
        return DNABWT_STATUS_NO_MEMORY;
    }

    if (reader->file_size > 0) {
        status = dnabwt_reader_read_at(reader, 0u, data, reader->file_size);
        if (status != DNABWT_STATUS_OK) {
            dnabwt_reader_close(reader);
            dnabwt_free(data);
            return status;
        }
    }

    *buffer = data;
    *size = reader->file_size;
    dnabwt_reader_close(reader);
    return DNABWT_STATUS_OK;
}

static void dnabwt_core_apply_ctx(dnabwt_core_ctx_t *ctx) {
    if (ctx == NULL) {
        dnabwt_progress_set_callback(NULL, NULL);
        dnabwt_signal_set_checker(NULL, NULL);
        return;
    }
    dnabwt_progress_set_callback((dnabwt_progress_cb)ctx->progress_cb, ctx->progress_user_data);
    dnabwt_signal_set_checker((dnabwt_signal_check_cb)ctx->signal_cb, ctx->signal_user_data);
}

void dnabwt_core_ctx_init(dnabwt_core_ctx_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
}

void dnabwt_core_ctx_set_progress(dnabwt_core_ctx_t *ctx, dnabwt_core_progress_cb cb, void *user_data) {
    if (ctx == NULL) {
        return;
    }
    ctx->progress_cb = cb;
    ctx->progress_user_data = user_data;
}

void dnabwt_core_ctx_set_signal(dnabwt_core_ctx_t *ctx, dnabwt_core_signal_cb cb, void *user_data) {
    if (ctx == NULL) {
        return;
    }
    ctx->signal_cb = cb;
    ctx->signal_user_data = user_data;
}

dnabwt_status_t dnabwt_core_transform_text(dnabwt_core_ctx_t *ctx,
                                           const uint8_t *input,
                                           size_t input_len,
                                           uint8_t **output,
                                           size_t *output_len) {
    dnabwt_status_t status;
    dnabwt_core_apply_ctx(ctx);
    status = dnabwt_transform_text(input, input_len, output, output_len);
    dnabwt_core_apply_ctx(NULL);
    return status;
}

dnabwt_status_t dnabwt_core_inverse_text(dnabwt_core_ctx_t *ctx,
                                         const uint8_t *input,
                                         size_t input_len,
                                         uint8_t **output,
                                         size_t *output_len) {
    dnabwt_status_t status;
    dnabwt_core_apply_ctx(ctx);
    status = dnabwt_inverse_text(input, input_len, output, output_len);
    dnabwt_core_apply_ctx(NULL);
    return status;
}

dnabwt_status_t dnabwt_core_search_count(dnabwt_core_ctx_t *ctx,
                                         const uint8_t *encoded_bwt,
                                         size_t encoded_bwt_len,
                                         const uint8_t *pattern,
                                         size_t pattern_len,
                                         size_t *match_count) {
    dnabwt_status_t status;
    dnabwt_core_apply_ctx(ctx);
    status = dnabwt_search_count(encoded_bwt, encoded_bwt_len, pattern, pattern_len, match_count);
    dnabwt_core_apply_ctx(NULL);
    return status;
}

dnabwt_status_t dnabwt_core_search_index_build_from_encoded_file(dnabwt_core_ctx_t *ctx,
                                                                 const char *encoded_path,
                                                                 size_t block_size,
                                                                 size_t cache_count,
                                                                 dnabwt_search_index_t **index_out) {
    dnabwt_status_t status;
    dnabwt_core_apply_ctx(ctx);
    status = dnabwt_search_index_build_from_encoded_file(encoded_path, block_size, cache_count, index_out);
    dnabwt_core_apply_ctx(NULL);
    return status;
}

dnabwt_status_t dnabwt_core_search_index_count(dnabwt_core_ctx_t *ctx,
                                                dnabwt_search_index_t *index,
                                                const uint8_t *pattern,
                                                size_t pattern_len,
                                                size_t *match_count) {
    dnabwt_status_t status;
    dnabwt_core_apply_ctx(ctx);
    status = dnabwt_search_index_count(index, pattern, pattern_len, match_count);
    dnabwt_core_apply_ctx(NULL);
    return status;
}

void dnabwt_core_search_index_free(dnabwt_search_index_t *index) {
    dnabwt_search_index_free(index);
}

dnabwt_status_t dnabwt_core_transform_file(dnabwt_core_ctx_t *ctx,
                                           const char *path,
                                           uint8_t **output,
                                           size_t *output_len) {
    uint8_t *input = NULL;
    size_t input_len = 0;
    dnabwt_status_t status = dnabwt_core_read_all(path, &input, &input_len);
    if (status != DNABWT_STATUS_OK) {
        return status;
    }

    status = dnabwt_core_transform_text(ctx, input, input_len, output, output_len);
    dnabwt_free(input);
    return status;
}

dnabwt_status_t dnabwt_core_inverse_file(dnabwt_core_ctx_t *ctx,
                                         const char *path,
                                         uint8_t **output,
                                         size_t *output_len) {
    uint8_t *input = NULL;
    size_t input_len = 0;
    dnabwt_status_t status = dnabwt_core_read_all(path, &input, &input_len);
    if (status != DNABWT_STATUS_OK) {
        return status;
    }

    status = dnabwt_core_inverse_text(ctx, input, input_len, output, output_len);
    dnabwt_free(input);
    return status;
}
