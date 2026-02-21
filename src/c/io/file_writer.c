#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io.h"
#include "../util/util.h"

static dnabwt_status_t dnabwt_writer_prepare_reverse(dnabwt_writer_t *writer) {
    FILE *fp;

    if (writer->total_size == 0) {
        return DNABWT_STATUS_OK;
    }

    fp = (FILE *)writer->fp;
    if (fseek(fp, (long)(writer->total_size - 1u), SEEK_SET) != 0) {
        return DNABWT_STATUS_IO_ERROR;
    }
    if (fputc(0, fp) == EOF) {
        return DNABWT_STATUS_IO_ERROR;
    }
    writer->next_offset = writer->total_size;
    return DNABWT_STATUS_OK;
}

dnabwt_status_t dnabwt_writer_open(dnabwt_writer_t **writer,
                                   const char *path,
                                   dnabwt_writer_mode_t mode,
                                   size_t reserve_size,
                                   size_t buffer_size) {
    dnabwt_writer_t *w;
    FILE *fp;

    if (writer == NULL || path == NULL) {
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }
    *writer = NULL;

    fp = fopen(path, "wb+");
    if (fp == NULL) {
        return DNABWT_STATUS_IO_ERROR;
    }

    w = (dnabwt_writer_t *)dnabwt_calloc(1u, sizeof(*w));
    if (w == NULL) {
        fclose(fp);
        return DNABWT_STATUS_NO_MEMORY;
    }

    w->capacity = buffer_size == 0 ? (1u << 16) : buffer_size;
    w->buffer = (uint8_t *)dnabwt_malloc(w->capacity);
    if (w->buffer == NULL) {
        fclose(fp);
        dnabwt_free(w);
        return DNABWT_STATUS_NO_MEMORY;
    }

    w->fp = fp;
    w->mode = mode;
    w->total_size = reserve_size;
    w->status = DNABWT_STATUS_OK;

    if (mode == DNABWT_WRITER_REVERSE) {
        w->status = dnabwt_writer_prepare_reverse(w);
        if (w->status != DNABWT_STATUS_OK) {
            dnabwt_writer_close(w);
            return w->status;
        }
    }

    *writer = w;
    return DNABWT_STATUS_OK;
}

dnabwt_status_t dnabwt_writer_flush(dnabwt_writer_t *writer) {
    FILE *fp;

    if (writer == NULL || writer->status != DNABWT_STATUS_OK) {
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }

    if (writer->length == 0) {
        return DNABWT_STATUS_OK;
    }

    fp = (FILE *)writer->fp;
    if (writer->mode == DNABWT_WRITER_FORWARD) {
        if (fwrite(writer->buffer, 1u, writer->length, fp) != writer->length) {
            writer->status = DNABWT_STATUS_IO_ERROR;
            return writer->status;
        }
    } else {
        size_t start = writer->next_offset - writer->length;
        size_t i;
        if (fseek(fp, (long)start, SEEK_SET) != 0) {
            writer->status = DNABWT_STATUS_IO_ERROR;
            return writer->status;
        }
        for (i = 0; i < writer->length; ++i) {
            if (fputc((int)writer->buffer[writer->length - 1u - i], fp) == EOF) {
                writer->status = DNABWT_STATUS_IO_ERROR;
                return writer->status;
            }
        }
        writer->next_offset = start;
    }

    writer->length = 0;
    return DNABWT_STATUS_OK;
}

dnabwt_status_t dnabwt_writer_write(dnabwt_writer_t *writer, const uint8_t *data, size_t length) {
    size_t copied = 0;

    if (writer == NULL || data == NULL) {
        return DNABWT_STATUS_INVALID_ARGUMENT;
    }

    while (copied < length) {
        size_t room = writer->capacity - writer->length;
        size_t chunk = length - copied;
        if (room == 0) {
            dnabwt_status_t status = dnabwt_writer_flush(writer);
            if (status != DNABWT_STATUS_OK) {
                return status;
            }
            room = writer->capacity;
        }
        if (chunk > room) {
            chunk = room;
        }
        memcpy(writer->buffer + writer->length, data + copied, chunk);
        writer->length += chunk;
        copied += chunk;
    }

    return DNABWT_STATUS_OK;
}

void dnabwt_writer_close(dnabwt_writer_t *writer) {
    FILE *fp;

    if (writer == NULL) {
        return;
    }

    if (writer->status == DNABWT_STATUS_OK) {
        dnabwt_writer_flush(writer);
    }

    fp = (FILE *)writer->fp;
    if (fp != NULL) {
        fclose(fp);
    }
    dnabwt_free(writer->buffer);
    dnabwt_free(writer);
}
