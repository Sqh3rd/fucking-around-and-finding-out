#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "logger.h"
#include "thread_pool.h"
#include "task_queue.h"

typedef struct worker_pool_t {
    task_queue_t *task_queue;

    busy_threads_t *busy_threads;
    pthread_cond_t *c_busy_threads;
    pthread_mutex_t *m_busy_threads;

    pthread_t **worker_threads;
    unsigned short worker_count;
} worker_pool_t;

typedef struct thread_pool_t {
    worker_pool_t *worker_pool;
    pthread_t *watcher_thread;
    unsigned short thread_count;
} thread_pool_t;


thread_pool_t *create_thread_pool(unsigned short thread_count, thread_pool_creation_status_t *status);
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
    _destroy_task_queue(pool->worker_pool->task_queue);
    free(pool->worker_pool);

    // Others
    pthread_cancel(*(pool->watcher_thread));
    free(pool->watcher_thread);

    free(pool);
}

typedef struct
{
    int id;
    worker_pool_t *worker_pool;
} worker_thread_entry_arg_t;

void *get_tasks(void *arg)
{
    worker_thread_entry_arg_t *thread_entry_arg = (worker_thread_entry_arg_t *)arg;
    worker_pool_t *worker_pool = thread_entry_arg->worker_pool;
    task_queue_t *task_queue = worker_pool->task_queue;

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
        *worker_pool->busy_threads |= (1 << id);
        pthread_cond_broadcast(worker_pool->c_busy_threads);
        pthread_mutex_unlock(worker_pool->m_busy_threads);

        int err = _dequeue_task(task_queue, &task_func, &task_arg);
        pthread_mutex_unlock(task_queue->m_lock);

        if (err != 0)
        {
            pthread_mutex_unlock(worker_pool->m_busy_threads);
            log_error("Thread %d: Failed to dequeue task: %d\n", id, err);
            break;
        }
        if (task_func && task_arg)
        {
            task_arg->id = id;

            int err = task_func(task_arg);
            if (err)
            {
                log_error("Thread %d: Task function failed with error: %d\n", id, err);
                break;
            }
        }
        pthread_mutex_lock(worker_pool->m_busy_threads);
        *worker_pool->busy_threads ^= (1 << id);
        pthread_cond_broadcast(worker_pool->c_busy_threads);
        pthread_mutex_unlock(worker_pool->m_busy_threads);
    }
    log_info("Thread %d finished\n", id);
}

void *watch_threads(void *arg)
{
    thread_pool_t *pool = (thread_pool_t *)arg;
    worker_pool_t *worker_pool = pool->worker_pool;
    task_queue_t *task_queue = worker_pool->task_queue;

    int busy_threads_count;
    log_info("Watcher Thread started...\n");
    // printf("%s Busy Threads: 0\n%s Open Tasks: \033[s0", DEBUG, DEBUG);
    for (;;)
    {
        pthread_mutex_lock(worker_pool->m_busy_threads);
        while(*worker_pool->busy_threads)
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

        log_info("All tasks completed, stopping task queue...\n");

        int err = _stop_task_queue(task_queue);
        if (err)
        {
            log_info("Failed to stop task queue: %d\n", err);
        }

        pthread_cond_broadcast(task_queue->c_updated);
        pthread_mutex_unlock(task_queue->m_lock);
        break;
    }
    log_info("Waiting for worker threads to finish...\n");
    for (int i = 0; i < worker_pool->worker_count; i++)
    {
        pthread_join(*(worker_pool->worker_threads[i]), NULL);
    }
    _destroy_task_queue(task_queue);
    worker_pool->task_queue = NULL;
    log_info("Watcher Thread exiting...\n");
}

thread_pool_t *create_thread_pool(unsigned short thread_count, thread_pool_creation_status_t *status)
{
    *status = MAX_THREAD_AMOUNT_EXCEEDED;
    if (thread_count <= 0 || thread_count >= sizeof(unsigned int) * 8)
    {
        return NULL;
    }

    // --
    // -- MALLOC
    // --

    *status = OUT_OF_MEMORY;
    task_queue_t *task_queue = _create_task_queue();
    if (!task_queue)
    {
        return NULL;
    }

    pthread_t **worker_threads = malloc(thread_count * sizeof(pthread_t *));
    if (!worker_threads)
    {
        _destroy_task_queue(task_queue);
        return NULL;
    }

    thread_pool_t *pool = malloc(sizeof(thread_pool_t));
    worker_pool_t *worker_pool = malloc(sizeof(worker_pool_t));
    pthread_t *watcher_thread = malloc(sizeof(pthread_t));
    pthread_cond_t *c_busy_threads = malloc(sizeof(pthread_cond_t));
    pthread_mutex_t *m_busy_threads = malloc(sizeof(pthread_mutex_t));
    unsigned int *busy_threads = malloc(sizeof(unsigned int));

    unsigned short all_workers_mallocd = 1;
    for (int i = 0; i < thread_count; i++)
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
        !c_busy_threads ||
        !m_busy_threads ||
        !busy_threads ||
        !all_workers_mallocd)
    {
        _destroy_task_queue(task_queue);
        if (pool)
            free(pool);
        if (worker_pool)
            free(worker_pool);
        if (watcher_thread)
            free(watcher_thread);
        if (c_busy_threads)
            free(c_busy_threads);
        if (m_busy_threads)
            free(m_busy_threads);
        if (busy_threads)
            free(busy_threads);
        for (int i = 0; i < thread_count; i++)
        {
            if (worker_threads[i])
                free(worker_threads[i]);
        }
        return NULL;
    }
    *busy_threads = 0;

    pool->thread_count = thread_count + 2;
    pool->watcher_thread = watcher_thread;
    pool->worker_pool = worker_pool;

    worker_pool->busy_threads = busy_threads;
    worker_pool->c_busy_threads = c_busy_threads;
    worker_pool->m_busy_threads = m_busy_threads;
    worker_pool->task_queue = task_queue;
    worker_pool->worker_count = thread_count;
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

    for (int i = 0; i < thread_count; i++)
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

    log_info("Thread Pool created with %hu threads\n", thread_count);

    return pool;
}

int enqueue_task(thread_pool_t *pool, int(*task_func)(task_queue_entry_arg_t *), task_queue_entry_arg_t *arg) {
    if (!pool || !pool->worker_pool)
        return ILLEGAL_ARGS;
    return _enqueue_task_locked(pool->worker_pool->task_queue, task_func, arg);
}

int join(thread_pool_t *pool) {
    if (!pool || !pool->worker_pool)
        return ILLEGAL_ARGS;
    int result = pthread_join(*(pool->watcher_thread), NULL);
    if (result)
        return result;
    free_pool(pool);
    return 0;
}