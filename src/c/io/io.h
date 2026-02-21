#ifndef DNABWT_IO_IO_H
#define DNABWT_IO_IO_H

#include <stddef.h>
#include <stdint.h>

#include "../util/status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum dnabwt_writer_mode {
    DNABWT_WRITER_FORWARD = 0,
    DNABWT_WRITER_REVERSE = 1
} dnabwt_writer_mode_t;

typedef struct dnabwt_writer {
    void *fp;
    uint8_t *buffer;
    size_t capacity;
    size_t length;
    size_t total_size;
    size_t next_offset;
    dnabwt_writer_mode_t mode;
    dnabwt_status_t status;
} dnabwt_writer_t;

typedef struct dnabwt_reader_cache {
    size_t block_index;
    size_t length;
    uint64_t stamp;
    uint8_t *data;
} dnabwt_reader_cache_t;

typedef struct dnabwt_reader {
    void *fp;
    size_t file_size;
    size_t block_size;
    size_t cache_count;
    uint64_t clock;
    dnabwt_reader_cache_t *caches;
    dnabwt_status_t status;
} dnabwt_reader_t;

dnabwt_status_t dnabwt_writer_open(dnabwt_writer_t **writer,
                                   const char *path,
                                   dnabwt_writer_mode_t mode,
                                   size_t reserve_size,
                                   size_t buffer_size);
dnabwt_status_t dnabwt_writer_write(dnabwt_writer_t *writer, const uint8_t *data, size_t length);
dnabwt_status_t dnabwt_writer_flush(dnabwt_writer_t *writer);
void dnabwt_writer_close(dnabwt_writer_t *writer);

dnabwt_status_t dnabwt_reader_open(dnabwt_reader_t **reader,
                                   const char *path,
                                   size_t block_size,
                                   size_t cache_count);
dnabwt_status_t dnabwt_reader_read_at(dnabwt_reader_t *reader, size_t offset, uint8_t *out, size_t length);
void dnabwt_reader_close(dnabwt_reader_t *reader);

#ifdef __cplusplus
}
#endif

#endif
