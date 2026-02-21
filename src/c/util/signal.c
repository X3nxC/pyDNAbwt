#include "util.h"

static dnabwt_signal_check_cb g_signal_checker = NULL;
static void *g_signal_user_data = NULL;

void dnabwt_signal_set_checker(dnabwt_signal_check_cb cb, void *user_data) {
    g_signal_checker = cb;
    g_signal_user_data = user_data;
}

int dnabwt_signal_poll(void) {
    if (g_signal_checker == NULL) {
        return 0;
    }
    return g_signal_checker(g_signal_user_data);
}
