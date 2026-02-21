#include <stdio.h>
#include <string.h>

#include "io.h"
#include "../util/util.h"

static dnabwt_reader_cache_t *dnabwt_reader_find_cache(dnabwt_reader_t *reader, size_t block_index) {
    size_t i;

    for (i = 0; i < reader->cache_count; ++i) {
        if (reader->caches[i].length > 0 && reader->caches[i].block_index == block_index) {
            reader->caches[i].stamp = ++reader->clock;
            return &reader->caches[i];
        }
    }
    return NULL;
}

static dnabwt_reader_cache_t *dnabwt_reader_pick_victim(dnabwt_reader_t *reader) {
    size_t i;
    size_t victim = 0;
    uint64_t oldest = UINT64_MAX;

    for (i = 0; i < reader->cache_count; ++i) {
        if (reader->caches[i].length == 0) {
            return &reader->caches[i];
        }
        if (reader->caches[i].stamp < oldest) {
            oldest = reader->caches[i].stamp;
            victim = i;
        }
    }
    return &reader->caches[victim];
}

static dnabwt_status_t dnabwt_reader_fill_cache(dnabwt_reader_t *reader,
                                                dnabwt_reader_cache_t *cache,
                                                size_t block_index) {
    FILE *fp = (FILE *)reader->fp;
    size_t offset = block_index * reader->block_size;
    size_t remain;

    if (offset >= reader->file_size) {
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }

    remain = reader->file_size - offset;
    if (remain > reader->block_size) {
        remain = reader->block_size;
    }

    if (fseek(fp, (long)offset, SEEK_SET) != 0) {
        return DNABWT_STATUS_IO_ERROR;
    }
    if (fread(cache->data, 1u, remain, fp) != remain) {
        return DNABWT_STATUS_IO_ERROR;
    }

    cache->block_index = block_index;
    cache->length = remain;
    cache->stamp = ++reader->clock;
    return DNABWT_STATUS_OK;
}

dnabwt_status_t dnabwt_reader_open(dnabwt_reader_t **reader,
                                   const char *path,
                                   size_t block_size,
                                   size_t cache_count) {
    dnabwt_reader_t *r;
    FILE *fp;
    long end;
    size_t i;

    if (reader == NULL || path == NULL) {
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }
    *reader = NULL;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return DNABWT_STATUS_IO_ERROR;
    }

    if (fseek(fp, 0L, SEEK_END) != 0) {
        fclose(fp);
        return DNABWT_STATUS_IO_ERROR;
    }
    end = ftell(fp);
    if (end < 0) {
        fclose(fp);
        return DNABWT_STATUS_IO_ERROR;
    }
    if (fseek(fp, 0L, SEEK_SET) != 0) {
        fclose(fp);
        return DNABWT_STATUS_IO_ERROR;
    }

    r = (dnabwt_reader_t *)dnabwt_calloc(1u, sizeof(*r));
    if (r == NULL) {
        fclose(fp);
        return DNABWT_STATUS_NO_MEMORY;
    }

    r->fp = fp;
    r->file_size = (size_t)end;
    r->block_size = block_size == 0 ? (1u << 16) : block_size;
    r->cache_count = cache_count == 0 ? 4u : cache_count;
    r->status = DNABWT_STATUS_OK;

    r->caches = (dnabwt_reader_cache_t *)dnabwt_calloc(r->cache_count, sizeof(*r->caches));
    if (r->caches == NULL) {
        dnabwt_reader_close(r);
        return DNABWT_STATUS_NO_MEMORY;
    }

    for (i = 0; i < r->cache_count; ++i) {
        r->caches[i].data = (uint8_t *)dnabwt_malloc(r->block_size);
        if (r->caches[i].data == NULL) {
            dnabwt_reader_close(r);
            return DNABWT_STATUS_NO_MEMORY;
        }
    }

    *reader = r;
    return DNABWT_STATUS_OK;
}

dnabwt_status_t dnabwt_reader_read_at(dnabwt_reader_t *reader, size_t offset, uint8_t *out, size_t length) {
    size_t copied = 0;

    if (reader == NULL || out == NULL) {
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }
    if (offset + length > reader->file_size) {
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }

    while (copied < length) {
        size_t pos = offset + copied;
        size_t block_index = pos / reader->block_size;
        size_t block_offset = pos % reader->block_size;
        size_t need = length - copied;
        dnabwt_reader_cache_t *cache = dnabwt_reader_find_cache(reader, block_index);

        if (cache == NULL) {
            dnabwt_status_t status;
            cache = dnabwt_reader_pick_victim(reader);
            status = dnabwt_reader_fill_cache(reader, cache, block_index);
            if (status != DNABWT_STATUS_OK) {
                return status;
            }
        }

        if (need > cache->length - block_offset) {
            need = cache->length - block_offset;
        }

        memcpy(out + copied, cache->data + block_offset, need);
        copied += need;
    }

    return DNABWT_STATUS_OK;
}

void dnabwt_reader_close(dnabwt_reader_t *reader) {
    size_t i;

    if (reader == NULL) {
        return;
    }

    if (reader->caches != NULL) {
        for (i = 0; i < reader->cache_count; ++i) {
            dnabwt_free(reader->caches[i].data);
        }
        dnabwt_free(reader->caches);
    }

    if (reader->fp != NULL) {
        fclose((FILE *)reader->fp);
    }

    dnabwt_free(reader);
}
