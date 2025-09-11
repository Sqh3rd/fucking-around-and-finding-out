#ifndef LOGGING_QUEUE_H
#define LOGGING_QUEUE_H

#include <pthread.h>
#include <stdarg.h>

#define ILLEGAL_FORMAT_SPECIFIER    -6
#define LOGGER_MISSING              -7

typedef struct logger_t logger_t;

typedef enum log_level_enum_t
{
    LOG_LEVEL_TRACE,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
} log_level_enum_t;

logger_t *init_logger(log_level_enum_t log_level);
int stop_logger();

int log_debug(const char *message, ...);
int log_info(const char *message, ...);
int log_warning(const char *message, ...);
int log_error(const char *message, ...);

#endif