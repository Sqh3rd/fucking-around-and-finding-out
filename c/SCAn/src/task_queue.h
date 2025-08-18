#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#define ILLEGAL_ARGS -1;
#define MEMORY_ERROR -2;
#define QUEUE_EMPTY  -3;

#include <pthread.h>

typedef struct task_entry_t task_entry_t;

typedef struct task_entry_arg_t {
    pthread_t id;
    void *arg;
} task_entry_arg_t;

typedef struct task_entry_t
{
    int (*task_func)(task_entry_arg_t *);
    task_entry_arg_t *arg;
    task_entry_t *next;
    unsigned int id;
} task_entry_t;

typedef struct task_queue_t
{
    task_entry_t *head;
    task_entry_t *tail;
    unsigned int count;
} task_queue_t;

task_queue_t *create_task_queue();
int destroy_task_queue(task_queue_t *queue);
int enqueue_task(task_queue_t *queue, int(*task_func)(task_entry_arg_t *), task_entry_arg_t *arg);
int dequeue_task(task_queue_t *queue, int(* *task_func)(task_entry_arg_t *), task_entry_arg_t **arg);

#endif