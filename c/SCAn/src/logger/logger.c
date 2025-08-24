#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logger.h"
#include "constants.h"

#define __LOG_BUFFER_SIZE__ 500

const char DEBUG[]  = "[\033[36mDEBUG\033[0m]";
const char INFO[]   = "[\033[32m INFO\033[0m]";
const char WARN[]   = "[\033[36m WARN\033[0m]";
const char ERR[]    = "[\033[31m  ERR\033[0m]";

pthread_mutex_t log_init_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct logger_t logger_t;

typedef struct logging_queue_entry_t logging_queue_entry_t;

struct logging_queue_entry_t
{
    char buff[__LOG_BUFFER_SIZE__];
    logging_queue_entry_t *next;
};

typedef struct logging_queue_t
{
    pthread_mutex_t *m_lock;
    pthread_cond_t *c_updated;

    logging_queue_entry_t *head;
    logging_queue_entry_t *tail;

    unsigned int count;
    unsigned short is_stopped;

    log_level_enum_t log_level;
} logging_queue_t;

typedef struct logger_t
{
    logging_queue_t *queue;
    pthread_t *thread;
} logger_t;

logger_t *logger = NULL;

logger_t *init_logger(const log_level_enum_t log_level);
logger_t *get_logger();
int destroy_logger();
logging_queue_t *create_logging_queue(const log_level_enum_t log_level);
int destroy_logging_queue(logging_queue_t *queue);

int log_debug(const char *message, ...);
int log_info(const char *message, ...);
int log_warning(const char *message, ...);
int log_error(const char *message, ...);

int dequeue_log_and_print(logging_queue_t *queue);

void *log_messages(void *arg)
{
    logging_queue_t *logging_queue = (logging_queue_t *)arg;
    unsigned short should_exit = 0;
    while (!should_exit)
    {
        pthread_mutex_lock(logging_queue->m_lock);
        while (!(should_exit = logging_queue->is_stopped) || logging_queue->count == 0)
        {
            pthread_cond_wait(logging_queue->c_updated, logging_queue->m_lock);
        }
        pthread_mutex_unlock(logging_queue->m_lock);

        int result;
        while ((result = dequeue_log_and_print(logging_queue)) == 0)
            ;
        if (result != QUEUE_EMPTY)
        {
            printf("%s Failed to dequeue log entry: %d\n", ERR, result);
        }
    }
    printf("%s Logger Thread exiting...\n", INFO);
}

logger_t *init_logger(const log_level_enum_t log_level)
{
    printf("%s Initializing Logger...\n", INFO);
    pthread_mutex_lock(&log_init_lock);
    if (logger) {
        printf("%s Logger already initialized...\n", INFO);
        pthread_mutex_unlock(&log_init_lock);
        return logger;
    }

    logger = malloc(sizeof(logger_t));

    if (!logger)
    {
        printf("%s Logger initialization failed...\n", ERR);
        pthread_mutex_unlock(&log_init_lock);
        return NULL;
    }

    logger->queue = create_logging_queue(log_level);

    if (!logger->queue)
    {
        printf("%s Logger queue initialization failed...\n", ERR);
        free(logger);
        pthread_mutex_unlock(&log_init_lock);
        return NULL;
    }

    logger->thread = malloc(sizeof(pthread_t));

    if (!logger->thread || pthread_create(logger->thread, NULL, log_messages, logger->queue))
    {
        printf("%s Logger thread initialization failed...\n", ERR);
        if (logger->thread)
            free(logger->thread);
        destroy_logging_queue(logger->queue);
        free(logger);
        pthread_mutex_unlock(&log_init_lock);
        return NULL;
    }
    pthread_mutex_unlock(&log_init_lock);
    printf("%s Logger initialized...\n", INFO);
    return logger;
}

logger_t *get_logger()
{
    if (!logger)
    {
        return init_logger(LOG_LEVEL_INFO);
    }
    return logger;
}

