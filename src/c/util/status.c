#include "status.h"

const char *dnabwt_status_message(dnabwt_status_t status) {
    switch (status) {
        case DNABWT_STATUS_OK:
            return "ok";
        case DNABWT_STATUS_INVALID_ARGUMENT:
            return "invalid argument";
        case DNABWT_STATUS_IO_ERROR:
            return "io error";
        case DNABWT_STATUS_NO_MEMORY:
            return "out of memory";
        case DNABWT_STATUS_INTERRUPTED:
            return "interrupted";
        case DNABWT_STATUS_NOT_FOUND:
            return "not found";
        case DNABWT_STATUS_INTERNAL_ERROR:
        default:
            return "internal error";
    }
}
