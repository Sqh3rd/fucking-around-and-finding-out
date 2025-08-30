#ifndef TASK_QUEUE_PUBLIC_H
#define TASK_QUEUE_PUBLIC_H

#include <pthread.h>

typedef struct task_queue_entry_t task_queue_entry_t;
typedef struct task_queue_t task_queue_t;

typedef struct task_queue_entry_arg_t {
    pthread_t id;
    void *arg;
} task_queue_entry_arg_t;

#endif
