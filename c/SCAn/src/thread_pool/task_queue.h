#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include <pthread.h>

#include "constants.h"

typedef struct task_queue_entry_t task_queue_entry_t;

typedef struct task_queue_entry_arg_t {
    pthread_t id;
    void *arg;
} task_queue_entry_arg_t;

typedef struct task_queue_entry_t
{
    int (*task_func)(task_queue_entry_arg_t *);
    task_queue_entry_arg_t *arg;
    task_queue_entry_t *next;
    unsigned int id;
} task_queue_entry_t;

typedef struct task_queue_t
{
    pthread_mutex_t *m_lock;
    pthread_cond_t *c_updated;
    task_queue_entry_t *head;
    task_queue_entry_t *tail;
    unsigned int count;
    unsigned short is_started;
    unsigned short is_stopped;
} task_queue_t;

task_queue_t *create_task_queue();
int stop_task_queue(task_queue_t *queue);
int destroy_task_queue(task_queue_t *queue);
int enqueue_task(task_queue_t *queue, int(*task_func)(task_queue_entry_arg_t *), task_queue_entry_arg_t *arg);
int dequeue_task(task_queue_t *queue, int(* *task_func)(task_queue_entry_arg_t *), task_queue_entry_arg_t **arg);

int enqueue_task_locked(task_queue_t *queue, int(*task_func)(task_queue_entry_arg_t *), task_queue_entry_arg_t *arg);
int dequeue_task_locked(task_queue_t *queue, int(* *task_func)(task_queue_entry_arg_t *), task_queue_entry_arg_t **arg);

#endif