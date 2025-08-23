#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "thread_pool.h"
#include "task_queue.h"

thread_pool_t *create_thread_pool(unsigned short thread_count, logging_queue_t *logging_queue, thread_pool_creation_status_t *status);
void free_pool(thread_pool_t *pool);

void free_pool(thread_pool_t *pool)
{
    if (!pool)
    {
        return;
    }

    // Worker Pool
    free(pool->worker_pool->busy_threads);
    free(pool->worker_pool->c_busy_threads);
    free(pool->worker_pool->m_busy_threads);

    for (int i = 0; i < pool->worker_pool->worker_count; i++)
    {
        if (!pool->worker_pool->worker_threads[i])
        {
            continue;
        }
        pthread_cancel(*(pool->worker_pool->worker_threads[i]));
        free(pool->worker_pool->worker_threads[i]);
    }
    free(pool->worker_pool->worker_threads);
    destroy_task_queue(pool->worker_pool->task_queue);
    free(pool->worker_pool);

    // Others
    pthread_cancel(*(pool->watcher_thread));
    free(pool->watcher_thread);

    pthread_cancel(*(pool->logger_thread));
    free(pool->logger_thread);
    destroy_logging_queue(pool->logging_queue);

    free(pool);
}

typedef struct
{
    int id;
    worker_pool_t *worker_pool;
    logging_queue_t *logging_queue;
} worker_thread_entry_arg_t;

void *get_tasks(void *arg)
{
    worker_thread_entry_arg_t *thread_entry_arg = (worker_thread_entry_arg_t *)arg;
    worker_pool_t *worker_pool = thread_entry_arg->worker_pool;
    task_queue_t *task_queue = worker_pool->task_queue;
    logging_queue_t *logging_queue = thread_entry_arg->logging_queue;

    int id = thread_entry_arg->id;

    for (;;)
    {
        pthread_mutex_lock(task_queue->m_lock);
        while (!task_queue->is_stopped && task_queue->count == 0)
        {
            pthread_cond_wait(task_queue->c_updated, task_queue->m_lock);
        }

        if (task_queue->is_stopped)
        {
            pthread_mutex_unlock(task_queue->m_lock);
            break;
        }

        int (*task_func)(task_queue_entry_arg_t *);
        task_queue_entry_arg_t *task_arg;

        pthread_mutex_lock(worker_pool->m_busy_threads);

        int err = dequeue_task(task_queue, &task_func, &task_arg);
        pthread_mutex_unlock(task_queue->m_lock);

        if (err != 0)
        {
            pthread_mutex_unlock(worker_pool->m_busy_threads);
            log_error(logging_queue, "Thread %d: Failed to dequeue task: %d\n", id, err);
            break;
        }
        if (task_func && task_arg)
        {
            *worker_pool->busy_threads |= (1 << id);
            pthread_cond_broadcast(worker_pool->c_busy_threads);
            pthread_mutex_unlock(worker_pool->m_busy_threads);

            task_arg->id = id;

            int err = task_func(task_arg);
            if (err)
            {
                log_error(logging_queue, "Thread %d: Task function failed with error: %d\n", id, err);
                break;
            }

            pthread_mutex_lock(worker_pool->m_busy_threads);
            *worker_pool->busy_threads ^= (1 << id);
            pthread_cond_broadcast(worker_pool->c_busy_threads);
        }
        pthread_mutex_unlock(worker_pool->m_busy_threads);
    }
    log_info(logging_queue, "Thread %d finished\n", id);
}

void *watch_threads(void *arg)
{
    thread_pool_t *pool = (thread_pool_t *)arg;
    logging_queue_t *logging_queue = pool->logging_queue;
    worker_pool_t *worker_pool = pool->worker_pool;
    task_queue_t *task_queue = worker_pool->task_queue;

    int busy_threads_count;
    log_info(logging_queue, "Watcher Thread started...\n");
    // printf("%s Busy Threads: 0\n%s Open Tasks: \033[s0", DEBUG, DEBUG);
    for (;;)
    {
        pthread_mutex_lock(worker_pool->m_busy_threads);
        while(!task_queue->is_started || *worker_pool->busy_threads || task_queue->count)
        {
            pthread_cond_wait(worker_pool->c_busy_threads, worker_pool->m_busy_threads);
        }
        pthread_mutex_unlock(worker_pool->m_busy_threads);

        pthread_mutex_lock(task_queue->m_lock);
        if (!task_queue->is_started || task_queue->count)
        {
            pthread_mutex_unlock(task_queue->m_lock);
            continue;
        }

        log_info(logging_queue, "All tasks completed, stopping task queue...\n");

        int err = stop_task_queue(task_queue);
        if (err)
        {
            log_info(logging_queue, "Failed to stop task queue: %d\n", err);
        }

        pthread_cond_broadcast(task_queue->c_updated);
        pthread_mutex_unlock(task_queue->m_lock);
        break;
    }
    log_info(logging_queue, "Waiting for worker threads to finish...\n");
    for (int i = 0; i < worker_pool->worker_count; i++)
    {
        pthread_join(*(worker_pool->worker_threads[i]), NULL);
    }
    destroy_task_queue(task_queue);
    worker_pool->task_queue = NULL;
    log_info(logging_queue, "Watcher Thread exiting...\n");
}

