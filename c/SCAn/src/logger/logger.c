#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "logger.h"
#include "constants.h"

logging_queue_t *create_logging_queue();
int destroy_logging_queue(logging_queue_t *queue);

int log_debug(logging_queue_t *queue, const char *message, ...);
int log_info(logging_queue_t *queue, const char *message, ...);
int log_warning(logging_queue_t *queue, const char *message, ...);
int log_error(logging_queue_t *queue, const char *message, ...);

int dequeue_log(logging_queue_t *queue, logging_queue_entry_t **entry);

logging_queue_t *create_logging_queue()
{
    logging_queue_t *queue = malloc(sizeof(logging_queue_t));
    if (!queue)
    {
        return NULL;
    }
    queue->m_lock = malloc(sizeof(pthread_mutex_t));
    queue->c_updated = malloc(sizeof(pthread_cond_t));
    if (pthread_mutex_init(queue->m_lock, NULL) ||
        pthread_cond_init(queue->c_updated, NULL))
    {
        free(queue->m_lock);
        free(queue->c_updated);
        free(queue);
        return NULL;
    }
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    return queue;
}

int destroy_logging_queue(logging_queue_t *queue)
{
    if (!queue)
    {
        return ILLEGAL_ARGS;
    }
    logging_queue_entry_t *current = queue->head;
    while (current != NULL)
    {
        logging_queue_entry_t *next = current->next;
        free(current->buff);
        free(current);
        current = next;
    }
    pthread_cond_destroy(queue->c_updated);
    pthread_mutex_destroy(queue->m_lock);
    free(queue->m_lock);
    free(queue->c_updated);
    free(queue);
    return 0;
}

int enqueue_log(logging_queue_t *queue, const char *level, const char *message, va_list args)
{
    if (!queue || !level || !message)
    {
        return ILLEGAL_ARGS;
    }

    logging_queue_entry_t *ent = malloc(sizeof(logging_queue_entry_t));
    if (!ent)
    {
        return MEMORY_ERROR;
    }

    int len = snprintf(ent->buff, sizeof(ent->buff), "%s ", level);
    if (len < 0 || len >= sizeof(ent->buff))
    {
        free(ent);
        return ILLEGAL_FORMAT_SPECIFIER;
    }
    vsnprintf(ent->buff + len, sizeof(ent->buff) - len, message, args);

    ent->next = NULL;

    if (!queue->head)
    {
        queue->head = ent;
        queue->tail = ent;
    }
    else
    {
        queue->tail->next = ent;
        queue->tail = ent;
    }
    queue->count++;
    return 0;
}

int enqueue_log_locked(logging_queue_t *queue, const char *level, const char *message, va_list args)
{
    if (!queue || !level || !message)
    {
        return ILLEGAL_ARGS;
    }
    pthread_mutex_lock(queue->m_lock);
    int result = enqueue_log(queue, level, message, args);
    pthread_cond_broadcast(queue->c_updated);
    pthread_mutex_unlock(queue->m_lock);
    return result;
}

int log_debug(logging_queue_t *queue, const char *message, ...)
{
    va_list args;
    va_start(args, message);
    int result = enqueue_log_locked(queue, DEBUG, message, args);
    va_end(args);
    return result;
};

int log_info(logging_queue_t *queue, const char *message, ...)
{
    va_list args;
    va_start(args, message);
    int result = enqueue_log_locked(queue, INFO, message, args);
    va_end(args);
    return result;
};

int log_warning(logging_queue_t *queue, const char *message, ...)
{
    va_list args;
    va_start(args, message);
    int result = enqueue_log_locked(queue, WARN, message, args);
    va_end(args);
    return result;
};

int log_error(logging_queue_t *queue, const char *message, ...)
{
    va_list args;
    va_start(args, message);
    int result = enqueue_log_locked(queue, ERR, message, args);
    va_end(args);
    return result;
};

int dequeue_log(logging_queue_t *queue, logging_queue_entry_t **entry)
{
    if (!queue || !entry)
    {
        return ILLEGAL_ARGS;
    }
    pthread_mutex_lock(queue->m_lock);
    if (queue->count == 0)
    {
        pthread_mutex_unlock(queue->m_lock);
        return QUEUE_EMPTY;
    }

    *entry = queue->head;

    queue->head = queue->head->next;
    if (!queue->head)
    {
        queue->tail = NULL;
    }
    queue->count--;

    pthread_mutex_unlock(queue->m_lock);
    return 0;
}