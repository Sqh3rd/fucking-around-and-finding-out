#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "logger.h"
#include "constants.h"

#define __LOG_BUFFER_SIZE__ 350

const char DATE_TIME_FORMAT[] = "0000-00-00T00:00:00.000";
const char THREAD_FORMAT[] = "[THREAD-XXXX]";

const char ANSI_RESET[] = "\033[0m";
const char ANSI_COLOR_GREY[] = "\033[90m";

const char DEBUG[] = "[\033[36mDEBUG\033[0m]";
const char INFO[] = "[\033[32m INFO\033[0m]";
const char WARN[] = "[\033[33m WARN\033[0m]";
const char ERR[] = "[\033[31m  ERR\033[0m]";

const size_t PREFIX_SIZE = sizeof(ANSI_COLOR_GREY) + sizeof(DATE_TIME_FORMAT) + sizeof(THREAD_FORMAT) + sizeof(ANSI_RESET) + sizeof(DEBUG) - 1;

pthread_mutex_t log_init_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct logger_t logger_t;

typedef struct logging_queue_entry_t logging_queue_entry_t;

struct logging_queue_entry_t
{
    char *buff;
    log_level_enum_t level;
    logging_queue_entry_t *next;
};

typedef struct out_stream_info_t
{
    int (*put_log)(const char *buff, FILE *out_stream);
    FILE *out_stream;
    log_level_enum_t log_level;
    int flushes;
    int changes;
    long last_flush;
    long tbf;
} out_stream_info_t;

typedef struct logging_queue_t
{
    pthread_mutex_t *m_lock;
    pthread_cond_t *c_updated;

    logging_queue_entry_t *head;
    logging_queue_entry_t *tail;

    unsigned int count;
    unsigned short is_stopped;

    out_stream_info_t **out_stream_infos;
    int out_stream_count;
} logging_queue_t;

typedef struct logger_t
{
    logging_queue_t *queue;
    pthread_t *thread;
    log_level_enum_t lowest_log_level;
} logger_t;

logger_t *logger = NULL;

// --- STATS
unsigned int total_logs = 0;
unsigned int buffer_misses = 0;
double buffer_usage = 0;
// --- STATS END

logger_t *init_logger(const log_level_enum_t log_level);
int destroy_logger(logger_t *logger);
int stop_logger();
logging_queue_t *create_logging_queue();
int destroy_logging_queue(logging_queue_t *queue);

int log_debug(const char *message, ...);
int log_info(const char *message, ...);
int log_warning(const char *message, ...);
int log_error(const char *message, ...);

int pprint(log_level_enum_t log_level, const char *message);
int dequeue_log_and_put(logging_queue_t *queue);
int put_without_escape(const char *buff, FILE *out_stream);
int flush_out_streams(logging_queue_t *queue);
int close_out_streams(logging_queue_t *queue);
int stat_it_up(logging_queue_t *queue);
out_stream_info_t *create_out_stream_info(log_level_enum_t log_level, FILE *out_stream);

int destroy_out_stream_info(out_stream_info_t *info)
{
    if (!info)
        return 0;
    if (info->out_stream && info->out_stream != stdout && info->out_stream != stderr)
        fclose(info->out_stream);
    free(info);
    return 0;
}

int destroy_logging_queue(logging_queue_t *queue)
{
    if (!queue)
        return 0;
    logging_queue_entry_t *current = queue->head;
    while (current != NULL)
    {
        logging_queue_entry_t *next = current->next;
        free(current->buff);
        free(current);
        current = next;
    }
    if (queue->c_updated)
    {
        pthread_cond_destroy(queue->c_updated);
        free(queue->c_updated);
    }
    if (queue->m_lock)
    {
        pthread_mutex_destroy(queue->m_lock);
        free(queue->m_lock);
    }
    for (int i = 0; i < queue->out_stream_count; i++)
    {
        destroy_out_stream_info(queue->out_stream_infos[i]);
    }
    free(queue);
    return 0;
}

