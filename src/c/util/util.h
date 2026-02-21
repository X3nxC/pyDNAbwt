#ifndef DNABWT_UTIL_UTIL_H
#define DNABWT_UTIL_UTIL_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum dnabwt_log_level {
    DNABWT_LOG_ERROR = 0,
    DNABWT_LOG_WARN = 1,
    DNABWT_LOG_INFO = 2,
    DNABWT_LOG_DEBUG = 3
} dnabwt_log_level_t;

typedef void (*dnabwt_log_sink_fn)(void *user_data, dnabwt_log_level_t level, const char *message);
typedef int (*dnabwt_progress_cb)(void *user_data, uint64_t processed_chars, uint64_t total_chars);
typedef int (*dnabwt_signal_check_cb)(void *user_data);

void *dnabwt_malloc(size_t size);
void *dnabwt_calloc(size_t count, size_t size);
void *dnabwt_realloc(void *ptr, size_t size);
void dnabwt_free(void *ptr);
char *dnabwt_strdup(const char *s);
void dnabwt_mem_reset_peak(void);
size_t dnabwt_mem_current(void);
size_t dnabwt_mem_peak(void);

void dnabwt_log_set_level(dnabwt_log_level_t level);
void dnabwt_log_set_sink(dnabwt_log_sink_fn sink, void *user_data);
void dnabwt_log(dnabwt_log_level_t level, const char *fmt, ...);
void dnabwt_vlog(dnabwt_log_level_t level, const char *fmt, va_list args);

void dnabwt_progress_set_callback(dnabwt_progress_cb cb, void *user_data);
int dnabwt_progress_report(uint64_t processed_chars, uint64_t total_chars);

void dnabwt_signal_set_checker(dnabwt_signal_check_cb cb, void *user_data);
int dnabwt_signal_poll(void);

#ifdef __cplusplus
}
#endif

#endif
