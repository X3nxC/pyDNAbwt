#ifndef DNABWT_UTIL_STATUS_H
#define DNABWT_UTIL_STATUS_H

// 错误码枚举
typedef enum dnabwt_status {
    DNABWT_STATUS_OK = 0,
    DNABWT_STATUS_INVALID_ARGUMENT = 1,
    DNABWT_STATUS_IO_ERROR = 2,
    DNABWT_STATUS_NO_MEMORY = 3,
    DNABWT_STATUS_INTERRUPTED = 4,
    DNABWT_STATUS_NOT_FOUND = 5,
    DNABWT_STATUS_INTERNAL_ERROR = 255
} dnabwt_status_t;

#ifdef __cplusplus
extern "C" {
#endif

const char *dnabwt_status_message(dnabwt_status_t status);

#ifdef __cplusplus
}
#endif

#endif
