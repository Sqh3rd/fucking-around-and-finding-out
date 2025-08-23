#include <stdlib.h>
#include <stdio.h>
#include "task_queue.h"

task_queue_t *create_task_queue();
int destroy_task_queue(task_queue_t *queue);
int enqueue_task(task_queue_t *queue, int(*task_func)(task_queue_entry_arg_t *), task_queue_entry_arg_t *arg);
int dequeue_task(task_queue_t *queue, int(* *task_func)(task_queue_entry_arg_t *), task_queue_entry_arg_t **arg);

task_queue_t *create_task_queue() {
    task_queue_t *queue = malloc(sizeof(task_queue_t));
    if (!queue) {
        return NULL;
    }
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    return queue;
}

int destroy_task_queue(task_queue_t *queue) {
    if (!queue) {
        return ILLEGAL_ARGS;
    }
    task_queue_entry_t *current = queue->head;
    while (current != NULL) {
        task_queue_entry_t *next = current->next;
        free(current);
        current = next;
    }
    free(queue);
    return 0;
}

int enqueue_task(task_queue_t *queue, int(*task_func)(task_queue_entry_arg_t *), task_queue_entry_arg_t *arg) {
    if (!queue || !task_func || !arg)
        return ILLEGAL_ARGS;
    
    task_queue_entry_t *ent = malloc(sizeof(task_queue_entry_t));
    if (!ent) {
        return MEMORY_ERROR;
    }

    ent->arg = arg;
    ent->task_func = task_func;
    ent->next = NULL;

    if (!queue->head) {
        ent->id = 0;
        queue->head = ent;
        queue->tail = ent;
    } else {
        ent->id = queue->tail->id + 1;
        queue->tail->next = ent;
        queue->tail = ent;
    }
    queue->count++;
    return 0;
}

int dequeue_task(task_queue_t *queue, int(* *task_func)(task_queue_entry_arg_t *), task_queue_entry_arg_t **arg) {
    if (!queue || !task_func || !arg)
        return ILLEGAL_ARGS;
    if (queue->count == 0)
        return QUEUE_EMPTY;

    task_queue_entry_t *head = queue->head;
    *task_func = head->task_func;
    *arg = head->arg;

    queue->head = head->next;
    free(head);
    if (!queue->head) {
        queue->tail = NULL;
    }
    queue->count--;
    
    return 0;
}