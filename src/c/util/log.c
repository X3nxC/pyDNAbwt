#include <stdarg.h>
#include <stdio.h>

#include "util.h"

static dnabwt_log_level_t g_level = DNABWT_LOG_WARN;
static dnabwt_log_sink_fn g_sink = NULL;
static void *g_sink_user_data = NULL;

void dnabwt_log_set_level(dnabwt_log_level_t level) {
    g_level = level;
}

void dnabwt_log_set_sink(dnabwt_log_sink_fn sink, void *user_data) {
    g_sink = sink;
    g_sink_user_data = user_data;
}

void dnabwt_vlog(dnabwt_log_level_t level, const char *fmt, va_list args) {
    char message[1024];
    const char *prefix = "WARN";
    int n;

    if (fmt == NULL || level > g_level) {
        return;
    }

    n = vsnprintf(message, sizeof(message), fmt, args);
    if (n < 0) {
        return;
    }

    switch (level) {
        case DNABWT_LOG_ERROR:
            prefix = "ERROR";
            break;
        case DNABWT_LOG_WARN:
            prefix = "WARN";
            break;
        case DNABWT_LOG_INFO:
            prefix = "INFO";
            break;
        case DNABWT_LOG_DEBUG:
            prefix = "DEBUG";
            break;
    }

    if (g_sink != NULL) {
        g_sink(g_sink_user_data, level, message);
        return;
    }

    fprintf(stderr, "[dnabwt:%s] %s\n", prefix, message);
}

void dnabwt_log(dnabwt_log_level_t level, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    dnabwt_vlog(level, fmt, args);
    va_end(args);
}