void *log_messages(void *arg) {
    logging_queue_t *logging_queue = (logging_queue_t *)arg;
    for (;;) {
        pthread_mutex_lock(logging_queue->m_lock);
        while (logging_queue->count == 0) {
            pthread_cond_wait(logging_queue->c_updated, logging_queue->m_lock);
        }
        pthread_mutex_unlock(logging_queue->m_lock);

        logging_queue_entry_t *entry;
        int result;
        while ((result = dequeue_log(logging_queue, &entry)) == 0) {
            printf(entry->buff);
            free(entry->buff);
        }
    }
}

thread_pool_t *create_thread_pool(unsigned short thread_count, logging_queue_t *logging_queue, thread_pool_creation_status_t *status);
{
    *status = MAX_THREAD_AMOUNT_EXCEEDED;
    if (size <= 0 || size >= sizeof(unsigned int) * 8)
    {
        return NULL;
    }

    // --
    // -- MALLOC
    // --

    *status = OUT_OF_MEMORY;
    task_queue_t *task_queue = create_task_queue();
    if (!task_queue)
    {
        return NULL;
    }

    pthread_t **worker_threads = malloc(size * sizeof(pthread_t *));
    if (!worker_threads)
    {
        destroy_task_queue(task_queue);
        destroy_logging_queue(logging_queue);
        return NULL;
    }

    thread_pool_t *pool = malloc(sizeof(thread_pool_t));
    worker_pool_t *worker_pool = malloc(sizeof(worker_pool_t));
    pthread_t *watcher_thread = malloc(sizeof(pthread_t));
    pthread_t *logger_thread = malloc(sizeof(pthread_t));
    pthread_cond_t *c_busy_threads = malloc(sizeof(pthread_cond_t));
    pthread_mutex_t *m_busy_threads = malloc(sizeof(pthread_mutex_t));
    unsigned int *busy_threads = malloc(sizeof(unsigned int));

    unsigned short all_workers_mallocd = 1;
    for (int i = 0; i < size; i++)
    {
        worker_threads[i] = malloc(sizeof(pthread_t));
        if (!worker_threads[i])
        {
            all_workers_mallocd = 0;
            break;
        }
    }

    if (
        !pool ||
        !worker_pool ||
        !watcher_thread ||
        !logger_thread ||
        !c_busy_threads ||
        !m_busy_threads ||
        !busy_threads ||
        !all_workers_mallocd)
    {
        destroy_task_queue(task_queue);
        destroy_logging_queue(logging_queue);
        if (pool)
            free(pool);
        if (worker_pool)
            free(worker_pool);
        if (watcher_thread)
            free(watcher_thread);
        if (logger_thread)
            free(logger_thread);
        if (c_busy_threads)
            free(c_busy_threads);
        if (m_busy_threads)
            free(m_busy_threads);
        if (busy_threads)
            free(busy_threads);
        for (int i = 0; i < size; i++)
        {
            if (worker_threads[i])
                free(worker_threads[i]);
        }
        return NULL;
    }
    *busy_threads = 0;

    pool->logger_thread = logger_thread;
    pool->logging_queue = logging_queue;
    pool->thread_count = size + 2;
    pool->watcher_thread = watcher_thread;
    pool->worker_pool = worker_pool;

    worker_pool->busy_threads = busy_threads;
    worker_pool->c_busy_threads = c_busy_threads;
    worker_pool->m_busy_threads = m_busy_threads;
    worker_pool->task_queue = task_queue;
    worker_pool->worker_count = size;
    worker_pool->worker_threads = worker_threads;

    // --
    // -- CREATE THREADS
    // --
    *status = THREAD_CREATION_FAILED;

    if (
        pthread_cond_init(c_busy_threads, NULL) ||
        pthread_mutex_init(m_busy_threads, NULL))
    {
        free_pool(pool);
        return NULL;
    }

    if (pthread_create(logger_thread, NULL, log_messages, logging_queue))
    {
        free_pool(pool);
        return NULL;
    }

    for (int i = 0; i < size; i++)
    {
        worker_thread_entry_arg_t *thread_entry_arg = malloc(sizeof(worker_thread_entry_arg_t));
        if (!thread_entry_arg)
        {
            *status = OUT_OF_MEMORY;
            free_pool(pool);
            return NULL;
        }
        thread_entry_arg->worker_pool = worker_pool;
        thread_entry_arg->id = i;
        thread_entry_arg->logging_queue = logging_queue;
        if (pthread_create(worker_threads[i], NULL, get_tasks, thread_entry_arg))
        {
            free_pool(pool);
            return NULL;
        }
    }

    if (pthread_create(watcher_thread, NULL, watch_threads, pool))
    {
        free_pool(pool);
        return NULL;
    }

    *status = CREATED;

    log_info(logging_queue, "Thread Pool created with %hu threads\n", size);

    return pool;
}