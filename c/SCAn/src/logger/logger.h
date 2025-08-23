#ifndef LOGGING_QUEUE_H
#define LOGGING_QUEUE_H

#include <pthread.h>
#include <stdarg.h>

#define __LOG_PREFIX_LENGTH__ 17
#define ILLEGAL_FORMAT_SPECIFIER -5
#define __LOG_BUFFER_SIZE__ 500

#define INFO  "[\033[32m INFO\033[0m]"
#define DEBUG "[\033[36mDEBUG\033[0m]"
#define WARN  "[\033[36m WARN\033[0m]"
#define ERR   "[\033[31m  ERR\033[0m]"


typedef struct logging_queue_entry_t logging_queue_entry_t;

struct logging_queue_entry_t {
    char buff[__LOG_BUFFER_SIZE__];
    logging_queue_entry_t *next;
};

typedef struct logging_queue_t {
    pthread_mutex_t *m_lock;
    pthread_cond_t *c_updated;

    logging_queue_entry_t *head;
    logging_queue_entry_t *tail;
    unsigned int count;
} logging_queue_t;

logging_queue_t *create_logging_queue();
int destroy_logging_queue(logging_queue_t *queue);

int log_debug(logging_queue_t *queue, const char *message, ...);
int log_info(logging_queue_t *queue, const char *message, ...);
int log_warning(logging_queue_t *queue, const char *message, ...);
int log_error(logging_queue_t *queue, const char *message, ...);

int dequeue_log(logging_queue_t *queue, logging_queue_entry_t **entry);

#endif