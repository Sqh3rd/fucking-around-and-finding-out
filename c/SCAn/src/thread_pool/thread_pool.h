#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>

#include "task_queue_public.h"

typedef unsigned int busy_threads_t;
typedef struct worker_pool_t worker_pool_t;
typedef struct thread_pool_t thread_pool_t;

typedef enum {
    CREATED,
    MAX_THREAD_AMOUNT_EXCEEDED,
    OUT_OF_MEMORY,
    THREAD_CREATION_FAILED,
} thread_pool_creation_status_t;

thread_pool_t *create_thread_pool(unsigned short thread_count, thread_pool_creation_status_t *status);
void free_pool(thread_pool_t *pool);
int enqueue_task(thread_pool_t *pool, int(*task_func)(task_queue_entry_arg_t *), task_queue_entry_arg_t *arg);
int join(thread_pool_t *pool);

#endif