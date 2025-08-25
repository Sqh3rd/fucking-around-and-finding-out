#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "logger.h"
#include "constants.h"

#define __LOG_BUFFER_SIZE__ 1024

const char DATE_TIME_FORMAT[] = "0000-00-00T00:00:00.000";
const char THREAD_FORMAT[] = "[THREAD-XXXX]";

const char ANSI_RESET[] = "\033[0m";
const char ANSI_COLOR_GREY[] = "\033[90m";

const char DEBUG[] = "[\033[36mDEBUG\033[0m]";
const char INFO[] = "[\033[32m INFO\033[0m]";
const char WARN[] = "[\033[36m WARN\033[0m]";
const char ERR[] = "[\033[31m  ERR\033[0m]";

const size_t PREFIX_SIZE = sizeof(ANSI_COLOR_GREY) + sizeof(DATE_TIME_FORMAT) + sizeof(THREAD_FORMAT) + sizeof(ANSI_RESET) + sizeof(DEBUG) - 1;

pthread_mutex_t log_init_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct logger_t logger_t;

typedef struct logging_queue_entry_t logging_queue_entry_t;

struct logging_queue_entry_t
{
    char *buff;
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

    char *cur_buff;
    size_t cur_buff_last_entry;
} logging_queue_t;

typedef struct logger_t
{
    logging_queue_t *queue;
    pthread_t *thread;
    log_level_enum_t log_level;
} logger_t;

logger_t *logger = NULL;

logger_t *init_logger(const log_level_enum_t log_level);
logger_t *get_logger();
int destroy_logger();
logging_queue_t *create_logging_queue();
int destroy_logging_queue(logging_queue_t *queue);

int log_debug(const char *message, ...);
int log_info(const char *message, ...);
int log_warning(const char *message, ...);
int log_error(const char *message, ...);

int flush_buffer(logging_queue_t *queue);
int pprint(log_level_enum_t log_level, const char *message);
int dequeue_log_and_print(logging_queue_t *queue);

void *log_messages(void *arg)
{
    logging_queue_t *logging_queue = (logging_queue_t *)arg;
    unsigned short should_exit = 0;
    struct timespec *wait_time = malloc(sizeof(struct timespec));
    wait_time->tv_sec = 1;
    while (!should_exit)
    {
        pthread_mutex_lock(logging_queue->m_lock);
        while (!(should_exit = logging_queue->is_stopped) || logging_queue->count == 0)
        {
            pthread_cond_timedwait(logging_queue->c_updated, logging_queue->m_lock, wait_time);
        }
        pthread_mutex_unlock(logging_queue->m_lock);

        int result;
        while ((result = dequeue_log_and_print(logging_queue)) == 0)
            ;
        if (result != QUEUE_EMPTY)
        {
            pprint(LOG_LEVEL_ERROR, "Failed to dequeue log entry: ");
            printf("%d\n", result);
        }
    }
    flush_buffer(logging_queue);
    dequeue_log_and_print(logging_queue);
    pprint(LOG_LEVEL_INFO, "Logger Thread exiting...\n");
}

logger_t *init_logger(const log_level_enum_t log_level)
{
    pprint(LOG_LEVEL_INFO, "Initializing Logger with log level ");
    printf("%d...\n", log_level);
    pthread_mutex_lock(&log_init_lock);
    if (logger)
    {
        pprint(LOG_LEVEL_INFO, "Logger already initialized...\n");
        pthread_mutex_unlock(&log_init_lock);
        return logger;
    }

    logger = malloc(sizeof(logger_t));

    if (!logger)
    {
        pprint(LOG_LEVEL_ERROR, "Logger initialization failed...\n");
        pthread_mutex_unlock(&log_init_lock);
        return NULL;
    }

    logger->queue = create_logging_queue();

    if (!logger->queue)
    {
        pprint(LOG_LEVEL_ERROR, "Logger queue initialization failed...\n");
        free(logger);
        pthread_mutex_unlock(&log_init_lock);
        return NULL;
    }

    logger->thread = malloc(sizeof(pthread_t));

    if (!logger->thread || pthread_create(logger->thread, NULL, log_messages, logger->queue))
    {
        pprint(LOG_LEVEL_ERROR, "Logger thread initialization failed...\n");
        if (logger->thread)
            free(logger->thread);
        destroy_logging_queue(logger->queue);
        free(logger);
        pthread_mutex_unlock(&log_init_lock);
        return NULL;
    }
    logger->log_level = log_level;
    pthread_mutex_unlock(&log_init_lock);
    pprint(LOG_LEVEL_INFO, "Logger initialized...\n");
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

logging_queue_t *create_logging_queue()
{
    logging_queue_t *queue = malloc(sizeof(logging_queue_t));
    if (!queue)
    {
        return NULL;
    }
    queue->m_lock = malloc(sizeof(pthread_mutex_t));
    queue->c_updated = malloc(sizeof(pthread_cond_t));
    queue->cur_buff = malloc(__LOG_BUFFER_SIZE__ * sizeof(char));

    if (
        !queue->cur_buff ||
        pthread_mutex_init(queue->m_lock, NULL) ||
        pthread_cond_init(queue->c_updated, NULL))
    {
        if (queue->cur_buff)
            free(queue->cur_buff);
        if (queue->m_lock)
            free(queue->m_lock);
        if (queue->c_updated)
            free(queue->c_updated);
        free(queue);
        return NULL;
    }
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    queue->cur_buff_last_entry = 0;
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

size_t format_message_prefix(char *buffer, size_t buffer_size, log_level_enum_t log_level)
{
    if (buffer_size < PREFIX_SIZE)
        return -1;

    const char *level_str;
    switch (log_level)
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
        return ILLEGAL_ARGS;
    }

    size_t len = 0;

    len += snprintf(buffer + len, buffer_size - len, "%s", ANSI_COLOR_GREY);

    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);

    struct tm *local = localtime(&tp.tv_sec);
    len += strftime(buffer + len, buffer_size - len, "%Y-%m-%dT%H:%M:%S", local); // same as "%Y-%m-%dT%H:%M:%S"

    len += snprintf(buffer + len, buffer_size - len, ".%03ld", tp.tv_nsec / 1000000);
    len += snprintf(buffer + len, buffer_size - len, " [THREAD-%4d]", pthread_self());
    len += snprintf(buffer + len, buffer_size - len, "%s", ANSI_RESET);

    len += snprintf(buffer + len, buffer_size - len, " %s ", level_str);
    if (len < 0 || len >= buffer_size)
    {
        return ILLEGAL_FORMAT_SPECIFIER;
    }
    return len;
}

