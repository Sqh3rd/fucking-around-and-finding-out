#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>

#include "task_queue.h"
#include "logger.h"

typedef unsigned int busy_threads_t;

typedef enum {
    CREATED,
    MAX_THREAD_AMOUNT_EXCEEDED,
    OUT_OF_MEMORY,
    THREAD_CREATION_FAILED,
} thread_pool_creation_status_t;

typedef struct worker_pool_t {
    task_queue_t *task_queue;

    busy_threads_t *busy_threads;
    pthread_cond_t *c_busy_threads;
    pthread_mutex_t *m_busy_threads;

    pthread_t **worker_threads;
    unsigned short worker_count;
} worker_pool_t;

typedef struct thread_pool_t {
    worker_pool_t *worker_pool;

    pthread_t *watcher_thread;
    pthread_t *logger_thread;

    logging_queue_t *logging_queue;
    unsigned short thread_count;
} thread_pool_t;

thread_pool_t *create_thread_pool(unsigned short thread_count, logging_queue_t *logging_queue, thread_pool_creation_status_t *status);
void free_pool(thread_pool_t *pool);

#endif