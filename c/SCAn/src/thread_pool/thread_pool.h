#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>

#include "task_queue.h"

#define INFO   "[\033[32mINFO.\033[0m]"
#define DEBUG  "[\033[36mDEBUG\033[0m]"
#define ERR    "[\033[31mERR..\033[0m]"

typedef unsigned int busy_threads_t;

typedef enum {
    CREATED,
    MAX_THREAD_AMOUNT_EXCEEDED,
    OUT_OF_MEMORY,
    THREAD_CREATION_FAILED,
} thread_pool_creation_status_t;

typedef struct worker_pool_t {
    task_queue_t *task_queue;
    pthread_cond_t *c_task_queue;
    pthread_mutex_t *m_task_queue;

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
    unsigned short thread_count;
} thread_pool_t;

thread_pool_t *create_thread_pool(unsigned short thread_count, thread_pool_creation_status_t *status);
void free_pool(thread_pool_t *pool);

#endif