int write_to_buff(char *buffer, size_t buffer_size, const log_level_enum_t level, const char *message, va_list args) {
    if (buffer_size < PREFIX_SIZE)
        return -1;
    format_message_prefix(buffer, buffer_size, level);

}

int v_enqueue_log(logging_queue_t *queue, const log_level_enum_t level, const char *message, va_list args)
{
    if (!queue || !message)
    {
        return ILLEGAL_ARGS;
    }

    if (!queue->cur_buff)
        queue->cur_buff = malloc(__LOG_BUFFER_SIZE__ * sizeof(char));

    size_t remaining_buff = __LOG_BUFFER_SIZE__ - queue->cur_buff_last_entry;

    unsigned int should_create_new_entry = remaining_buff < PREFIX_SIZE ? 1 : 0;
    unsigned int new_text_present = 0;

    if (!should_create_new_entry) {
        new_text_present = 1;
        size_t remainder = 0;
        size_t len;
        len = format_message_prefix(queue->cur_buff + queue->cur_buff_last_entry, __LOG_BUFFER_SIZE__ - queue->cur_buff_last_entry, level);
        if (len < 0)
        {
            return ILLEGAL_ARGS;
        }
        queue->cur_buff_last_entry += len;

        remaining_buff -= len;

        len = vsnprintf(queue->cur_buff + queue->cur_buff_last_entry, remaining_buff, message, args);
        
        if (len > remaining_buff) {
            should_create_new_entry = 1;
            queue->cur_buff[__LOG_BUFFER_SIZE__ - PREFIX_SIZE - remaining_buff + 1] = '\0';
        }
    }

    if (should_create_new_entry) {
        logging_queue_entry_t *ent = malloc(sizeof(logging_queue_entry_t));
        if (!ent)
        {
            return MEMORY_ERROR;
        }

        ent->buff = queue->cur_buff;
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

        queue->cur_buff = malloc(__LOG_BUFFER_SIZE__ * sizeof(char));
        queue->cur_buff_last_entry = 0;
    }

    return 0;
}

int enqueue_log(logging_queue_t *queue, const log_level_enum_t level, const char *message, ...)
{
    va_list args;
    va_start(args, message);
    int result = v_enqueue_log(queue, level, message, args);
    va_end(args);
    return result;
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
    if (level < logger->log_level)
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
    free(entry->buff);
    free(entry);
    return 0;
}

int flush_buffer(logging_queue_t *queue)
{
    if (!queue)
    {
        return ILLEGAL_ARGS;
    }
    pthread_mutex_lock(queue->m_lock);

    if (!queue->cur_buff || !queue->cur_buff_last_entry)
    {
        pthread_mutex_unlock(queue->m_lock);
        return 0;
    }

    logging_queue_entry_t *ent = malloc(sizeof(logging_queue_entry_t));
    if (!ent)
    {
        pthread_mutex_unlock(queue->m_lock);
        return MEMORY_ERROR;
    }
    ent->buff = queue->cur_buff;
    ent->next = NULL;

    queue->cur_buff = NULL;
    queue->cur_buff_last_entry = 0;

    if (!queue->head)
    {
        queue->head = queue->tail = ent;
    }
    else
    {
        queue->tail = queue->tail->next = ent;
    }

    pthread_mutex_unlock(queue->m_lock);
    return 0;
}

int pprint(log_level_enum_t log_level, const char *message)
{
    char *buffer = malloc(PREFIX_SIZE);
    size_t len = format_message_prefix(buffer, PREFIX_SIZE, log_level);
    if (len == -1)
    {
        free(buffer);
        return -1;
    }
    printf("%s%s", buffer, message);
    free(buffer);
    return 0;
}
