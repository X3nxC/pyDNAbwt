#include "util.h"

static dnabwt_progress_cb g_progress_cb = NULL;
static void *g_progress_user_data = NULL;

void dnabwt_progress_set_callback(dnabwt_progress_cb cb, void *user_data) {
    g_progress_cb = cb;
    g_progress_user_data = user_data;
}

int dnabwt_progress_report(uint64_t processed_chars, uint64_t total_chars) {
    if (g_progress_cb == NULL) {
        return 0;
    }
    return g_progress_cb(g_progress_user_data, processed_chars, total_chars);
}