int destroy_logger()
{
    if (!logger)
    {
        return 0;
    }
    logging_queue_t *queue = logger->queue;
    if (!queue)
    {
        free(logger);
        logger = NULL;
        return 0;
    }
    pthread_mutex_lock(queue->m_lock);
    queue->is_stopped = 1;
    pthread_cond_broadcast(queue->c_updated);
    pthread_mutex_unlock(queue->m_lock);

    pthread_join(*(logger->thread), NULL);

    free(logger->thread);
    destroy_logging_queue(logger->queue);
    free(logger);
    logger = NULL;
    return 0;
}

logging_queue_t *create_logging_queue(const log_level_enum_t log_level)
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
    queue->log_level = log_level;
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

int enqueue_log(logging_queue_t *queue, const log_level_enum_t level, const char *message, ...) {
    va_list args;
    va_start(args, message);
    int result = enqueue_log(queue, level, message, args);
    va_end(args);
    return result;
}

int v_enqueue_log(logging_queue_t *queue, const log_level_enum_t level, const char *message, va_list args)
{
    if (!queue || !message)
    {
        return ILLEGAL_ARGS;
    }

    logging_queue_entry_t *ent = malloc(sizeof(logging_queue_entry_t));
    if (!ent)
    {
        return MEMORY_ERROR;
    }

    const char *level_str;
    switch (level)
    {
    case LOG_LEVEL_DEBUG:
        level_str = DEBUG;
        break;
    case LOG_LEVEL_INFO:
        level_str = INFO;
        break;
    case LOG_LEVEL_WARN:
        level_str = WARN;
        break;
    case LOG_LEVEL_ERROR:
        level_str = ERR;
        break;
    default:
        free(ent);
        return ILLEGAL_ARGS;
    }

    int len = snprintf(ent->buff, sizeof(ent->buff), "%s ", level_str);
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

int enqueue_log_locked(const log_level_enum_t level, const char *message, va_list args)
{
    if (!level || !message)
    {
        return ILLEGAL_ARGS;
    }
    logger_t *logger = get_logger();
    if (!logger || !logger->queue)
    {
        return LOGGER_MISSING;
    }
    logging_queue_t *queue = logger->queue;
    if (level < queue->log_level)
    {
        return 0;
    }
    pthread_mutex_lock(queue->m_lock);
    int result = v_enqueue_log(queue, level, message, args);
    if (result != 0)
    {
        enqueue_log(queue, LOG_LEVEL_ERROR, "Failed to enqueue log entry with error: %d\n", result);
    }
    pthread_cond_broadcast(queue->c_updated);
    pthread_mutex_unlock(queue->m_lock);
    return result;
}

int log_debug(const char *message, ...)
{

    va_list args;
    va_start(args, message);
    int result = enqueue_log_locked(LOG_LEVEL_DEBUG, message, args);
    va_end(args);
    return result;
};

int log_info(const char *message, ...)
{
    va_list args;
    va_start(args, message);
    int result = enqueue_log_locked(LOG_LEVEL_INFO, message, args);
    va_end(args);
    return result;
};

int log_warning(const char *message, ...)
{
    va_list args;
    va_start(args, message);
    int result = enqueue_log_locked(LOG_LEVEL_WARN, message, args);
    va_end(args);
    return result;
};

int log_error(const char *message, ...)
{
    va_list args;
    va_start(args, message);
    int result = enqueue_log_locked(LOG_LEVEL_ERROR, message, args);
    va_end(args);
    return result;
};

int dequeue_log_and_print(logging_queue_t *queue)
{
    if (!queue)
    {
        return ILLEGAL_ARGS;
    }
    pthread_mutex_lock(queue->m_lock);
    if (queue->count == 0)
    {
        pthread_mutex_unlock(queue->m_lock);
        return QUEUE_EMPTY;
    }

    logging_queue_entry_t *entry = queue->head;
    queue->head = queue->head->next;
    if (!queue->head)
    {
        queue->tail = NULL;
    }
    queue->count--;

    pthread_mutex_unlock(queue->m_lock);
    printf(entry->buff);
    free(entry);
    return 0;
}