#include <stdlib.h>
#include <stdio.h>
#include "task_queue.h"

task_queue_t *_create_task_queue();
int _stop_task_queue(task_queue_t *queue);
int _destroy_task_queue(task_queue_t *queue);
int _enqueue_task(task_queue_t *queue, int (*task_func)(task_queue_entry_arg_t *), task_queue_entry_arg_t *arg);
int _dequeue_task(task_queue_t *queue, int (**task_func)(task_queue_entry_arg_t *), task_queue_entry_arg_t **arg);

int stop_task_queue_locked(task_queue_t *queue);
int _enqueue_task_locked(task_queue_t *queue, int (*task_func)(task_queue_entry_arg_t *), task_queue_entry_arg_t *arg);
int _dequeue_task_locked(task_queue_t *queue, int (**task_func)(task_queue_entry_arg_t *), task_queue_entry_arg_t **arg);

task_queue_t *_create_task_queue()
{
    task_queue_t *queue = malloc(sizeof(task_queue_t));
    if (!queue)
    {
        return NULL;
    }
    queue->c_updated = malloc(sizeof(pthread_cond_t));
    queue->m_lock = malloc(sizeof(pthread_mutex_t));
    if (pthread_cond_init(queue->c_updated, NULL) ||
        pthread_mutex_init(queue->m_lock, NULL))
    {
        free(queue->c_updated);
        free(queue->m_lock);
        free(queue);
        return NULL;
    }
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    queue->is_started = 0;
    queue->is_stopped = 0;
    return queue;
}

int _stop_task_queue(task_queue_t *queue)
{
    if (!queue)
    {
        return ILLEGAL_ARGS;
    }
    queue->is_stopped = 1;
    return 0;
}

int _destroy_task_queue(task_queue_t *queue)
{
    if (!queue)
    {
        return ILLEGAL_ARGS;
    }
    task_queue_entry_t *current = queue->head;
    while (current != NULL)
    {
        task_queue_entry_t *next = current->next;
        free(current);
        current = next;
    }

    pthread_cond_destroy(queue->c_updated);
    pthread_mutex_destroy(queue->m_lock);

    free(queue);
    return 0;
}

int _enqueue_task(task_queue_t *queue, int (*task_func)(task_queue_entry_arg_t *), task_queue_entry_arg_t *arg)
{
    if (!queue || !task_func || !arg)
        return ILLEGAL_ARGS;

    if (queue->is_stopped)
    {
        return QUEUE_STOPPED;
    }

    task_queue_entry_t *ent = malloc(sizeof(task_queue_entry_t));
    if (!ent)
    {
        return MEMORY_ERROR;
    }

    ent->arg = arg;
    ent->task_func = task_func;
    ent->next = NULL;

    if (!queue->is_started)
        queue->is_started = 1;
    if (!queue->head)
    {
        ent->id = 0;
        queue->head = ent;
        queue->tail = ent;
    }
    else
    {
        ent->id = queue->tail->id + 1;
        queue->tail->next = ent;
        queue->tail = ent;
    }
    queue->count++;
    return 0;
}

int _dequeue_task(task_queue_t *queue, int (**task_func)(task_queue_entry_arg_t *), task_queue_entry_arg_t **arg)
{
    if (!queue || !task_func || !arg)
        return ILLEGAL_ARGS;
    if (queue->count == 0 || queue->is_stopped)
    {
        return (queue->count == 0) ? QUEUE_EMPTY : QUEUE_STOPPED;
    }

    task_queue_entry_t *head = queue->head;
    *task_func = head->task_func;
    *arg = head->arg;

    queue->head = head->next;
    free(head);
    if (!queue->head)
    {
        queue->tail = NULL;
    }
    queue->count--;
    return 0;
}

int stop_task_queue_locked(task_queue_t *queue)
{
    if (!queue)
    {
        return ILLEGAL_ARGS;
    }
    pthread_mutex_lock(queue->m_lock);
    int err = _stop_task_queue(queue);
    if (err == 0)
    {
        pthread_cond_broadcast(queue->c_updated);
    }
    pthread_mutex_unlock(queue->m_lock);
    return err;
}

int _enqueue_task_locked(task_queue_t *queue, int (*task_func)(task_queue_entry_arg_t *), task_queue_entry_arg_t *arg)
{
    if (!queue)
        return ILLEGAL_ARGS;

    pthread_mutex_lock(queue->m_lock);
    int err = _enqueue_task(queue, task_func, arg);
    if (err == 0)
    {
        pthread_cond_broadcast(queue->c_updated);
    }
    pthread_mutex_unlock(queue->m_lock);
    return err;
}

int _dequeue_task_locked(task_queue_t *queue, int (**task_func)(task_queue_entry_arg_t *), task_queue_entry_arg_t **arg)
{
    if (!queue)
        return ILLEGAL_ARGS;

    pthread_mutex_lock(queue->m_lock);
    int err = _dequeue_task(queue, task_func, arg);
    if (err == 0)
    {
        pthread_cond_broadcast(queue->c_updated);
    }
    pthread_mutex_unlock(queue->m_lock);
    return err;
}