int stop_logger()
{
    return destroy_logger(logger);
}

int destroy_logger(logger_t *logger)
{
    if (!logger)
        return ILLEGAL_ARGS;
    if (logger->thread)
    {
        pthread_mutex_lock(logger->queue->m_lock);
        logger->queue->is_stopped = 1;
        pthread_cond_signal(logger->queue->c_updated);
        pthread_mutex_unlock(logger->queue->m_lock);
        pthread_join(*logger->thread, NULL);
    }
    destroy_logging_queue(logger->queue);
    free(logger);
    return 0;
}

struct timespec *rel_time(struct timespec *ts, const long milliseconds)
{
    clock_gettime(CLOCK_REALTIME, ts);

    ts->tv_sec += milliseconds / 1000;
    ts->tv_nsec += milliseconds * 1000 * 1000;

    return ts;
}

void *log_messages(void *arg)
{
    logging_queue_t *logging_queue = (logging_queue_t *)arg;
    unsigned short should_exit = 0;
    struct timespec *ts = malloc(sizeof(struct timespec));
    while (!should_exit)
    {
        pthread_mutex_lock(logging_queue->m_lock);

        do
        {
            pthread_cond_timedwait(logging_queue->c_updated, logging_queue->m_lock, rel_time(ts, 100));
            should_exit = logging_queue->is_stopped;
        } while (!should_exit && logging_queue->count == 0);

        pthread_mutex_unlock(logging_queue->m_lock);

        int result;
        do
        {
            result = dequeue_log_and_put(logging_queue);
        } while (result == 0);

        flush_out_streams(logging_queue);

        if (result != QUEUE_EMPTY)
        {
            pprint(LOG_LEVEL_ERROR, "Failed to dequeue log entry: ");
            printf("%d\n", result);
        }
    }
    free(ts);
    stat_it_up(logging_queue);
    close_out_streams(logging_queue);
}

logger_t *init_logger(const log_level_enum_t log_level)
{
    pthread_mutex_lock(&log_init_lock);
    if (logger)
    {
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

    FILE *log_file = fopen("log.txt", "w");
    if (!log_file)
    {
        pprint(LOG_LEVEL_WARN, "Failed to open log file...\n");
    }

    if (!logger->queue)
    {
        pprint(LOG_LEVEL_ERROR, "Logger queue initialization failed...\n");
        destroy_logger(logger);
        logger = NULL;
        pthread_mutex_unlock(&log_init_lock);
        return NULL;
    }

    logger->queue->out_stream_count = log_file ? 3 : 2;
    logger->queue->out_stream_infos = calloc(logger->queue->out_stream_count, sizeof(out_stream_info_t *));

    logger->queue->out_stream_infos[0] = create_out_stream_info(LOG_LEVEL_ERROR, stderr);
    logger->queue->out_stream_infos[1] = create_out_stream_info(log_level, stdout);
    if (log_file)
        logger->queue->out_stream_infos[2] = create_out_stream_info(LOG_LEVEL_DEBUG, log_file);

    logger->lowest_log_level = LOG_LEVEL_DEBUG;

    for (int i = 0; i < logger->queue->out_stream_count; i++)
    {
        if (!logger->queue->out_stream_infos[i])
        {
            pprint(LOG_LEVEL_ERROR, "Failed to create out stream info...\n");
            destroy_logger(logger);
            logger = NULL;
            pthread_mutex_unlock(&log_init_lock);
            return NULL;
        }
    }

    logger->thread = malloc(sizeof(pthread_t));
    if (!logger->thread || pthread_create(logger->thread, NULL, log_messages, logger->queue))
    {
        pprint(LOG_LEVEL_ERROR, "Logger thread initialization failed...\n");
        destroy_logger(logger);
        logger = NULL;
        pthread_mutex_unlock(&log_init_lock);
        return NULL;
    }

    pthread_mutex_unlock(&log_init_lock);
    pprint(LOG_LEVEL_INFO, "Logger initialized...\n");
    return logger;
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

    if (
        pthread_mutex_init(queue->m_lock, NULL) ||
        pthread_cond_init(queue->c_updated, NULL))
    {
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

    return queue;
}

out_stream_info_t *create_out_stream_info(log_level_enum_t log_level, FILE *out_stream)
{
    if (!out_stream)
        return NULL;

    out_stream_info_t *info = malloc(sizeof(out_stream_info_t));
    if (!info)
        return NULL;

    info->log_level = log_level;
    info->out_stream = out_stream;
    info->changes = 0;
    info->last_flush = 0;
    info->flushes = 0;
    if (isatty(fileno(out_stream)))
        info->put_log = fputs;
    else
        info->put_log = put_without_escape;

    return info;
}

size_t format_message_prefix(char *buffer, size_t buffer_size, log_level_enum_t log_level)
{
    if (buffer_size < PREFIX_SIZE)
        return PREFIX_SIZE;

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
        return FATAL_ERROR;
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

int write_to_buff(char *buffer, size_t buffer_size, const log_level_enum_t level, const char *message, va_list args)
{
    int len = format_message_prefix(buffer, buffer_size, level);
    if (len < 0)
        return len;

    int written = vsnprintf(buffer + len, buffer_size - len, message, args);
    if (written < 0)
        return written;
    return len + written;
}

logging_queue_entry_t *append_buff_to_queue(logging_queue_t *queue, log_level_enum_t level, char *buff)
{
    logging_queue_entry_t *ent = malloc(sizeof(logging_queue_entry_t));
    if (!ent)
    {
        return NULL;
    }

    ent->buff = buff;
    ent->next = NULL;
    ent->level = level;
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

    return ent;
}

int v_enqueue_log(const log_level_enum_t level, const char *message, va_list args)
{
    if (!message)
    {
        return ILLEGAL_ARGS;
    }

    logging_queue_t *queue = logger->queue;

    char *new_buff = malloc(__LOG_BUFFER_SIZE__ * sizeof(char));
    if (!new_buff)
    {
        return MEMORY_ERROR;
    }

    va_list args_copy;
    va_copy(args_copy, args);
    int len = write_to_buff(new_buff, __LOG_BUFFER_SIZE__, level, message, args_copy);
    va_end(args_copy);

    if (len < 0)
    {
        free(new_buff);
        return len;
    }
    if (len < __LOG_BUFFER_SIZE__)
    {
        total_logs++;
        buffer_usage += (double)(len + 1) / __LOG_BUFFER_SIZE__;
        append_buff_to_queue(queue, level, new_buff);
        return 0;
    }

    free(new_buff);
    new_buff = malloc((len + 1) * sizeof(char));

    if (!new_buff)
    {
        return MEMORY_ERROR;
    }

    va_copy(args_copy, args);
    len = write_to_buff(new_buff, len + 1, level, message, args_copy);
    va_end(args_copy);

    append_buff_to_queue(queue, level, new_buff);

    if (len < 0)
    {
        return FATAL_ERROR;
    }
    total_logs++;
    buffer_misses++;
    buffer_usage += (double)(len + 1) / __LOG_BUFFER_SIZE__;

    return 0;
}

int enqueue_log(const log_level_enum_t level, const char *message, ...)
{
    va_list args;
    va_start(args, message);
    int result = v_enqueue_log(level, message, args);
    va_end(args);
    return result;
}

int enqueue_log_locked(const log_level_enum_t level, const char *message, va_list args)
{
    if (!message)
    {
        return ILLEGAL_ARGS;
    }
    if (level < logger->lowest_log_level)
    {
        return 0;
    }
    pthread_mutex_lock(logger->queue->m_lock);
    int result = v_enqueue_log(level, message, args);
    if (result < 0)
        result = enqueue_log(LOG_LEVEL_ERROR, "Failed to enqueue log entry with error: %d\n", result);
    if (result < 0)
    {
        pprint(LOG_LEVEL_ERROR, "Failed to enqueue log entry with error: ");
        printf("%d\n", result);
    }
    if (result > 0)
        pthread_cond_broadcast(logger->queue->c_updated);
    pthread_mutex_unlock(logger->queue->m_lock);
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

int dequeue_log_and_put(logging_queue_t *queue)
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
    for (int i = 0; i < queue->out_stream_count; i++)
    {
        out_stream_info_t *info = queue->out_stream_infos[i];
        if (entry->level >= info->log_level)
        {
            info->put_log(entry->buff, info->out_stream);
            info->changes++;
        }
    }
    free(entry->buff);
    free(entry);
    return 0;
}

int flush_out_streams(logging_queue_t *queue)
{
    for (int i = 0; i < queue->out_stream_count; i++)
    {
        out_stream_info_t *info = queue->out_stream_infos[i];
        struct timespec *ts = malloc(sizeof(struct timespec));
        clock_gettime(CLOCK_REALTIME, ts);
        if (info->out_stream && (info->changes > 100 || info->last_flush < ts->tv_nsec - 100000000))
        {
            fflush(info->out_stream);
            info->changes = 0;
            if (info->last_flush != 0)
                info->tbf += ts->tv_nsec - info->last_flush;
            info->last_flush = ts->tv_nsec;
            info->flushes++;
        }
    }
}

int close_out_streams(logging_queue_t *queue)
{
    if (!queue)
        return ILLEGAL_ARGS;
    for (int i = 0; i < queue->out_stream_count; i++)
    {
        out_stream_info_t *info = queue->out_stream_infos[i];
        if (info->out_stream && info->out_stream != stdout && info->out_stream != stderr)
            fclose(info->out_stream);
        info->out_stream = NULL;
    }
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

int put_without_escape(const char *buff, FILE *logfile)
{
    if (!logfile)
        return ILLEGAL_ARGS;

    int is_ansi = 0;
    int i = 0;
    char cur;
    while ((cur = buff[i]) != '\0')
    {
        i++;
        if (cur == '\033')
            is_ansi = 1;
        if (!is_ansi)
            fputc(cur, logfile);
        if (cur == 'm')
            is_ansi = 0;
    }

    return 0;
}

int stat_it_up(logging_queue_t *queue)
{
    if (!queue)
        return ILLEGAL_ARGS;

    char message[] = "\n----- LOGGER STATS -----\nTotal Logs: %u\nBuffer Misses: %u\nAvg Buffer Misses: %.2f%%\nAvg Buffer Usage: %.2f%%\nAvg Buffer Usage (chars): %.0f\n";
    char message_per_out_stream[] = "\nOut Stream: %d\nFlushes: %d\nTime Between Flushes: %f seconds\n";
    size_t size = (sizeof(message) + sizeof(message_per_out_stream) * 3 + 1024) * sizeof(char);
    char *stat_buffer = malloc(size);
    double avg_buff_usage = buffer_usage / (double)total_logs;
    int len = snprintf(stat_buffer, size, message, total_logs, buffer_misses, buffer_misses / (double)total_logs * 100, avg_buff_usage * 100, avg_buff_usage * __LOG_BUFFER_SIZE__);
    for (int i = 0; i < queue->out_stream_count; i++)
    {
        out_stream_info_t *info = queue->out_stream_infos[i];
        if (len >= size)
        {
            break;
        }
        len += snprintf(stat_buffer + len, size - len, message_per_out_stream, i, info->flushes, info->tbf / (info->flushes ? info->flushes : 1) / 1000000000.0);
    }
    append_buff_to_queue(queue, LOG_LEVEL_DEBUG, stat_buffer);
    dequeue_log_and_put(queue);
    return 0;
}